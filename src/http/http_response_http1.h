/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef HTTP1_RESPONSE_FORMATTER
#define HTTP1_RESPONSE_FORMATTER

// clang-format off
#include "libsock.h"
#include "http_response.h"
// clang-format on

#ifndef UNUSED_FUNC
#define UNUSED_FUNC __attribute__((unused))
#endif

/* *****************************************************************************
Helpers
***************************************************************************** */

/**
The padding for the status line (62 + 2 for the extra \r\n after the headers).
*/
#define H1P_HEADER_START 80
#define H1P_OVERFLOW_PADDING 512

/** checks for overflow */
#define overflowing(response)                                                  \
  (((response)->metadata.headers_pos -                                         \
    (char *)((response)->metadata.packet->buffer)) >=                          \
   (BUFFER_PACKET_SIZE - H1P_OVERFLOW_PADDING))

#define HEADERS_FINISHED(response)                                             \
  ((response)->metadata.packet == NULL || (response)->metadata.headers_sent)

#define invalid_cookie_char(c)                                                 \
  ((c) < '!' || (c) > '~' || (c) == '=' || (c) == ' ' || (c) == ',' ||         \
   (c) == ';')

#define h1p_validate_hpos(response)                                            \
  {                                                                            \
    if ((response)->metadata.headers_pos == 0) {                               \
      (response)->metadata.headers_pos =                                       \
          (response)->metadata.packet->buffer + H1P_HEADER_START;              \
    }                                                                          \
  }
static inline int h1p_protected_copy(http_response_s *response,
                                     const char *data, size_t data_len) {
  if (data_len > 0 && data_len < H1P_OVERFLOW_PADDING) {
    memcpy(response->metadata.headers_pos, data, data_len);
    response->metadata.headers_pos += data_len;
  } else {
    while (*data) {
      if (overflowing(response))
        return -1;
      *(response->metadata.headers_pos++) = *(data++);
    }
  }
  return overflowing(response);
}

/* this function assume the padding in `h1p_protected_copy` saved enough room
 * for the data to be safely written.*/
UNUSED_FUNC static inline sock_packet_s *
h1p_finalize_headers(http_response_s *response) {
  if (HEADERS_FINISHED(response))
    return NULL;
  h1p_validate_hpos(response);

  sock_packet_s *headers = response->metadata.packet;
  response->metadata.packet = NULL;
  const char *status = http_response_status_str(response->status);
  if (!status) {
    response->status = 500;
    status = http_response_status_str(response->status);
  }

  /* write the content length header, unless forced not to (<0) */
  if (response->metadata.content_length_written == 0 &&
      !(response->content_length < 0) && response->status >= 200 &&
      response->status != 204 && response->status != 304) {
    h1p_protected_copy(response, "Content-Length:", 15);
    response->metadata.headers_pos +=
        http_ul2a(response->metadata.headers_pos, response->content_length);
    /* write the header seperator (`\r\n`) */
    *(response->metadata.headers_pos++) = '\r';
    *(response->metadata.headers_pos++) = '\n';
  }
  /* write the date, if missing */
  if (!response->metadata.date_written) {
    if (response->date < response->last_modified)
      response->date = response->last_modified;
    struct tm t;
    /* date header */
    http_gmtime(&response->date, &t);
    h1p_protected_copy(response, "Date:", 5);
    response->metadata.headers_pos +=
        http_date2str(response->metadata.headers_pos, &t);
    *(response->metadata.headers_pos++) = '\r';
    *(response->metadata.headers_pos++) = '\n';
    /* last-modified header */
    http_gmtime(&response->last_modified, &t);
    h1p_protected_copy(response, "Last-Modified:", 14);
    response->metadata.headers_pos +=
        http_date2str(response->metadata.headers_pos, &t);
    *(response->metadata.headers_pos++) = '\r';
    *(response->metadata.headers_pos++) = '\n';
  }
  /* write the keep-alive (connection) header, if missing */
  if (!response->metadata.connection_written) {
    if (response->metadata.should_close) {
      h1p_protected_copy(response, "Connection:close\r\n", 18);
    } else {
      h1p_protected_copy(response, "Connection:keep-alive\r\n"
                                   "Keep-Alive:timeout=2\r\n",
                         45);
    }
  }
  /* write the headers completion marker (empty line - `\r\n`) */
  *(response->metadata.headers_pos++) = '\r';
  *(response->metadata.headers_pos++) = '\n';

  /* write the status string is "HTTP/1.1 xxx <...>\r\n" length == 15 +
   * strlen(status) */

  size_t tmp = strlen(status);
  int start = H1P_HEADER_START - (15 + tmp);
  memcpy(headers->buffer + start, "HTTP/1.1 ### ", 13);
  memcpy(headers->buffer + start + 13, status, tmp);
  ((char *)headers->buffer)[H1P_HEADER_START - 1] = '\n';
  ((char *)headers->buffer)[H1P_HEADER_START - 2] = '\r';
  tmp = response->status / 10;
  *((char *)headers->buffer + start + 11) =
      '0' + (response->status - (10 * tmp));
  *((char *)headers->buffer + start + 10) = '0' + (tmp - (10 * (tmp / 10)));
  *((char *)headers->buffer + start + 9) = '0' + (response->status / 100);

  headers->buffer = (char *)headers->buffer + start;
  headers->length = response->metadata.headers_pos - (char *)headers->buffer;
  return headers;
}

static int h1p_send_headers(http_response_s *response, sock_packet_s *packet) {
  if (packet == NULL)
    return -1;
  /* mark headers as sent */
  response->metadata.headers_sent = 1;
  response->metadata.packet = NULL;
  response->metadata.headers_pos = (char *)packet->length;
  /* write data to network */
  return sock_send_packet(response->metadata.fd, packet);
};

/* *****************************************************************************
Implementation
***************************************************************************** */

UNUSED_FUNC static inline int h1p_response_write_header(http_response_s *response,
                                                     http_headers_s header) {
  if (HEADERS_FINISHED(response) || header.name == NULL)
    return -1;

  h1p_validate_hpos(response);

  if (h1p_protected_copy(response, header.name, header.name_length))
    return -1;
  *(response->metadata.headers_pos++) = ':';
  /*  *(response->metadata.headers_pos++) = ' '; -- better leave out */
  if (header.value != NULL &&
      h1p_protected_copy(response, header.value, header.value_length))
    return -1;
  *(response->metadata.headers_pos++) = '\r';
  *(response->metadata.headers_pos++) = '\n';
  return 0;
}

/**
Set / Delete a cookie using this helper function.
*/
UNUSED_FUNC static int h1p_response_set_cookie(http_response_s *response,
                                            http_cookie_s cookie) {
  if (HEADERS_FINISHED(response) || cookie.name == NULL ||
      overflowing(response))
    return -1; /* must have a cookie name. */
  h1p_validate_hpos(response);

  /* write the header's name to the buffer */
  if (h1p_protected_copy(response, "Set-Cookie:", 11))
    return -1;

  /* we won't use h1p_protected_copy because we'll be testing the name and value
   *  for illegal characters. */

  /* write the cookie name */
  if (cookie.name_len && cookie.name_len < H1P_OVERFLOW_PADDING) {
    for (size_t i = 0; i < cookie.name_len; i++) {
      if (invalid_cookie_char(*cookie.name) == 0) {
        *(response->metadata.headers_pos++) = *(cookie.name++);
        continue;
      } else {
        fprintf(stderr, "Invalid cookie name cookie name character: %c\n",
                *cookie.name);
        return -1;
      }
    }
  } else {
    while (*cookie.name && overflowing(response) == 0) {
      if (invalid_cookie_char(*cookie.name) == 0) {
        *(response->metadata.headers_pos++) = *(cookie.name++);
        continue;
      } else {
        fprintf(stderr, "Invalid cookie name cookie name character: %c\n",
                *cookie.name);
        return -1;
      }
    }
  }
  if (overflowing(response))
    return -1;
  /* seperate name from value */
  *(response->metadata.headers_pos++) = '=';
  /* write the cookie value, if any */
  if (cookie.value) {
    if (cookie.value_len && cookie.value_len < H1P_OVERFLOW_PADDING) {
      for (size_t i = 0; i < cookie.value_len; i++) {
        if (invalid_cookie_char(*cookie.value) == 0) {
          *(response->metadata.headers_pos++) = *(cookie.value++);
          continue;
        } else {
          fprintf(stderr, "Invalid cookie value cookie name character: %c\n",
                  *cookie.value);
          return -1;
        }
      }
    } else {
      while (*cookie.value && overflowing(response) == 0) {
        if (invalid_cookie_char(*cookie.value) == 0) {
          *(response->metadata.headers_pos++) = *(cookie.value++);
          continue;
        } else {
          fprintf(stderr, "Invalid cookie value cookie name character: %c\n",
                  *cookie.value);
          return -1;
        }
      }
    }
    if (overflowing(response))
      return -1;
  } else {
    cookie.max_age = -1;
  }
  /* complete value data */
  *(response->metadata.headers_pos++) = ';';
  if (cookie.max_age) {
    response->metadata.headers_pos +=
        sprintf(response->metadata.headers_pos, "Max-Age=%d;", cookie.max_age);
  }
  if (cookie.domain) {
    memcpy(response->metadata.headers_pos, "domain=", 7);
    response->metadata.headers_pos += 7;
    if (h1p_protected_copy(response, cookie.domain, cookie.domain_len))
      return -1;
    *(response->metadata.headers_pos++) = ';';
  }
  if (cookie.path) {
    memcpy(response->metadata.headers_pos, "path=", 5);
    response->metadata.headers_pos += 5;
    if (h1p_protected_copy(response, cookie.path, cookie.path_len))
      return -1;
    *(response->metadata.headers_pos++) = ';';
  }
  if (cookie.http_only) {
    memcpy(response->metadata.headers_pos, "HttpOnly;", 9);
    response->metadata.headers_pos += 9;
  }
  if (cookie.secure) {
    memcpy(response->metadata.headers_pos, "secure;", 7);
    response->metadata.headers_pos += 7;
  }
  *(response->metadata.headers_pos++) = '\r';
  *(response->metadata.headers_pos++) = '\n';
  return 0;
}

/**
Sends the headers (if unsent) and sends the body.
*/
UNUSED_FUNC static inline int h1p_response_write_body(http_response_s *response,
                                                   const char *body,
                                                   size_t length) {
  if (!response->content_length)
    response->content_length = length;
  sock_packet_s *headers = h1p_finalize_headers(response);
  if (headers != NULL) { /* we haven't sent the headers yet */
    ssize_t i_read =
        ((BUFFER_PACKET_SIZE - H1P_HEADER_START) - headers->length);
    if (i_read > 1024) {
      /* we can fit at least some of the data inside the response buffer. */
      if ((size_t)i_read > length) {
        i_read = length;
        /* we can fit the data inside the response buffer. */
        memcpy(response->metadata.headers_pos, body, i_read);
        response->metadata.headers_pos += i_read;
        headers->length += i_read;
        return h1p_send_headers(response, headers);
      }
      memcpy(response->metadata.headers_pos, body, i_read);
      response->metadata.headers_pos += i_read;
      headers->length += i_read;
      length -= i_read;
      body += i_read;
    }
    /* we need to send the (rest of the) body seperately. */
    if (h1p_send_headers(response, headers))
      return -1;
  }
  response->metadata.headers_pos += length;
  return sock_write(response->metadata.fd, (void *)body, length);
}
/**
Sends the headers (if unsent) and schedules the file to be sent.
*/
UNUSED_FUNC static inline int h1p_response_sendfile(http_response_s *response,
                                                 int source_fd, off_t offset,
                                                 size_t length) {
  if (!response->content_length)
    response->content_length = length;

  sock_packet_s *headers = h1p_finalize_headers(response);

  if (headers != NULL) { /* we haven't sent the headers yet */
    if (headers->length < (BUFFER_PACKET_SIZE - H1P_HEADER_START)) {
      /* we can fit at least some of the data inside the response buffer. */
      ssize_t i_read = pread(
          source_fd, response->metadata.headers_pos,
          ((BUFFER_PACKET_SIZE - H1P_HEADER_START) - headers->length), offset);
      if (i_read > 0) {
        if ((size_t)i_read >= length) {
          headers->length += length;
          close(source_fd);
          return h1p_send_headers(response, headers);
        } else {
          headers->length += i_read;
          length -= i_read;
          offset += i_read;
        }
      }
    }
    /* we need to send the rest seperately. */
    if (h1p_send_headers(response, headers)) {
      close(source_fd);
      return -1;
    }
  }
  response->metadata.headers_pos += length;
  return sock_sendfile(response->metadata.fd, source_fd, offset, length);
}

UNUSED_FUNC static inline int h1p_response_finish(http_response_s *response) {
  sock_packet_s *headers = h1p_finalize_headers(response);
  if (headers) {
    return h1p_send_headers(response, headers);
  }
  if (response->metadata.should_close) {
    sock_close(response->metadata.fd);
  }

  return 0;
}

/* *****************************************************************************
Cleanup
***************************************************************************** */

/* clear any used definitions */
#undef overflowing
#undef HEADERS_FINISHED
#undef invalid_cookie_char
#undef h1p_validate_hpos
#endif
