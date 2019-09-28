/*
Copyright: Boaz Segev, 2016-2019
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include <http.h>

#include <http1.h>
#include <http_internal.h>

#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* *****************************************************************************
External functions
***************************************************************************** */

// #define HTTP_BUSY_UNLESS_HAS_FDS 64
// #define HTTP_DEFAULT_BODY_LIMIT (1024 * 1024 * 50)
// #define HTTP_MAX_HEADER_COUNT 128
// #define HTTP_MAX_HEADER_LENGTH 8192
// #define FIO_HTTP_EXACT_LOGGING 0

#include <http1.h>

/* *****************************************************************************
Small Helpers
***************************************************************************** */

static inline void add_content_length(http_internal_s *h, uintptr_t length) {
  set_header_if_missing(h->headers_out, HTTP_HEADER_CONTENT_LENGTH,
                        fiobj_num_new(length));
}
static inline void add_content_type(http_internal_s *h) {
  uint64_t hash = fiobj2hash(h->headers_out, HTTP_HEADER_CONTENT_TYPE);
  if (!fiobj_hash_get(h->headers_out, hash, HTTP_HEADER_CONTENT_TYPE)) {
    fiobj_hash_set(h->headers_out, hash, HTTP_HEADER_CONTENT_TYPE,
                   http_mimetype_find2(h->public.path), NULL);
  }
}
static inline FIOBJ get_date___(void) {
  static FIOBJ_STR_TEMP_VAR(date);
  static char date_buf[48];
  static size_t date_len;
  static time_t last_date_added;
  if (fio_last_tick().tv_sec > last_date_added) {
    const time_t now = fio_last_tick().tv_sec;
    struct tm tm;
    http_gmtime(now, &tm);
    date_len = http_date2rfc7231(date_buf, &tm);
    fio_str_info_s i = fiobj_str_resize(date, date_len);
    memcpy(i.buf, date_buf, date_len);
  }
  return date;
}
static inline void add_date(http_internal_s *h) {
  if (fiobj_hash_get2(h->headers_out, HTTP_HEADER_DATE) &&
      fiobj_hash_get2(h->headers_out, HTTP_HEADER_LAST_MODIFIED))
    return;
  FIOBJ date = get_date___(); /* no fiobj_dup, since it's a static tmp var. */
  set_header_if_missing(h->headers_out, HTTP_HEADER_DATE, date);
  set_header_if_missing(h->headers_out, HTTP_HEADER_LAST_MODIFIED, date);
}

static inline int hex2byte(uint8_t *dest, const uint8_t *source) {
  if (source[0] >= '0' && source[0] <= '9')
    *dest = (source[0] - '0');
  else if ((source[0] >= 'a' && source[0] <= 'f') ||
           (source[0] >= 'A' && source[0] <= 'F'))
    *dest = (source[0] | 32) - ('a' - 10);
  else
    return -1;
  *dest <<= 4;
  if (source[1] >= '0' && source[1] <= '9')
    *dest |= (source[1] - '0');
  else if ((source[1] >= 'a' && source[1] <= 'f') ||
           (source[1] >= 'A' && source[1] <= 'F'))
    *dest |= (source[1] | 32) - ('a' - 10);
  else
    return -1;
  return 0;
}

/* *****************************************************************************
The HTTP hanndler (Request / Response) functions
***************************************************************************** */

/**
 * Sets a response header, taking ownership of the value object, but NOT the
 * name object (so name objects could be reused in future responses).
 *
 * Returns -1 on error and 0 on success.
 */
int http_set_header(http_s *h_, FIOBJ name, FIOBJ value) {
  http_internal_s *h = HTTP2PRIVATE(h_);
  if (!h_ || !name || !FIOBJ_TYPE_IS(h->headers_out, FIOBJ_T_HASH))
    goto error;
  set_header_add(h->headers_out, name, value);
  return 0;
error:
  fiobj_free(value);
  return -1;
}

/**
 * Sets a response header.
 *
 * Returns -1 on error and 0 on success.
 */
int http_set_header2(http_s *h_, fio_str_info_s name, fio_str_info_s value) {
  http_internal_s *h = HTTP2PRIVATE(h_);
  if (!h_ || !FIOBJ_TYPE_IS(h->headers_out, FIOBJ_T_HASH) || !name.buf)
    return -1;
  FIOBJ k = fiobj_str_new_cstr(name.buf, name.len);
  FIOBJ v = FIOBJ_INVALID;
  if (value.len)
    v = fiobj_str_new_cstr(value.buf, value.len);
  set_header_add(h->headers_out, k, v);
  fiobj_free(k);
  return 0;
}

void http_set_cookie___(void); /* SublimeText marker */
/**
 * Sets a response cookie.
 *
 * Returns -1 on error and 0 on success.
 *
 * Note: Long cookie names and long cookie values will be considered a security
 * violation and an error will be returned. It should be noted that most
 * proxies and servers will refuse long cookie names or values and many impose
 * total header lengths (including cookies) of ~8Kib.
 */
int http_set_cookie FIO_NOOP(http_s *h_, http_cookie_args_s cookie) {
  /* promises that some warnings print only once. */
  static int warn_illegal = 0;
  /* valid / invalid characters in cookies, create with Ruby using:

      a = []
      256.times {|i| a[i] = 1;}
      ('a'.ord..'z'.ord).each {|i| a[i] = 0;}
      ('A'.ord..'Z'.ord).each {|i| a[i] = 0;}
      ('0'.ord..'9'.ord).each {|i| a[i] = 0;}
      "!#$%&'*+-.^_`|~".bytes.each {|i| a[i] = 0;}
      p a; nil
      "!#$%&'()*+-./:<=>?@[]^_`{|}~".bytes.each {|i| a[i] = 0;} # for values
      p a; nil
  */
  static const char invalid_cookie_name_char[256] = {
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  static const char invalid_cookie_value_char[256] = {
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  static const char hex_chars[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                   '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

  http_internal_s *h = HTTP2PRIVATE(h_);
#if DEBUG
  FIO_ASSERT(h_, "Can't set cookie for NULL HTTP handler!");
#endif
  if (HTTP_S_INVALID(h_) || FIOBJ_TYPE_CLASS(h->headers_out) != FIOBJ_T_HASH ||
      cookie.name_len >= 32768 || cookie.value_len >= 131072) {
    return -1;
  }
  /* write name and value while auto-correcting encoding issues */
  size_t capa = cookie.name_len + cookie.value_len + 128;
  size_t len = 0;
  FIOBJ c = fiobj_str_new_buf(capa);
  fio_str_info_s t = fiobj_str_info(c);

#define copy_cookie_ch(ch_var)                                                 \
  if (invalid_cookie_##ch_var##_char[(uint8_t)cookie.ch_var[tmp]]) {           \
    if (!warn_illegal) {                                                       \
      ++warn_illegal;                                                          \
      FIO_LOG_WARNING("illegal char 0x%.2x in cookie " #ch_var " (in %s)\n"    \
                      "         automatic %% encoding applied",                \
                      cookie.ch_var[tmp], cookie.ch_var);                      \
    }                                                                          \
    t.buf[len++] = '%';                                                        \
    t.buf[len++] = hex_chars[((uint8_t)cookie.ch_var[tmp] >> 4) & 0x0F];       \
    t.buf[len++] = hex_chars[(uint8_t)cookie.ch_var[tmp] & 0x0F];              \
  } else {                                                                     \
    t.buf[len++] = cookie.ch_var[tmp];                                         \
  }                                                                            \
  tmp += 1;                                                                    \
  if (capa <= len + 3) {                                                       \
    capa += 32;                                                                \
    fiobj_str_reserve(c, capa);                                                \
    t = fiobj_str_info(c);                                                     \
  }

  if (cookie.name) {
    size_t tmp = 0;
    if (cookie.name_len) {
      while (tmp < cookie.name_len) {
        copy_cookie_ch(name);
      }
    } else {
      while (cookie.name[tmp]) {
        copy_cookie_ch(name);
      }
    }
  }
  t.buf[len++] = '=';
  if (cookie.value) {
    size_t tmp = 0;
    if (cookie.value_len) {
      while (tmp < cookie.value_len) {
        copy_cookie_ch(value);
      }
    } else {
      while (cookie.value[tmp]) {
        copy_cookie_ch(value);
      }
    }
  } else
    cookie.max_age = -1;

  /* client cookie headers are simpler */
  if (http_settings(h_) && http_settings(h_)->is_client) {
    if (!cookie.value) {
      fiobj_free(c);
      return -1;
    }
    set_header_add(h->headers_out, HTTP_HEADER_COOKIE, c);
    return 0;
  }
  /* server cookie data */
  t.buf[len++] = ';';
  t.buf[len++] = ' ';
  t = fiobj_str_resize(c, len);

  if (cookie.max_age) {
    t = fiobj_str_reserve(c, t.len + 40);

    fiobj_str_write(c, "Max-Age=", 8);
    fiobj_str_write_i(c, cookie.max_age);
    fiobj_str_write(c, "; ", 2);
  }

  if (cookie.domain && cookie.domain_len) {
    t = fiobj_str_reserve(c, t.len + 7 + 1 + cookie.domain_len);
    fiobj_str_write(c, "domain=", 7);
    fiobj_str_write(c, cookie.domain, cookie.domain_len);
    fiobj_str_write(c, ";", 1);
    t.buf[len++] = ' ';
  }
  if (cookie.path && cookie.path_len) {
    t = fiobj_str_reserve(c, t.len + 5 + 1 + cookie.path_len);
    fiobj_str_write(c, "path=", 5);
    fiobj_str_write(c, cookie.path, cookie.path_len);
    fiobj_str_write(c, ";", 1);
    t.buf[len++] = ' ';
  }
  if (cookie.http_only) {
    fiobj_str_write(c, "HttpOnly;", 9);
  }
  if (cookie.secure) {
    fiobj_str_write(c, "secure;", 7);
  }
  set_header_add(h->headers_out, HTTP_HEADER_SET_COOKIE, c);
  return 0;
#undef copy_cookie_ch
}

/**
 * Sends the response headers and body.
 *
 * **Note**: The body is *copied* to the HTTP stream and it's memory should be
 * freed by the calling function.
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
int http_send_body(http_s *h_, void *data, uintptr_t length) {
  http_internal_s *h = HTTP2PRIVATE(h_);
  if (HTTP_S_INVALID(h_))
    return -1;
  if (!length || !data) {
    http_finish(h_);
    return 0;
  }
  add_content_length(h, length);
  add_content_type(h);
  add_date(h);
  return h->vtbl->send_body(h, data, length);
}

/**
 * Sends the response headers and the specified file (the response's body).
 *
 * The file is closed automatically.
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
int http_sendfile(http_s *h_, int fd, uintptr_t offset, uintptr_t length) {
  http_internal_s *h = HTTP2PRIVATE(h_);
  if (HTTP_S_INVALID(h_))
    goto handle_invalid;
  if (!length || fd == -1)
    goto input_error;
  if (!length) {
    FIO_LOG_WARNING("(HTTP) http_sendfile length missing\n");
    struct stat s;
    if (-1 == fstat(fd, &s) || (uintptr_t)s.st_size < offset)
      goto input_error;
    length = s.st_size - offset;
  }
  add_content_length(h, length);
  add_content_type(h);
  add_date(h);
  return h->vtbl->sendfile(h, fd, offset, length);
input_error:
  http_finish(h_);
  if (fd >= 0)
    close(fd);
  return 0;
handle_invalid:
  if (fd >= 0)
    close(fd);
  return -1;
}

/**
 * Sends an HTTP error response.
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 *
 * The `uuid` and `settings` arguments are only required if the `http_s` handle
 * is NULL.
 */
int http_send_error(http_s *h_, size_t error) {
  http_internal_s *h = HTTP2PRIVATE(h_);
  if (HTTP_S_INVALID(h_))
    return -1;

  if (error < 100 || error >= 1000)
    error = 500;
  h_->status = error;
  char buffer[16];
  buffer[0] = '/';
  size_t pos = 1 + fio_ltoa(buffer + 1, error, 10);
  buffer[pos++] = '.';
  buffer[pos++] = 'h';
  buffer[pos++] = 't';
  buffer[pos++] = 'm';
  buffer[pos++] = 'l';
  buffer[pos] = 0;
  if (http_sendfile2(h_, h->pr->settings->public_folder,
                     h->pr->settings->public_folder_length, buffer, pos)) {
    FIOBJ mime_type = http_mimetype_find((char *)"html", 3);
    if (mime_type) {
      http_set_header(h_, HTTP_HEADER_CONTENT_TYPE,
                      http_mimetype_find((char *)"html", 3));
    }
    fio_str_info_s t = http_status2str(error);
    http_send_body(h_, t.buf, t.len);
  }
  return 0;
}

/**
 * Sends the response headers for a header only response.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
void http_finish(http_s *h_) {
  http_internal_s *h = HTTP2PRIVATE(h_);
  if (HTTP_S_INVALID(h_)) {
    return;
  }
  if (FIOBJ_TYPE_IS(h->headers_out, FIOBJ_T_HASH)) {
    add_content_length(h, 0);
    add_date(h);
  }
  h->vtbl->finish(h);
}

/**
 * Sends the response headers (if not sent) and streams the data.
 *
 * **Note**: The body is *copied* to the HTTP stream and it's memory should be
 * freed by the calling function.
 *
 * Returns -1 on error and 0 on success.
 *
 * The `http_s` object remsains valid. Remember to call `http_finish`.
 */
int http_stream(http_s *h_, void *data, uintptr_t length) {
  http_internal_s *h = HTTP2PRIVATE(h_);
  if (HTTP_S_INVALID(h_))
    return -1;
  if (FIOBJ_TYPE_IS(h->headers_out,
                    FIOBJ_T_HASH)) /* may be altered if already sent */
    add_date(h);
  return h->vtbl->stream(h, data, length);
}

/**
 * Pushes a data response when supported (HTTP/2 only).
 *
 * `mime_type` will be automatically freed by the `push` function.
 *
 * Returns -1 on error and 0 on success.
 */
int http_push_data(http_s *h_, void *data, uintptr_t length, FIOBJ mime_type) {
  http_internal_s *h = HTTP2PRIVATE(h_);
  if (HTTP_S_INVALID(h_))
    return -1;
  return h->vtbl->push_data(h, data, length, mime_type);
}

/**
 * Pushes a file response when supported (HTTP/2 only).
 *
 * If `mime_type` is NULL, an attempt at automatic detection using `filename`
 * will be made.
 *
 * `filename` and `mime_type` will be automatically freed by the `push`
 * function.
 *
 * Returns -1 on error and 0 on success.
 */
int http_push_file(http_s *h_, FIOBJ filename, FIOBJ mime_type) {
  http_internal_s *h = HTTP2PRIVATE(h_);
  if (HTTP_S_INVALID(h_))
    return -1;
  return h->vtbl->push_file(h, filename, mime_type);
}

/* *****************************************************************************
HTTP `sendfile2` - Sending a File by Name
***************************************************************************** */

/* internal helper - tests for a file and prepers response */
static int http_sendfile___test_filename(http_s *h, fio_str_info_s filename,
                                         fio_str_info_s enc) {
  // FIO_LOG_DEBUG2("(HTTP) sendfile testing: %s", filename.buf);
  http_internal_s *hpriv = HTTP2PRIVATE(h);
  struct stat file_data = {.st_size = 0};
  int file = -1;
  if (stat(filename.buf, &file_data) ||
      (!S_ISREG(file_data.st_mode) && !S_ISLNK(file_data.st_mode)))
    return -1;
  /*** file name Okay, handle request ***/
  // FIO_LOG_DEBUG2("(HTTP) sendfile found: %s", filename.buf);
  if (hpriv->pr->settings->static_headers) { /* copy default headers */
    FIOBJ defs = hpriv->pr->settings->static_headers;
    FIO_MAP_EACH(((fiobj_hash_s *)FIOBJ_PTR_UNTAG(defs)), pos) {
      set_header_if_missing(hpriv->headers_out, pos->obj.key,
                            fiobj_dup(pos->obj.value));
    }
  }
  {
    /* set last-modified */
    FIOBJ tmp = fiobj_str_new_buf(42);
    struct tm tm;
    http_gmtime(file_data.st_mtime, &tm);
    fiobj_str_resize(tmp, http_date2str(fiobj_str2ptr(tmp), &tm));
    set_header_if_missing(hpriv->headers_out, HTTP_HEADER_LAST_MODIFIED, tmp);
  }
  /* set cache-control */
  set_header_if_missing(hpriv->headers_out, HTTP_HEADER_CACHE_CONTROL,
                        fiobj_dup(HTTP_HVALUE_MAX_AGE));
  FIOBJ etag_str = FIOBJ_INVALID;
  {
    /* set & test etag */
    uint64_t etag = (uint64_t)file_data.st_size;
    etag ^= (uint64_t)file_data.st_mtime;
    etag = fio_risky_hash(&etag, sizeof(uint64_t), 0);
    etag_str = fiobj_str_new();
    fiobj_str_write_base64enc(etag_str, (void *)&etag, sizeof(uint64_t), 0);
    set_header_if_missing(hpriv->headers_out, HTTP_HEADER_ETAG, etag_str);
    /* test for if-none-match */
    FIOBJ tmp = fiobj_hash_get2(h->headers, HTTP_HEADER_IF_NONE_MATCH);
    if (tmp != FIOBJ_INVALID && fiobj_is_eq(tmp, etag_str)) {
      h->status = 304;
      http_finish(h);
      return 0;
    }
  }
  /* handle range requests */
  int64_t offset = 0;
  int64_t length = file_data.st_size;
  {
    FIOBJ tmp = fiobj_hash_get2(h->headers, HTTP_HEADER_IF_RANGE);
    /* TODO: support date/time in HTTP_HEADER_IF_RANGE? */
    if (tmp && !fiobj_is_eq(tmp, etag_str)) {
      fiobj_hash_remove2(h->headers, HTTP_HEADER_RANGE, NULL);
    } else {
      tmp = fiobj_hash_get2(h->headers, HTTP_HEADER_RANGE);
      if (tmp) {
        /* range ahead... */
        if (FIOBJ_TYPE_IS(tmp, FIOBJ_T_ARRAY))
          tmp = fiobj_array_get(tmp, 0);
        fio_str_info_s range = fiobj2cstr(tmp);
        if (!range.buf || memcmp("bytes=", range.buf, 6))
          goto open_file;
        char *pos = range.buf + 6;
        int64_t start_at = 0, end_at = 0;
        start_at = fio_atol(&pos);
        if (start_at >= file_data.st_size)
          goto open_file;
        if (start_at >= 0) {
          if (*pos)
            pos++;
          end_at = fio_atol(&pos);
        }
        /* we ignore multimple ranges, only responding with the first range. */
        if (start_at < 0) {
          if (0 - start_at < file_data.st_size) {
            offset = file_data.st_size - start_at;
            length = 0 - start_at;
          }
        } else if (end_at) {
          offset = start_at;
          length = end_at - start_at + 1;
          if (length + start_at > file_data.st_size || length <= 0)
            length = length - start_at;
        } else {
          offset = start_at;
          length = length - start_at;
        }
        h->status = 206;

        {
          FIOBJ cranges = fiobj_str_new();
          fiobj_str_printf(cranges, "bytes %lu-%lu/%lu",
                           (unsigned long)start_at,
                           (unsigned long)(start_at + length - 1),
                           (unsigned long)file_data.st_size);
          set_header_overwite(hpriv->headers_out, HTTP_HEADER_CONTENT_RANGE,
                              cranges);
        }
        set_header_overwite(hpriv->headers_out, HTTP_HEADER_ACCEPT_RANGES,
                            fiobj_dup(HTTP_HVALUE_BYTES));
      }
    }
  }
  /* test for an OPTIONS request or invalid methods */
  {
    fio_str_info_s s = fiobj2cstr(h->method);
    switch (s.len) {
    case 7:
      if (!strncasecmp("options", s.buf, 7)) {
        http_set_header2(
            h, (fio_str_info_s){.buf = (char *)"allow", .len = 5},
            (fio_str_info_s){.buf = (char *)"GET, HEAD", .len = 9});
        h->status = 200;
        http_finish(h);
        return 0;
      }
      break;
    case 3:
      if (!strncasecmp("get", s.buf, 3))
        goto open_file;
      break;
    case 4:
      if (!strncasecmp("head", s.buf, 4)) {
        set_header_overwite(hpriv->headers_out, HTTP_HEADER_CONTENT_LENGTH,
                            fiobj_num_new(length));
        http_finish(h);
        return 0;
      }
      break;
    }
    http_send_error(h, 403);
  }
  return 0;

open_file:

  file = open(filename.buf, O_RDONLY);
  if (file == -1) {
    FIO_LOG_ERROR("(HTTP) couldn't open file %s!\n", filename.buf);
    perror("     ");
    http_send_error(h, 500);
    return 0;
  }
  {
    FIOBJ mime = FIOBJ_INVALID;
    if (enc.buf) {
      set_header_overwite(hpriv->headers_out, HTTP_HEADER_CONTENT_ENCODING,
                          fiobj_str_new_cstr(enc.buf, enc.len));
      size_t pos = filename.len - 4;
      while (pos && filename.buf[pos] != '.')
        pos--;
      pos++; /* assuming, but that's fine. */
      mime = http_mimetype_find(filename.buf + pos, filename.len - pos - 3);

    } else {
      size_t pos = filename.len - 1;
      while (pos && filename.buf[pos] != '.')
        pos--;
      pos++; /* assuming, but that's fine. */
      mime = http_mimetype_find(filename.buf + pos, filename.len - pos);
    }
    if (mime)
      set_header_if_missing(hpriv->headers_out, HTTP_HEADER_CONTENT_TYPE, mime);
  }
  return http_sendfile(h, file, offset, length);
}

static inline int http_test_encoded_path(const char *mem, size_t len) {
  const char *pos = NULL;
  const char *end = mem + len;
  while (mem < end && (pos = memchr(mem, '/', (size_t)len))) {
    len = end - pos;
    mem = pos + 1;
    if (len >= 1 && pos[1] == '/')
      return -1;
    if (len > 3 && pos[1] == '.' && pos[2] == '.' && pos[4] == '/')
      return -1;
  }
  return 0;
}

/**
 * Sends the response headers and the specified file (the response's body).
 *
 * The `local` and `encoded` strings will be joined into a single string that
 * represent the file name. Either or both of these strings can be empty.
 *
 * The `encoded` string will be URL decoded while the `local` string will used
 * as is.
 *
 * Returns 0 on success. A success value WILL CONSUME the `http_s` handle (it
 * will become invalid).
 *
 * Returns -1 on error (The `http_s` handle should still be used).
 */
int http_sendfile2(http_s *h_, const char *prefix, size_t prefix_len,
                   const char *encoded, size_t encoded_len) {
  if (HTTP_S_INVALID(h_))
    return -1;
  FIOBJ_STR_TEMP_VAR(fn);
  /* stack allocated buffer for filename data */
  char buffer[2048];
  fn___tmp[1] = (fiobj_str_s)FIO_STRING_INIT_EXISTING(buffer, 0, 2048, NULL);
  /* copy file name, protect against root (//) and tree traversal (..) */
  if (prefix && prefix_len) {
    fiobj_str_write(fn, prefix, prefix_len);
    if (prefix[prefix_len] - 1 == '/' && encoded) {
      if (encoded_len && encoded[0] == '/') {
        ++encoded;
        --encoded_len;
      }
    }
  }
  if (encoded) {
    char tmp[128];
    uint8_t len = 0;
    while (encoded_len--) {
      if (*encoded == '%' && encoded_len >= 2) {
        if (hex2byte((uint8_t *)(tmp + len), (const uint8_t *)(encoded + 1)))
          goto path_error;
        ++len;
        encoded += 3;
        encoded_len -= 2;
      } else {
        tmp[len++] = *encoded;
        ++encoded;
      }
      if (len >= 125) {
        /* minimize calls to `write`, because the test memory capacity */
        fiobj_str_write(fn, tmp, len);
        len = 0;
      }
    }
    if (len) {
      fiobj_str_write(fn, tmp, len);
    }
    fio_str_info_s fn_inf = fiobj_str2cstr(fn);
    if (http_test_encoded_path(fn_inf.buf + prefix_len,
                               fn_inf.len - prefix_len))
      goto path_error;
  }

  /* raw file name is now stored at `fn`, we need to test for variations */
  fio_str_info_s ext[] = {/* holds default changes */
                          {.buf = "", .len = 0},
                          {.buf = ".html", .len = 5},
                          {.buf = "/index.html", .len = 11},
                          {.buf = NULL}};

  /* store original length and test for folder (index) */
  size_t org_len = fiobj_str_len(fn);
  {
    char *ptr = fiobj_str2ptr(fn);
    if (fiobj_str2ptr(fn)[org_len - 1] == '/') {
      org_len = fiobj_str_write(fn, "index.html", 10).len;
      ext[1] = (fio_str_info_s){.buf = NULL};
    } else {
      /* avoid extension & folder testing for files with extensions */
      size_t i = org_len - 1;
      while (i && ptr[i] != '.' && ptr[i] != '/')
        --i;
      if (ptr[i] == '.')
        ext[1] = (fio_str_info_s){.buf = NULL};
    }
  }

  fio_str_info_s enc_src[7]; /* holds default tests + accept-encoding headers */
  fio_str_info_s enc_ext[7]; /* holds default tests + accept-encoding headers */
  size_t enc_count = 0;
  {
    /* add any supported encoding options, such as gzip, deflate, br, etc' */
    FIOBJ encodeings =
        fiobj_hash_get2(h_->headers, HTTP_HEADER_ACCEPT_ENCODING);
    if (encodeings) {
      fio_str_info_s i = fiobj2cstr(encodeings);
      while (i.len && enc_count < 7) {
        while (i.len && (*i.buf == ',' || *i.buf == ' ')) {
          ++i.buf;
          --i.len;
        }
        if (!i.len)
          break;
        size_t end = 0;
        while (end < i.len && i.buf[end] != ',' && i.buf[end] != ' ')
          ++end;
        if (end && end < 64) { /* avoid malicious encoding information */
          if (end == 4 && !memcmp(i.buf, "gzip", 4)) {
            enc_ext[enc_count] = (fio_str_info_s){.buf = "gz", .len = 2};
            enc_src[enc_count++] = (fio_str_info_s){.buf = i.buf, .len = end};
          } else if (end == 7 && !memcmp(i.buf, "deflate", 7)) {
            enc_ext[enc_count] = (fio_str_info_s){.buf = "zz", .len = 2};
            enc_src[enc_count++] = (fio_str_info_s){.buf = i.buf, .len = end};
          } else { /* passthrough / unknown variations */
            enc_ext[enc_count] = (fio_str_info_s){.buf = i.buf, .len = end};
            enc_src[enc_count++] = (fio_str_info_s){.buf = i.buf, .len = end};
          }
        }
        i.len -= end;
        i.buf += end;
      }
    }
  }
  size_t pos = 0;
  do {
    /* add extension */
    fio_str_info_s n = fiobj_str2cstr(fn);
    if (ext[pos].len) {
      if (n.len > ext[pos].len &&
          !memcmp(n.buf - ext[pos].len, ext[pos].buf, ext[pos].len)) {
        ++pos;
        continue;
      }
      n = fiobj_str_write(fn, ext[pos].buf, ext[pos].len);
    }
    size_t ext_len = n.len;
    /* test each supported encoding option */
    for (size_t i = 0; i < enc_count; ++i) {
      fiobj_str_write(fn, ".", 1);
      n = fiobj_str_write(fn, enc_ext[i].buf, enc_ext[i].len);
      /* test filename */
      if (!http_sendfile___test_filename(h_, n, enc_src[i]))
        goto found_file;
      n = fiobj_str_resize(fn, ext_len);
    }
    /* test filename (without encoding) */
    if (!http_sendfile___test_filename(h_, fiobj_str2cstr(fn),
                                       (fio_str_info_s){.buf = NULL}))
      goto found_file;
    /* revert file name to original value */
    fiobj_str_resize(fn, org_len);
    ++pos;
  } while (ext[pos].buf);

  return -1;

found_file:
  FIOBJ_STR_TEMP_DESTROY(fn);
  return 0;

path_error:
  FIOBJ_STR_TEMP_DESTROY(fn);
  return -1;
}

/* test names in the following order:
 * - name.encoding
 * - name
 * - name.html.encoding
 * - name.html
 */

/* *****************************************************************************
HTTP evented API (pause / resume HTTp handling)
***************************************************************************** */
struct http_pause_handle_s {
  uintptr_t uuid;
  http_internal_s *h;
  http_fio_protocol_s *pr;
  void *udata;
  void (*task)(http_s *);
  void (*fallback)(void *);
};
typedef struct http_pause_handle_s http_pause_handle_s;

/* perform the pause task outside of the connection's lock */
static void http_pause_wrapper(void *h_, void *task_) {
  void (*task)(void *h) = (void (*)(void *h))((uintptr_t)task_);
  task(h_);
}

/* perform the resume task fallback */
static void http_resume_fallback_wrapper(intptr_t uuid, void *arg) {
  http_pause_handle_s *http = arg;
  if (http->fallback)
    http->fallback(http->udata);
  fio_free(http);
  (void)uuid;
}

/* perform the resume task within of the connection's lock */
static void http_resume_wrapper(intptr_t uuid, fio_protocol_s *p_, void *arg) {
  http_fio_protocol_s *p = (http_fio_protocol_s *)p_;
  http_pause_handle_s *http = arg;
  if (p != http->pr)
    goto protocol_invalid;
  http_internal_s *h = http->h;
  h->public.udata = http->udata;
  http_vtable_s *vtbl = h->vtbl;
  if (http->task)
    http->task(HTTP2PUBLIC(h));
  vtbl->on_resume(h, p);
  fio_free(http);
  (void)uuid;
  return;
protocol_invalid:
  http_resume_fallback_wrapper(uuid, arg);
  return;
}

/**
 * Pauses the request / response handling and INVALIDATES the current `http_s`
 * handle (no `http` functions should be called).
 *
 * The `http_resume` function MUST be called (at some point) using the opaque
 * `http` pointer given to the callback `task`.
 *
 * The opaque `http` pointer is only valid for a single call to `http_resume`
 * and can't be used by any other `http` function (it's a different data type)
 * except the `http_paused_udata_get` and `http_paused_udata_set` functions.
 *
 * Note: the current `http_s` handle will become invalid once this function is
 *    called and it's data might be deallocated, invalidated or used by a
 * different thread.
 */
void http_pause(http_s *h_, void (*task)(http_pause_handle_s *http)) {
  http_internal_s *h = HTTP2PRIVATE(h_);
  if (HTTP_S_INVALID(h_))
    return;

  http_fio_protocol_s *p = h->pr;
  http_pause_handle_s *http = fio_malloc(sizeof(*http));
  *http = (http_pause_handle_s){
      .pr = h->pr,
      .uuid = p->uuid,
      .h = h,
      .udata = h->public.udata,
  };
  h->vtbl->on_pause(h, p);
  fio_defer(http_pause_wrapper, http, (void *)((uintptr_t)task));
}

/**
 * Resumes a request / response handling within a task and INVALIDATES the
 * current `http_s` handle.
 *
 * The `task` MUST call one of the `http_send_*`, `http_finish`, or
 * `http_pause`functions.
 *
 * The (optional) `fallback` will receive the opaque `udata` that was stored in
 * the HTTP handle and can be used for cleanup.
 *
 * Note: `http_resume` can only be called after calling `http_pause` and
 * entering it's task.
 *
 * Note: the current `http_s` handle will become invalid once this function is
 *    called and it's data might be deallocated, invalidated or used by a
 *    different thread.
 */
void http_resume(http_pause_handle_s *http, void (*task)(http_s *h),
                 void (*fallback)(void *udata)) {
  if (!http)
    return;
  http->task = task;
  http->fallback = fallback;
  fio_defer_io_task(http->uuid, .udata = http, .type = FIO_PR_LOCK_TASK,
                    .task = http_resume_wrapper,
                    .fallback = http_resume_fallback_wrapper);
}

/** Returns the `udata` associated with the paused opaque handle */
void *http_paused_udata_get(http_pause_handle_s *http) { return http->udata; }

/**
 * Sets the `udata` associated with the paused opaque handle, returning the
 * old value.
 */
void *http_paused_udata_set(http_pause_handle_s *http, void *udata) {
  void *old = http->udata;
  http->udata = udata;
  return old;
}

/* *****************************************************************************
HTTP Settings Management
***************************************************************************** */

static void http_on_request_fallback(http_s *h) { http_send_error(h, 404); }
static void http_on_response_fallback(http_s *h) { http_send_error(h, 400); }
static void http_on_upgrade_fallback(http_s *h, char *p, size_t i) {
  http_send_error(h, 400);
  (void)p;
  (void)i;
}

static http_settings_s *http_settings_new(http_settings_s s) {
  /* set defaults */
  if (!s.on_request)
    s.on_request = http_on_request_fallback;
  if (!s.on_response)
    s.on_response = http_on_response_fallback;
  if (!s.on_upgrade)
    s.on_upgrade = http_on_upgrade_fallback;
  if (!s.max_body_size)
    s.max_body_size = HTTP_DEFAULT_BODY_LIMIT;
  if (!s.timeout)
    s.timeout = 40;
  if (!s.ws_max_msg_size)
    s.ws_max_msg_size = 262144; /** defaults to ~250KB */
  if (!s.ws_timeout)
    s.ws_timeout = 40; /* defaults to 40 seconds */
  if (!s.max_header_size)
    s.max_header_size = 32 * 1024; /* defaults to 32Kib */
  if (s.max_clients <= 0 ||
      (size_t)(s.max_clients + HTTP_BUSY_UNLESS_HAS_FDS) > fio_capa()) {
    s.max_clients = fio_capa();
    if ((ssize_t)s.max_clients - HTTP_BUSY_UNLESS_HAS_FDS > 0)
      s.max_clients -= HTTP_BUSY_UNLESS_HAS_FDS;
    else if (s.max_clients > 1)
      --s.max_clients;
  }
  if (!s.public_folder_length && s.public_folder) {
    s.public_folder_length = strlen(s.public_folder);
  }
  if (!s.public_folder_length)
    s.public_folder = NULL;

  /* test for "~/" path prefix (user home) */
  const char *home = NULL;
  size_t home_len = 0;
  if (s.public_folder && s.public_folder[0] == '~' &&
      s.public_folder[1] == '/' && getenv("HOME")) {
    home = getenv("HOME");
    home_len = strlen(home);
    ++s.public_folder;
    --s.public_folder_length;
    if (home[home_len - 1] == '/')
      --home_len;
  }
  /* allocate and copy data - ensure locality by using single allocation */
  const size_t size = sizeof(s) + s.public_folder_length + home_len + 1;
  http_settings_s *cpy = calloc(size, 1);
  *cpy = s;
  if (s.public_folder) {
    cpy->public_folder = (char *)(cpy + 1);
    if (home) {
      memcpy((char *)cpy->public_folder, home, home_len);
      cpy->public_folder_length += home_len;
    }
    memcpy((char *)cpy->public_folder + home_len, s.public_folder,
           s.public_folder_length);
  }
  return cpy;
}

static void http_settings_free(http_settings_s *s) {
  fiobj_free(s->static_headers);
  free(s);
}

/* *****************************************************************************
Listening to HTTP connections
***************************************************************************** */

static uint8_t fio_http_at_capa = 0;

static void http_on_server_protocol_http1(intptr_t uuid, void *set,
                                          void *ignr_) {
  fio_timeout_set(uuid, ((http_settings_s *)set)->timeout);
  if (fio_uuid2fd(uuid) >= ((http_settings_s *)set)->max_clients) {
    if (!fio_http_at_capa)
      FIO_LOG_WARNING("HTTP server at capacity");
    fio_http_at_capa = 1;
    http_send_error2(uuid, 503, set);
    fio_close(uuid);
    return;
  }
  fio_http_at_capa = 0;
  fio_protocol_s *pr = http1_new(uuid, set, NULL, 0);
  if (!pr)
    fio_close(uuid);
  (void)ignr_;
}

static void http_on_open(intptr_t uuid, void *set) {
  http_on_server_protocol_http1(uuid, set, NULL);
}

static void http_on_finish(intptr_t uuid, void *set) {
  http_settings_s *settings = set;

  if (settings->on_finish)
    settings->on_finish(settings);

  http_settings_free(settings);
  (void)uuid;
}

/**
 * Listens to HTTP connections at the specified `port`.
 *
 * Leave as NULL to ignore IP binding.
 *
 * Returns -1 on error and the socket's uuid on success.
 *
 * the `on_finish` callback is always called.
 */
intptr_t http_listen___(void); /* SublimeText Marker */
intptr_t http_listen FIO_NOOP(const char *port, const char *binding,
                              http_settings_s s) {
  if (s.on_request == NULL) {
    FIO_LOG_FATAL("http_listen requires the .on_request parameter "
                  "to be set\n");
    kill(0, SIGINT);
    exit(11);
  }

  http_settings_s *settings = http_settings_new(s);
  settings->is_client = 0;
  if (settings->tls) {
    fio_tls_alpn_add(settings->tls, "http/1.1", http_on_server_protocol_http1,
                     NULL, NULL);
  }
  return fio_listen(.port = port, .address = binding, .tls = s.tls,
                    .on_finish = http_on_finish, .on_open = http_on_open,
                    .udata = settings);
}

/* *****************************************************************************
HTTP Client Connections
***************************************************************************** */

static void http_on_close_client(intptr_t uuid, fio_protocol_s *protocol) {
  http_fio_protocol_s *p = (http_fio_protocol_s *)protocol;
  http_settings_s *set = p->settings;
  void (**original)(intptr_t, fio_protocol_s *) =
      (void (**)(intptr_t, fio_protocol_s *))(set + 1);
  if (set->on_finish)
    set->on_finish(set);

  original[0](uuid, protocol);
  http_settings_free(set);
}

static void http_on_open_client_perform(http_settings_s *set) {
  http_s *h = set->udata;
  set->on_response(h);
}
static void http_on_open_client_http1(intptr_t uuid, void *set_,
                                      void *ignore_) {
  http_settings_s *set = set_;
  http_s *h = set->udata;
  fio_timeout_set(uuid, set->timeout);
  fio_protocol_s *pr = http1_new(uuid, set, NULL, 0);
  if (!pr) {
    fio_close(uuid);
    return;
  }
  { /* store the original on_close at the end of the struct, we wrap it. */
    void (**original)(intptr_t, fio_protocol_s *) =
        (void (**)(intptr_t, fio_protocol_s *))(set + 1);
    *original = pr->on_close;
    pr->on_close = http_on_close_client;
  }
  HTTP2PRIVATE(h)->pr = (http_fio_protocol_s *)pr;
  HTTP2PRIVATE(h)->vtbl = http1_vtable();
  http_on_open_client_perform(set);
  (void)ignore_;
}

static void http_on_open_client(intptr_t uuid, void *set_) {
  http_on_open_client_http1(uuid, set_, NULL);
}

static void http_on_client_failed(intptr_t uuid, void *set_) {
  http_settings_s *set = set_;
  http_s *h = set->udata;
  set->udata = h->udata;
  http_h_destroy(HTTP2PRIVATE(h), 0);
  fio_free(HTTP2PRIVATE(h));
  if (set->on_finish)
    set->on_finish(set);
  http_settings_free(set);
  (void)uuid;
}

/**
 * Connects to an HTTP server as a client.
 *
 * Upon a successful connection, the `on_response` callback is called with an
 * empty `http_s*` handler (status == 0). Use the same API to set it's content
 * and send the request to the server. The next`on_response` will contain the
 * response.
 *
 * `address` should contain a full URL style address for the server. i.e.:
 *           "http:/www.example.com:8080/"
 *
 * Returns -1 on error and 0 on success. the `on_finish` callback is always
 * called.
 */
/**
 * Connects to an HTTP server as a client.
 *
 * Upon a successful connection, the `on_response` callback is called with an
 * empty `http_s*` handler (status == 0). Use the same API to set it's content
 * and send the request to the server. The next`on_response` will contain the
 * response.
 *
 * `address` should contain a full URL style address for the server. i.e.:
 *
 *           "http:/www.example.com:8080/"
 *
 * If an `address` includes a path or query data, they will be automatically
 * attached (both of them) to the HTTP handl'es `path` property. i.e.
 *
 *           "http:/www.example.com:8080/my_path?foo=bar"
 *           // will result in:
 *           fiobj2cstr(h->path).data; //=> "/my_path?foo=bar"
 *
 * To open a Websocket connection, it's possible to use the `ws` protocol
 * signature. However, it would be better to use the `websocket_connect`
 * function instead.
 *
 * Returns -1 on error and the socket's uuid on success.
 *
 * The `on_finish` callback is always called.
 */
intptr_t http_connect___(void); /* SublimeText Marker */
intptr_t http_connect FIO_NOOP(const char *url, const char *unix_address,
                               http_settings_s arg_settings) {
  if (!arg_settings.on_response && !arg_settings.on_upgrade) {
    FIO_LOG_ERROR("http_connect requires either an on_response "
                  " or an on_upgrade callback.\n");
    errno = EINVAL;
    goto on_error;
  }
  size_t len = 0, h_len = 0;
  char *a = NULL, *p = NULL, *host = NULL;
  uint8_t is_websocket = 0;
  uint8_t is_secure = 0;
  FIOBJ path = FIOBJ_INVALID;
  if (!url && !unix_address) {
    FIO_LOG_ERROR("http_connect requires a valid address.");
    errno = EINVAL;
    goto on_error;
  }
  if (url) {
    fio_url_s u = fio_url_parse(url, strlen(url));
    if (u.scheme.buf &&
        (u.scheme.len == 2 || (u.scheme.len == 3 && u.scheme.buf[2] == 's')) &&
        u.scheme.buf[0] == 'w' && u.scheme.buf[1] == 's') {
      is_websocket = 1;
      is_secure = (u.scheme.len == 3);
    } else if (u.scheme.buf &&
               (u.scheme.len == 4 ||
                (u.scheme.len == 5 && u.scheme.buf[4] == 's')) &&
               u.scheme.buf[0] == 'h' && u.scheme.buf[1] == 't' &&
               u.scheme.buf[2] == 't' && u.scheme.buf[3] == 'p') {
      is_secure = (u.scheme.len == 5);
    }
    if (is_secure && !arg_settings.tls) {
      FIO_LOG_ERROR("Secure connections (%.*s) require a TLS object.",
                    (int)u.scheme.len, u.scheme.buf);
      errno = EINVAL;
      goto on_error;
    }
    if (u.path.buf) {
      path = fiobj_str_new_cstr(
          u.path.buf, strlen(u.path.buf)); /* copy query and target as well */
    }
    if (unix_address) {
      a = (char *)unix_address;
      h_len = len = strlen(a);
      host = a;
    } else {
      if (!u.host.buf) {
        FIO_LOG_ERROR("http_connect requires a valid address.");
        errno = EINVAL;
        goto on_error;
      }
      /***** no more error handling, since memory is allocated *****/
      /* copy address */
      a = fio_malloc(u.host.len + 1);
      memcpy(a, u.host.buf, u.host.len);
      a[u.host.len] = 0;
      len = u.host.len;
      /* copy port */
      if (u.port.buf) {
        p = fio_malloc(u.port.len + 1);
        memcpy(p, u.port.buf, u.port.len);
        p[u.port.len] = 0;
      } else if (is_secure) {
        p = fio_malloc(3 + 1);
        memcpy(p, "443", 3);
        p[3] = 0;
      } else {
        p = fio_malloc(2 + 1);
        memcpy(p, "80", 2);
        p[2] = 0;
      }
    }
    if (u.host.buf) {
      host = u.host.buf;
      h_len = u.host.len;
    }
  }

  /* set settings */
  if (!arg_settings.timeout)
    arg_settings.timeout = 30;
  http_settings_s *settings = http_settings_new(arg_settings);
  settings->is_client = 1;

  if (!arg_settings.ws_timeout)
    settings->ws_timeout = 0; /* allow server to dictate timeout */
  if (!arg_settings.timeout)
    settings->timeout = 0; /* allow server to dictate timeout */
  http_internal_s *h = fio_malloc(sizeof(*h));
  FIO_ASSERT(h, "HTTP Client handler allocation failed");
  *h = (http_internal_s)HTTP_H_INIT(http1_vtable(), NULL);
  h->public.udata = arg_settings.udata;
  h->public.status = 0;
  h->public.path = path;
  settings->udata = &h->public;
  settings->tls = arg_settings.tls;
  if (host)
    http_set_header2(&h->public,
                     (fio_str_info_s){.buf = (char *)"host", .len = 4},
                     (fio_str_info_s){.buf = host, .len = h_len});
  intptr_t ret;
  if (is_websocket) {
    /* force HTTP/1.1 */
    ret = fio_connect(.address = a, .port = p, .on_fail = http_on_client_failed,
                      .on_connect = http_on_open_client, .udata = settings,
                      .tls = arg_settings.tls);
    (void)0;
  } else {
    /* Allow for any HTTP version */
    ret = fio_connect(.address = a, .port = p, .on_fail = http_on_client_failed,
                      .on_connect = http_on_open_client, .udata = settings,
                      .tls = arg_settings.tls);
    (void)0;
  }
  if (a != unix_address)
    fio_free(a);
  fio_free(p);
  return ret;
on_error:
  if (arg_settings.on_finish)
    arg_settings.on_finish(&arg_settings);
  return -1;
}

/* *****************************************************************************
HTTP connection information
***************************************************************************** */

/**
 * Returns the settings used to setup the connection or NULL on error.
 */
http_settings_s *http_settings(http_s *h_) {
  http_internal_s *h = HTTP2PRIVATE(h_);
  if (HTTP_S_INVALID(h_))
    return NULL;
  return h->pr->settings;
}

/**
 * Returns the direct address of the connected peer (likely an intermediary).
 */
fio_str_info_s http_peer_addr(http_s *h_) {
  http_internal_s *h = HTTP2PRIVATE(h_);
  if (HTTP_S_INVALID(h_))
    return (fio_str_info_s){.buf = NULL};
  /* TODO: test headers for address */
  return fio_peer_addr(h->pr->uuid);
}

/* *****************************************************************************
HTTP Connection Hijacking
***************************************************************************** */

/**
 * Hijacks the socket away from the HTTP protocol and away from facil.io.
 *
 * It's possible to hijack the socket and than reconnect it to a new protocol
 * object.
 *
 * It's possible to call `http_finish` immediately after calling `http_hijack`
 * in order to send the outgoing headers.
 *
 * If any additional HTTP functions are called after the hijacking, the protocol
 * object might attempt to continue reading data from the buffer.
 *
 * Returns the underlining socket connection's uuid. If `leftover` isn't NULL,
 * it will be populated with any remaining data in the HTTP buffer (the data
 * will be automatically deallocated, so copy the data when in need).
 *
 * WARNING: this isn't a good way to handle HTTP connections, especially as
 * HTTP/2 enters the picture.
 */
intptr_t http_hijack(http_s *h_, fio_str_info_s *leftover) {
  http_internal_s *h = HTTP2PRIVATE(h_);
  if (HTTP_S_INVALID(h_))
    return -1;
  /* TODO: test headers for address */
  return h->vtbl->hijack(h, leftover);
}

/* *****************************************************************************
Websocket Upgrade (Server and Client connection establishment)
***************************************************************************** */

/**
 * Upgrades an HTTP/1.1 connection to a Websocket connection.
 *
 * This function will end the HTTP stage of the connection and attempt to
 * "upgrade" to a Websockets connection.
 *
 * Thie `http_s` handle will be invalid after this call and the `udata` will be
 * set to the new Websocket `udata`.
 *
 * A client connection's `on_finish` callback will be called (since the HTTP
 * stage has finished).
 */
void http_upgrade2ws___(void); /* SublimeText Marker */
int http_upgrade2ws FIO_NOOP(http_s *http, websocket_settings_s);

/**
 * Connects to a Websocket service according to the provided address.
 *
 * This is a somewhat naive connector object, it doesn't perform any
 * authentication or other logical handling. However, it's quire easy to author
 * a complext authentication logic using a combination of `http_connect` and
 * `http_upgrade2ws`.
 *
 * Returns the uuid for the future websocket on success.
 *
 * Returns -1 on error;
 */
void websocket_connect___(void); /* SublimeText Marker */
int websocket_connect FIO_NOOP(const char *url, websocket_settings_s settings);

/* *****************************************************************************
EventSource Support (SSE)
***************************************************************************** */

/**
 * Upgrades an HTTP connection to an EventSource (SSE) connection.
 *
 * The `http_s` handle will be invalid after this call.
 *
 * On HTTP/1.1 connections, this will preclude future requests using the same
 * connection.
 */
void http_upgrade2sse___(void); /* SublimeText Marker */
int http_upgrade2sse FIO_NOOP(http_s *h, http_sse_s);

/**
 * Sets the ping interval for SSE connections.
 */
void http_sse_set_timout(http_sse_s *sse, uint8_t timeout);

/**
 * Subscribes to a channel for direct message deliverance. See {struct
 * http_sse_subscribe_args} for possible arguments.
 *
 * Returns a subscription ID on success and 0 on failure.
 *
 * To unsubscripbe from the channel, use `http_sse_unsubscribe` (NOT
 * `fio_unsubscribe`).
 *
 * All subscriptions are automatically cleared once the connection is closed.
 */
void http_sse_subscribe___(void); /* SublimeText Marker */
uintptr_t http_sse_subscribe FIO_NOOP(http_sse_s *sse,
                                      struct http_sse_subscribe_args args);

/**
 * Cancels a subscription and invalidates the subscription object.
 */
void http_sse_unsubscribe(http_sse_s *sse, uintptr_t subscription);

// struct http_sse_write_args {
//   fio_str_info_s id;
//   fio_str_info_s event;
//   fio_str_info_s data;
//   intptr_t retry;
// };

/**
 * Writes data to an EventSource (SSE) connection.
 *
 * See the {struct http_sse_write_args} for possible named arguments.
 */
void http_sse_write___(void); /* SublimeText Marker */
int http_sse_write FIO_NOOP(http_sse_s *sse, struct http_sse_write_args);

/**
 * Get the connection's UUID (for `fio_defer_io_task`, pub/sub, etc').
 */
intptr_t http_sse2uuid(http_sse_s *sse);

/**
 * Closes an EventSource (SSE) connection.
 */
int http_sse_close(http_sse_s *sse);

/**
 * Duplicates an SSE handle by reference, remember to http_sse_free.
 *
 * Returns the same object (increases a reference count, no allocation is made).
 */
http_sse_s *http_sse_dup(http_sse_s *sse);

/**
 * Frees an SSE handle by reference (decreases the reference count).
 */
void http_sse_free(http_sse_s *sse);

/* *****************************************************************************
HTTP GET and POST parsing helpers
***************************************************************************** */

/** URL decodes a string, returning a `FIOBJ`. */
static inline FIOBJ http_urlstr2fiobj(char *s, size_t len) {
  FIOBJ o = fiobj_str_new_buf(len);
  ssize_t l = http_decode_url(fiobj_str2cstr(o).buf, s, len);
  if (l < 0) {
    fiobj_str_destroy(o);
    return o; /* empty string */
  }
  fiobj_str_resize(o, (size_t)l);
  return o;
}

/** converts a string into a `FIOBJ`. */
static inline FIOBJ http_str2fiobj(char *s, size_t len, uint8_t encoded) {
  /* TODO: decode before conversion */
  // FIOBJ ret = FIOBJ_INVALID;
  // fiobj to_free = FIOBJ_INVALID;
  switch (len) {
  case 0:
    return fiobj_str_new(); /* empty string */
  case 4:
    if (!strncasecmp(s, "true", 4))
      return fiobj_true();
    if (!strncasecmp(s, "null", 4))
      return fiobj_null();
    break;
  case 5:
    if (!strncasecmp(s, "false", 5))
      return fiobj_false();
  }
  {
    char *end = s;
    const uint64_t tmp = fio_atol(&end);
    if (end == s + len)
      return fiobj_num_new(tmp);
  }
  {
    char *end = s;
    const double tmp = fio_atof(&end);
    if (end == s + len)
      return fiobj_float_new(tmp);
  }
  if (encoded)
    return http_urlstr2fiobj(s, len);
  return fiobj_str_new_cstr(s, len);
}

static inline void http_parse_cookies_cookie_str(FIOBJ dest, FIOBJ str,
                                                 uint8_t is_url_encoded) {
  if (!FIOBJ_TYPE_IS(str, FIOBJ_T_STRING))
    return;
  fio_str_info_s s = fiobj2cstr(str);
  while (s.len) {
    if (s.buf[0] == ' ') {
      ++s.buf;
      --s.len;
      continue;
    }
    char *cut = memchr(s.buf, '=', s.len);
    if (!cut)
      cut = s.buf;
    char *cut2 = memchr(cut, ';', s.len - (cut - s.buf));
    if (!cut2)
      cut2 = s.buf + s.len;
    http_add2hash(dest, s.buf, cut - s.buf, cut + 1, (cut2 - (cut + 1)),
                  is_url_encoded);
    if ((size_t)((cut2 + 1) - s.buf) > s.len)
      s.len = 0;
    else
      s.len -= ((cut2 + 1) - s.buf);
    s.buf = cut2 + 1;
  }
}

static inline void http_parse_cookies_setcookie_str(FIOBJ dest, FIOBJ str,
                                                    uint8_t is_url_encoded) {
  if (!FIOBJ_TYPE_IS(str, FIOBJ_T_STRING))
    return;
  fio_str_info_s s = fiobj2cstr(str);
  char *cut = memchr(s.buf, '=', s.len);
  if (!cut)
    cut = s.buf;
  char *cut2 = memchr(cut, ';', s.len - (cut - s.buf));
  if (!cut2)
    cut2 = s.buf + s.len;
  if (cut2 > cut)
    http_add2hash(dest, s.buf, cut - s.buf, cut + 1, (cut2 - (cut + 1)),
                  is_url_encoded);
}

/**
 * Parses the query part of an HTTP request/response. Uses `http_add2hash`.
 *
 * This should be called after the `http_parse_body` function, just in case the
 * body is JSON that doesn't have an object at it's root.
 */
void http_parse_query(http_s *h) {
  if (!h->query)
    return;
  if (!h->params)
    h->params = fiobj_hash_new();
  fio_str_info_s q = fiobj2cstr(h->query);
  do {
    char *cut = memchr(q.buf, '&', q.len);
    if (!cut)
      cut = q.buf + q.len;
    char *cut2 = memchr(q.buf, '=', (cut - q.buf));
    if (cut2) {
      /* we only add named elements... */
      http_add2hash(h->params, q.buf, (size_t)(cut2 - q.buf), (cut2 + 1),
                    (size_t)(cut - (cut2 + 1)), 1);
    }
    if (cut[0] == '&') {
      /* protecting against some ...less informed... clients */
      if (cut[1] == 'a' && cut[2] == 'm' && cut[3] == 'p' && cut[4] == ';')
        cut += 5;
      else
        cut += 1;
    }
    q.len -= (uintptr_t)(cut - q.buf);
    q.buf = cut;
  } while (q.len);
}

/** Parses any Cookie / Set-Cookie headers, using the `http_add2hash` scheme. */
void http_parse_cookies(http_s *h, uint8_t is_url_encoded) {
  if (!h->headers)
    return;
  if (h->cookies && fiobj_hash_count(h->cookies)) {
    FIO_LOG_WARNING("(http) attempting to parse cookies more than once.");
    return;
  }
  FIOBJ c = fiobj_hash_get2(h->headers, HTTP_HEADER_COOKIE);
  if (c) {
    if (!h->cookies)
      h->cookies = fiobj_hash_new();
    if (FIOBJ_TYPE_IS(c, FIOBJ_T_ARRAY)) {
      /* Array of Strings */
      size_t count = fiobj_array_count(c);
      for (size_t i = 0; i < count; ++i) {
        http_parse_cookies_cookie_str(
            h->cookies, fiobj_array_get(c, (int64_t)i), is_url_encoded);
      }
    } else {
      /* single string */
      http_parse_cookies_cookie_str(h->cookies, c, is_url_encoded);
    }
  }
  c = fiobj_hash_get2(h->headers, HTTP_HEADER_SET_COOKIE);
  if (c) {
    if (!h->cookies)
      h->cookies = fiobj_hash_new();
    if (FIOBJ_TYPE_IS(c, FIOBJ_T_ARRAY)) {
      /* Array of Strings */
      size_t count = fiobj_array_count(c);
      for (size_t i = 0; i < count; ++i) {
        http_parse_cookies_setcookie_str(
            h->cookies, fiobj_array_get(c, (int64_t)i), is_url_encoded);
      }
    } else {
      /* single string */
      http_parse_cookies_setcookie_str(h->cookies, c, is_url_encoded);
    }
  }
}

/**
 * Adds a named parameter to the hash, converting a string to an object and
 * resolving nesting references and URL decoding if required.
 *
 * i.e.:
 *
 * * "name[]" references a nested Array (nested in the Hash).
 * * "name[key]" references a nested Hash.
 * * "name[][key]" references a nested Hash within an array. Hash keys will be
 *   unique (repeating a key advances the hash).
 * * These rules can be nested (i.e. "name[][key1][][key2]...")
 * * "name[][]" is an error (there's no way for the parser to analyze
 *    dimensions)
 *
 * Note: names can't begin with "[" or end with "]" as these are reserved
 *       characters.
 */
int http_add2hash(FIOBJ dest, char *name, size_t name_len, char *value,
                  size_t value_len, uint8_t encoded) {
  return http_add2hash2(dest, name, name_len,
                        http_str2fiobj(value, value_len, encoded), encoded);
}

/**
 * Adds a named parameter to the hash, using an existing object and resolving
 * nesting references.
 *
 * i.e.:
 *
 * * "name[]" references a nested Array (nested in the Hash).
 * * "name[key]" references a nested Hash.
 * * "name[][key]" references a nested Hash within an array. Hash keys will be
 *   unique (repeating a key advances the array).
 * * These rules can be nested (i.e. "name[][key1][][key2]...")
 * * "name[][]" is an error (there's no way for the parser to analyze
 *    dimensions)
 *
 * Note: names can't begin with "[" or end with "]" as these are reserved
 *       characters.
 */
int http_add2hash2(FIOBJ dest, char *name, size_t name_len, FIOBJ val,
                   uint8_t encoded) {
  if (!name)
    goto error;
  FIOBJ nested_ary = FIOBJ_INVALID;
  char *cut1;
  /* we can't start with an empty object name */
  while (name_len && name[0] == '[') {
    --name_len;
    ++name;
  }
  if (!name_len) {
    /* an empty name is an error */
    goto error;
  }
  uint32_t nesting = ((uint32_t)~0);
rebase:
  /* test for nesting level limit (limit at 32) */
  if (!nesting)
    goto error;
  /* start clearing away bits. */
  nesting >>= 1;
  /* since we might be rebasing, notice that "name" might be "name]" */
  cut1 = memchr(name, '[', name_len);
  if (!cut1)
    goto place_in_hash;
  /* simple case "name=" (the "=" was already removed) */
  if (cut1 == name) {
    /* an empty name is an error */
    goto error;
  }
  if (cut1 + 1 == name + name_len) {
    /* we have name[= ... autocorrect */
    name_len -= 1;
    goto place_in_array;
  }

  if (cut1[1] == ']') {
    /* Nested Array "name[]..." */

    /* Test for name[]= */
    if ((cut1 + 2) == name + name_len) {
      name_len -= 2;
      goto place_in_array;
    }

    /* Test for a nested Array format error */
    if (cut1[2] != '[' || cut1[3] == ']') { /* error, we can't parse this */
      goto error;
    }

    /* we have name[][key...= */

    /* ensure array exists and it's an array + set nested_ary */
    const size_t len = ((cut1[-1] == ']') ? (size_t)((cut1 - 1) - name)
                                          : (size_t)(cut1 - name));
    {
      FIOBJ key = encoded ? http_urlstr2fiobj(name, len)
                          : fiobj_str_new_cstr(name, len);
      nested_ary = fiobj_hash_get2(dest, key);
      if (nested_ary == FIOBJ_INVALID) {
        /* create a new nested array */
        nested_ary = fiobj_array_new();
        fiobj_hash_set2(dest, key, nested_ary);
      } else if (!FIOBJ_TYPE_IS(nested_ary, FIOBJ_T_ARRAY)) {
        /* convert existing object to an array - auto error correction */
        FIOBJ tmp = fiobj_array_new();
        fiobj_array_push(tmp, nested_ary);
        nested_ary = tmp;
        fiobj_hash_set2(dest, key, nested_ary);
      }
      fiobj_free(key);
    }

    /* test if last object in the array is a hash - create hash if not */
    dest = fiobj_array_get(nested_ary, -1);
    if (!dest || !FIOBJ_TYPE_IS(dest, FIOBJ_T_HASH)) {
      dest = fiobj_hash_new();
      fiobj_array_push(nested_ary, dest);
    }

    /* rebase `name` to `key` and restart. */
    cut1 += 3; /* consume "[][" */
    name_len -= (size_t)(cut1 - name);
    name = cut1;
    goto rebase;

  } else {
    /* we have name[key]... */
    const size_t len = ((cut1[-1] == ']') ? (size_t)((cut1 - 1) - name)
                                          : (size_t)(cut1 - name));
    FIOBJ key =
        encoded ? http_urlstr2fiobj(name, len) : fiobj_str_new_cstr(name, len);
    FIOBJ tmp = fiobj_hash_get2(dest, key);
    if (tmp == FIOBJ_INVALID) {
      /* hash doesn't exist, create it */
      tmp = fiobj_hash_new();
      fiobj_hash_set2(dest, key, tmp);
      fiobj_free(key);
    } else if (!FIOBJ_TYPE_IS(tmp, FIOBJ_T_HASH)) {
      /* type error, referencing an existing object that isn't a Hash */
      fiobj_free(key);
      goto error;
    }
    dest = tmp;
    /* no rollback is possible once we enter the new nesting level... */
    nested_ary = FIOBJ_INVALID;
    /* rebase `name` to `key` and restart. */
    cut1 += 1; /* consume "[" */
    name_len -= (size_t)(cut1 - name);
    name = cut1;
    goto rebase;
  }

place_in_hash:
  if (name[name_len - 1] == ']')
    --name_len;
  {
    FIOBJ key = encoded ? http_urlstr2fiobj(name, name_len)
                        : fiobj_str_new_cstr(name, name_len);
    FIOBJ tmp = FIOBJ_INVALID;
    fiobj_hash_set(dest, fiobj2hash(dest, key), key, val, &tmp);
    if (tmp != FIOBJ_INVALID) {
      if (nested_ary) {
        /* don't free val */
        fiobj_hash_set(dest, fiobj2hash(dest, key), key, tmp, &tmp);
        tmp = fiobj_hash_new();
        fiobj_hash_set2(tmp, key, val);
        fiobj_array_push(nested_ary, tmp);
      } else {
        if (!FIOBJ_TYPE_IS(tmp, FIOBJ_T_ARRAY)) {
          FIOBJ tmp2 = fiobj_array_new();
          fiobj_array_push(tmp2, tmp);
          tmp = tmp2;
        }
        fiobj_array_push(tmp, val);
        /* don't free val */
        fiobj_hash_set(dest, fiobj2hash(dest, key), key, tmp, &tmp);
      }
    }
    fiobj_free(key);
  }
  return 0;

place_in_array:
  if (name[name_len - 1] == ']')
    --name_len;
  {
    FIOBJ key = encoded ? http_urlstr2fiobj(name, name_len)
                        : fiobj_str_new_cstr(name, name_len);
    FIOBJ ary = fiobj_hash_get2(dest, key);
    if (ary == FIOBJ_INVALID) {
      ary = fiobj_array_new();
      fiobj_hash_set2(dest, key, ary);
    } else if (!FIOBJ_TYPE_IS(ary, FIOBJ_T_ARRAY)) {
      FIOBJ tmp = fiobj_array_new();
      fiobj_array_push(tmp, ary);
      ary = tmp;
      fiobj_hash_set(dest, fiobj2hash(dest, key), key, ary, &tmp);
    }
    fiobj_array_push(ary, val);
    fiobj_free(key);
  }
  return 0;
error:
  fiobj_free(val);
  errno = EOPNOTSUPP;
  return -1;
}

/* *****************************************************************************
HTTP Body Parsing
***************************************************************************** */
#include <http_mime_parser.h>

typedef struct {
  http_mime_parser_s p;
  http_s *h;
  fio_str_info_s buffer;
  size_t pos;
  size_t partial_offset;
  size_t partial_length;
  FIOBJ partial_name;
} http_fio_mime_s;

#define http_mime_parser2fio(parser) ((http_fio_mime_s *)(parser))

/** Called when all the data is available at once. */
static void http_mime_parser_on_data(http_mime_parser_s *parser, void *name,
                                     size_t name_len, void *filename,
                                     size_t filename_len, void *mimetype,
                                     size_t mimetype_len, void *value,
                                     size_t value_len) {
  if (!filename_len) {
    http_add2hash(http_mime_parser2fio(parser)->h->params, name, name_len,
                  value, value_len, 0);
    return;
  }
  FIOBJ n = fiobj_str_new_cstr(name, name_len);
  fiobj_str_write(n, "[data]", 6);
  fio_str_info_s tmp = fiobj2cstr(n);
  http_add2hash(http_mime_parser2fio(parser)->h->params, tmp.buf, tmp.len,
                value, value_len, 0);
  fiobj_str_resize(n, name_len);
  fiobj_str_write(n, "[name]", 6);
  tmp = fiobj2cstr(n);
  http_add2hash(http_mime_parser2fio(parser)->h->params, tmp.buf, tmp.len,
                filename, filename_len, 0);
  if (mimetype_len) {
    fiobj_str_resize(n, name_len);
    fiobj_str_write(n, "[type]", 6);
    tmp = fiobj2cstr(n);
    http_add2hash(http_mime_parser2fio(parser)->h->params, tmp.buf, tmp.len,
                  mimetype, mimetype_len, 0);
  }
  fiobj_free(n);
}

/** Called when the data didn't fit in the buffer. Data will be streamed. */
static void http_mime_parser_on_partial_start(
    http_mime_parser_s *parser, void *name, size_t name_len, void *filename,
    size_t filename_len, void *mimetype, size_t mimetype_len) {
  http_mime_parser2fio(parser)->partial_length = 0;
  http_mime_parser2fio(parser)->partial_offset = 0;
  http_mime_parser2fio(parser)->partial_name =
      fiobj_str_new_cstr(name, name_len);

  if (!filename)
    return;

  fiobj_str_write(http_mime_parser2fio(parser)->partial_name, "[type]", 6);
  fio_str_info_s tmp = fiobj2cstr(http_mime_parser2fio(parser)->partial_name);
  http_add2hash(http_mime_parser2fio(parser)->h->params, tmp.buf, tmp.len,
                mimetype, mimetype_len, 0);

  fiobj_str_resize(http_mime_parser2fio(parser)->partial_name, name_len);
  fiobj_str_write(http_mime_parser2fio(parser)->partial_name, "[name]", 6);
  tmp = fiobj2cstr(http_mime_parser2fio(parser)->partial_name);
  http_add2hash(http_mime_parser2fio(parser)->h->params, tmp.buf, tmp.len,
                filename, filename_len, 0);

  fiobj_str_resize(http_mime_parser2fio(parser)->partial_name, name_len);
  fiobj_str_write(http_mime_parser2fio(parser)->partial_name, "[data]", 6);
}

/** Called when partial data is available. */
static void http_mime_parser_on_partial_data(http_mime_parser_s *parser,
                                             void *value, size_t value_len) {
  if (!http_mime_parser2fio(parser)->partial_offset)
    http_mime_parser2fio(parser)->partial_offset =
        http_mime_parser2fio(parser)->pos +
        ((uintptr_t)value -
         (uintptr_t)http_mime_parser2fio(parser)->buffer.buf);
  http_mime_parser2fio(parser)->partial_length += value_len;
  (void)value;
}

/** Called when the partial data is complete. */
static void http_mime_parser_on_partial_end(http_mime_parser_s *parser) {

  fio_str_info_s tmp = fiobj2cstr(http_mime_parser2fio(parser)->partial_name);
  FIOBJ o = FIOBJ_INVALID;
  if (!http_mime_parser2fio(parser)->partial_length)
    return;
  if (http_mime_parser2fio(parser)->partial_length < 42) {
    /* short data gets a new object */
    o = fiobj_str_new_cstr(http_mime_parser2fio(parser)->buffer.buf +
                               http_mime_parser2fio(parser)->partial_offset,
                           http_mime_parser2fio(parser)->partial_length);
  } else {
    /* longer data gets a reference object (memory collision concerns) */
    o = fiobj_io_new_slice(http_mime_parser2fio(parser)->h->body,
                           http_mime_parser2fio(parser)->partial_offset,
                           http_mime_parser2fio(parser)->partial_length);
  }
  http_add2hash2(http_mime_parser2fio(parser)->h->params, tmp.buf, tmp.len, o,
                 0);
  fiobj_free(http_mime_parser2fio(parser)->partial_name);
  http_mime_parser2fio(parser)->partial_name = FIOBJ_INVALID;
  http_mime_parser2fio(parser)->partial_offset = 0;
}

/**
 * Called when URL decoding is required.
 *
 * Should support inplace decoding (`dest == encoded`).
 *
 * Should return the length of the decoded string.
 */
static inline size_t http_mime_decode_url(char *dest, const char *encoded,
                                          size_t length) {
  return http_decode_url(dest, encoded, length);
}
/**
 * Attempts to decode the request's body.
 *
 * Supported Types include:
 * * application/x-www-form-urlencoded
 * * application/json
 * * multipart/form-data
 *
 * This should be called before `http_parse_query`, in order to support JSON
 * data.
 *
 * If the JSON data isn't an object, it will be saved under the key "JSON" in
 * the `params` hash.
 *
 * If the `multipart/form-data` type contains JSON files, they will NOT be
 * parsed (they will behave like any other file, with `data`, `type` and
 * `filename` keys assigned). This allows non-object JSON data (such as array)
 * to be handled by the app.
 */
int http_parse_body(http_s *h) {
  if (!h->body)
    return -1;
  FIOBJ ct = fiobj_hash_get2(h->headers, HTTP_HEADER_CONTENT_TYPE);
  fio_str_info_s content_type = fiobj2cstr(ct);
  if (content_type.len < 16)
    return -1;
  if (content_type.len >= 33 &&
      !strncasecmp("application/x-www-form-urlencoded", content_type.buf, 33)) {
    if (!h->params)
      h->params = fiobj_hash_new();
    FIOBJ tmp = h->query;
    h->query = h->body;
    http_parse_query(h);
    h->query = tmp;
    return 0;
  }
  if (content_type.len >= 16 &&
      !strncasecmp("application/json", content_type.buf, 16)) {
    if (h->params)
      return -1;
    size_t json_consumed = 0;
    h->params = fiobj_json_parse(fiobj2cstr(h->body), &json_consumed);
    if (json_consumed == 0)
      return -1;
    if (FIOBJ_TYPE_IS(h->params, FIOBJ_T_HASH))
      return 0;
    FIOBJ tmp = h->params;
    FIOBJ key = fiobj_str_new_cstr("JSON", 4);
    h->params = fiobj_hash_new();
    fiobj_hash_set2(h->params, key, tmp);
    fiobj_free(key);
    return 0;
  }

  http_fio_mime_s p = {.h = h};
  if (http_mime_parser_init(&p.p, content_type.buf, content_type.len))
    return -1;
  if (!h->params)
    h->params = fiobj_hash_new();

  do {
    size_t cons = http_mime_parse(&p.p, p.buffer.buf, p.buffer.len);
    p.pos += cons;
    p.buffer = fiobj_io_pread(h->body, p.pos, 4096);
  } while (p.buffer.buf && !p.p.done && !p.p.error);
  fiobj_free(p.partial_name);
  p.partial_name = FIOBJ_INVALID;
  return 0;
}

/* *****************************************************************************
HTTP Status Strings and Mime-Type helpers
***************************************************************************** */

#define FIO_MALLOC_TMP_USE_SYSTEM 1 /* use malloc for the mime registry */
#define FIO_MAP_NAME fio_mime_set
#define FIO_MAP_TYPE FIOBJ
#define FIO_MAP_TYPE_DESTROY(o) fiobj_free((o));

#include <fio-stl.h>

static fio_mime_set_s fio_http_mime_types = FIO_MAP_INIT;

#define LONGEST_FILE_EXTENSION_LENGTH 15

/** Returns a human readable string related to the HTTP status number. */
fio_str_info_s http_status2str(uintptr_t status) {
// clang-format off
#define HTTP_SET_STATUS_STR(status, str) [status-100] = { .buf = (char *)(str), .len = (sizeof(str) - 1) }
  // clang-format on
  static const fio_str_info_s status2str[] = {
      HTTP_SET_STATUS_STR(100, "Continue"),
      HTTP_SET_STATUS_STR(101, "Switching Protocols"),
      HTTP_SET_STATUS_STR(102, "Processing"),
      HTTP_SET_STATUS_STR(103, "Early Hints"),
      HTTP_SET_STATUS_STR(200, "OK"),
      HTTP_SET_STATUS_STR(201, "Created"),
      HTTP_SET_STATUS_STR(202, "Accepted"),
      HTTP_SET_STATUS_STR(203, "Non-Authoritative Information"),
      HTTP_SET_STATUS_STR(204, "No Content"),
      HTTP_SET_STATUS_STR(205, "Reset Content"),
      HTTP_SET_STATUS_STR(206, "Partial Content"),
      HTTP_SET_STATUS_STR(207, "Multi-Status"),
      HTTP_SET_STATUS_STR(208, "Already Reported"),
      HTTP_SET_STATUS_STR(226, "IM Used"),
      HTTP_SET_STATUS_STR(300, "Multiple Choices"),
      HTTP_SET_STATUS_STR(301, "Moved Permanently"),
      HTTP_SET_STATUS_STR(302, "Found"),
      HTTP_SET_STATUS_STR(303, "See Other"),
      HTTP_SET_STATUS_STR(304, "Not Modified"),
      HTTP_SET_STATUS_STR(305, "Use Proxy"),
      HTTP_SET_STATUS_STR(306, "(Unused), "),
      HTTP_SET_STATUS_STR(307, "Temporary Redirect"),
      HTTP_SET_STATUS_STR(308, "Permanent Redirect"),
      HTTP_SET_STATUS_STR(400, "Bad Request"),
      HTTP_SET_STATUS_STR(403, "Forbidden"),
      HTTP_SET_STATUS_STR(404, "Not Found"),
      HTTP_SET_STATUS_STR(401, "Unauthorized"),
      HTTP_SET_STATUS_STR(402, "Payment Required"),
      HTTP_SET_STATUS_STR(405, "Method Not Allowed"),
      HTTP_SET_STATUS_STR(406, "Not Acceptable"),
      HTTP_SET_STATUS_STR(407, "Proxy Authentication Required"),
      HTTP_SET_STATUS_STR(408, "Request Timeout"),
      HTTP_SET_STATUS_STR(409, "Conflict"),
      HTTP_SET_STATUS_STR(410, "Gone"),
      HTTP_SET_STATUS_STR(411, "Length Required"),
      HTTP_SET_STATUS_STR(412, "Precondition Failed"),
      HTTP_SET_STATUS_STR(413, "Payload Too Large"),
      HTTP_SET_STATUS_STR(414, "URI Too Long"),
      HTTP_SET_STATUS_STR(415, "Unsupported Media Type"),
      HTTP_SET_STATUS_STR(416, "Range Not Satisfiable"),
      HTTP_SET_STATUS_STR(417, "Expectation Failed"),
      HTTP_SET_STATUS_STR(421, "Misdirected Request"),
      HTTP_SET_STATUS_STR(422, "Unprocessable Entity"),
      HTTP_SET_STATUS_STR(423, "Locked"),
      HTTP_SET_STATUS_STR(424, "Failed Dependency"),
      HTTP_SET_STATUS_STR(425, "Unassigned"),
      HTTP_SET_STATUS_STR(426, "Upgrade Required"),
      HTTP_SET_STATUS_STR(427, "Unassigned"),
      HTTP_SET_STATUS_STR(428, "Precondition Required"),
      HTTP_SET_STATUS_STR(429, "Too Many Requests"),
      HTTP_SET_STATUS_STR(430, "Unassigned"),
      HTTP_SET_STATUS_STR(431, "Request Header Fields Too Large"),
      HTTP_SET_STATUS_STR(500, "Internal Server Error"),
      HTTP_SET_STATUS_STR(501, "Not Implemented"),
      HTTP_SET_STATUS_STR(502, "Bad Gateway"),
      HTTP_SET_STATUS_STR(503, "Service Unavailable"),
      HTTP_SET_STATUS_STR(504, "Gateway Timeout"),
      HTTP_SET_STATUS_STR(505, "HTTP Version Not Supported"),
      HTTP_SET_STATUS_STR(506, "Variant Also Negotiates"),
      HTTP_SET_STATUS_STR(507, "Insufficient Storage"),
      HTTP_SET_STATUS_STR(508, "Loop Detected"),
      HTTP_SET_STATUS_STR(509, "Unassigned"),
      HTTP_SET_STATUS_STR(510, "Not Extended"),
      HTTP_SET_STATUS_STR(511, "Network Authentication Required"),
  };
  fio_str_info_s ret = (fio_str_info_s){.len = 0, .buf = NULL};
  if (status >= 100 &&
      (status - 100) < sizeof(status2str) / sizeof(status2str[0]))
    ret = status2str[status - 100];
  if (!ret.buf) {
    ret = status2str[400]; /* 500 - Internal Server Error, offset by 100 */
  }
  return ret;
#undef HTTP_SET_STATUS_STR
}

/** Registers a Mime-Type to be associated with the file extension. */
void http_mimetype_register(char *file_ext, size_t file_ext_len,
                            FIOBJ mime_type_str) {
  uint64_t hash =
      fio_risky_hash(file_ext, file_ext_len, (uint64_t)&fio_http_mime_types);
  FIOBJ old = FIOBJ_INVALID;
  fio_mime_set_set(&fio_http_mime_types, hash, mime_type_str, &old);
  if (old != FIOBJ_INVALID) {
    FIO_LOG_WARNING("mime-type collision: %.*s was %s, now %s",
                    (int)file_ext_len, file_ext, fiobj2cstr(old).buf,
                    fiobj2cstr(mime_type_str).buf);
    fiobj_free(old);
  }
}

/**
 * Finds the mime-type associated with the file extension, returning a String on
 * success and FIOBJ_INVALID on failure.
 *
 * Remember to call `fiobj_free`.
 */
FIOBJ http_mimetype_find(char *file_ext, size_t file_ext_len) {
  uint64_t hash =
      fio_risky_hash(file_ext, file_ext_len, (uint64_t)&fio_http_mime_types);
  return fiobj_dup(fio_mime_set_get(&fio_http_mime_types, hash, FIOBJ_INVALID));
}

/**
 * Returns the mime-type associated with the URL or the default mime-type for
 * HTTP.
 *
 * Remember to call `fiobj_free`.
 */
FIOBJ http_mimetype_find2(FIOBJ url) {
  static __thread char buffer[LONGEST_FILE_EXTENSION_LENGTH + 1];
  fio_str_info_s ext = {.buf = NULL};
  FIOBJ mimetype;
  if (!url)
    goto finish;
  fio_str_info_s tmp = fiobj2cstr(url);
  uint8_t steps = 1;
  while (tmp.len > steps || steps >= LONGEST_FILE_EXTENSION_LENGTH) {
    switch (tmp.buf[tmp.len - steps]) {
    case '.':
      --steps;
      if (steps) {
        ext.len = steps;
        ext.buf = buffer;
        buffer[steps] = 0;
        for (size_t i = 1; i <= steps; ++i) {
          buffer[steps - i] = tolower(tmp.buf[tmp.len - i]);
        }
      }
    /* fallthrough */
    case '/':
      goto finish;
      break;
    }
    ++steps;
  }
finish:
  mimetype = http_mimetype_find(ext.buf, ext.len);
  if (!mimetype)
    mimetype = fiobj_dup(HTTP_HVALUE_CONTENT_TYPE_DEFAULT);
  return mimetype;
}

/** Clears the Mime-Type registry (it will be empty after this call). */
void http_mimetype_clear(void) { fio_mime_set_destroy(&fio_http_mime_types); }

/** Prints out debugging information about the mime storage. */
void http_mimetype_stats(void) {
  FIO_LOG_DEBUG("HTTP MIME hash storage count/capa: %zu / %zu",
                fio_mime_set_count(&fio_http_mime_types),
                fio_mime_set_capa(&fio_http_mime_types));
}

/* *****************************************************************************
Commonly used headers (fiobj Symbol objects)
***************************************************************************** */

// FIOBJ HTTP_HEADER_ACCEPT;
// FIOBJ HTTP_HEADER_CACHE_CONTROL;
// FIOBJ HTTP_HEADER_CONNECTION;
// FIOBJ HTTP_HEADER_CONTENT_ENCODING;
// FIOBJ HTTP_HEADER_CONTENT_LENGTH;
// FIOBJ HTTP_HEADER_CONTENT_RANGE;
// FIOBJ HTTP_HEADER_CONTENT_TYPE;
// FIOBJ HTTP_HEADER_COOKIE;
// FIOBJ HTTP_HEADER_DATE;
// FIOBJ HTTP_HEADER_ETAG;
// FIOBJ HTTP_HEADER_HOST;
// FIOBJ HTTP_HEADER_LAST_MODIFIED;
// FIOBJ HTTP_HEADER_ORIGIN;
// FIOBJ HTTP_HEADER_SET_COOKIE;
// FIOBJ HTTP_HEADER_UPGRADE;

/* *****************************************************************************
HTTP General Helper functions that could be used globally TODO
***************************************************************************** */

struct header_writer_s {
  FIOBJ dest;
};

static int http___write_header(FIOBJ o, void *w_) {
  struct header_writer_s *w = w_;
  FIOBJ header_name = fiobj_hash_each_get_key();
  if (!o || header_name == FIOBJ_INVALID)
    return 0;

  if (FIOBJ_TYPE_IS(o, FIOBJ_T_ARRAY)) {
    fiobj_each1(o, 0, http___write_header, w);
    return 0;
  }
  fio_str_info_s name = fiobj2cstr(header_name);
  fio_str_info_s str = fiobj2cstr(o);
  if (!str.buf)
    return 0;
  fiobj_str_write(w->dest, name.buf, name.len);
  fiobj_str_write(w->dest, ":", 1);
  fiobj_str_write(w->dest, str.buf, str.len);
  fiobj_str_write(w->dest, "\r\n", 2);
  return 0;
}

/**
 * Returns a String object representing the unparsed HTTP request/response (HTTP
 * version is capped at HTTP/1.1). Mostly usable for proxy usage and debugging.
 */
FIOBJ http2str(http_s *h_) {
  http_internal_s *h = HTTP2PRIVATE(h_);
  if (HTTP_S_INVALID(h_))
    return FIOBJ_INVALID;

  struct header_writer_s w;
  w.dest = fiobj_str_new();
  if (h->public.status_str == FIOBJ_INVALID) {
    /* request */
    fiobj_str_join(w.dest, h->public.method);
    fiobj_str_write(w.dest, " ", 1);
    fiobj_str_join(w.dest, h->public.path);
    if (h->public.query) {
      fiobj_str_write(w.dest, "?", 1);
      fiobj_str_join(w.dest, h->public.query);
    }
    {
      fio_str_info_s t = fiobj2cstr(h->public.version);
      if (t.len < 6 || t.buf[5] != '1')
        fiobj_str_write(w.dest, " HTTP/1.1\r\n", 10);
      else {
        fiobj_str_write(w.dest, " ", 1);
        fiobj_str_write(w.dest, t.buf, t.len);
        fiobj_str_write(w.dest, "\r\n", 2);
      }
    }
  } else {
    /* response */
    fiobj_str_join(w.dest, h->public.version);
    fiobj_str_write(w.dest, " ", 1);
    fiobj_str_write_i(w.dest, h->public.status);
    fiobj_str_write(w.dest, " ", 1);
    fiobj_str_join(w.dest, h->public.status_str);
    fiobj_str_write(w.dest, "\r\n", 2);
  }

  fiobj_each1(h->public.headers, 0, http___write_header, &w);
  fiobj_str_write(w.dest, "\r\n", 2);
  if (h->public.body) {
    fio_str_info_s t = fiobj2cstr(h->public.body);
    fiobj_str_write(w.dest, t.buf, t.len);
  }
  return w.dest;
}

/**
 * Writes a log line to `stderr` about the request / response object.
 *
 * This function is called automatically if the `.log` setting is enabled.
 */
void http_write_log(http_s *h_) {
  if (HTTP_S_INVALID(h_))
    return;
  char mem___[FIO_HTTP_LOG_LINE_TRUNCATION + 1];
  http_internal_s *h = HTTP2PRIVATE(h_);
  fio_str_info_s i = {
      .buf = mem___, .len = 0, .capa = FIO_HTTP_LOG_LINE_TRUNCATION};

  struct timespec start, end;
  clock_gettime(CLOCK_REALTIME, &end);
  start = h->public.received_at;

#define HTTP___WRITE_FIOBJ2LOG(o)                                              \
  do {                                                                         \
    fio_str_info_s oi = fiobj2cstr(o);                                         \
    if (oi.len + i.len > i.capa)                                               \
      goto line_truncated;                                                     \
    memcpy(i.buf + i.len, oi.buf, oi.len);                                     \
    i.len += oi.len;                                                           \
  } while (0)
#define HTTP___WRITE_STATIC2LOG(data, length)                                  \
  do {                                                                         \
    if (length + i.len > i.capa)                                               \
      goto line_truncated;                                                     \
    memcpy(i.buf + i.len, data, length);                                       \
    i.len += length;                                                           \
  } while (0)

  {
    // TODO Guess IP address from headers (forwarded) where possible
    fio_str_info_s peer = fio_peer_addr(h->pr->uuid);
    if (peer.len) {
      HTTP___WRITE_STATIC2LOG(peer.buf, peer.len);
    }
  }

  if (i.len == 0) {
    HTTP___WRITE_STATIC2LOG("[unknown]", 9);
  }
  HTTP___WRITE_STATIC2LOG(" - - [", 6);
  {
    FIOBJ date = get_date___();
    HTTP___WRITE_FIOBJ2LOG(date);
    // fiobj_free(date);
  }
  HTTP___WRITE_STATIC2LOG("] \"", 3);
  HTTP___WRITE_FIOBJ2LOG(h->public.method);
  HTTP___WRITE_STATIC2LOG(" ", 1);
  HTTP___WRITE_FIOBJ2LOG(h->public.path);
  HTTP___WRITE_STATIC2LOG(" ", 1);
  HTTP___WRITE_FIOBJ2LOG(h->public.version);
  HTTP___WRITE_STATIC2LOG("\" ", 2);
  if (h->public.status < 1000 && i.len + 3 < i.capa) {
    i.len += fio_ltoa(i.buf + i.len, h->public.status, 10);
  }
  if (h->bytes_sent > 0) {
    HTTP___WRITE_STATIC2LOG(" ", 1);
    char tmp[32];
    size_t len = fio_ltoa(tmp, h->bytes_sent, 10);
    HTTP___WRITE_STATIC2LOG(tmp, len);
    HTTP___WRITE_STATIC2LOG("b ", 2);
  } else {
    HTTP___WRITE_STATIC2LOG(" -- ", 4);
  }

  {
    size_t ms = ((end.tv_sec - start.tv_sec) * 1000) +
                ((end.tv_nsec - start.tv_nsec) / 1000000);
    char tmp[32];
    size_t len = fio_ltoa(tmp, ms, 10);
    HTTP___WRITE_STATIC2LOG(tmp, len);
    HTTP___WRITE_STATIC2LOG("ms\r\n", 4);
  }
  fwrite(i.buf, 1, i.len, stderr);
  return;

line_truncated:
  if (i.len + 5 > i.capa)
    i.len = i.capa - 5;
  memcpy(i.buf + i.len, "...\r\n", 5);
  i.len += 3;
  i.buf[i.len] = 0;
  fwrite(i.buf, 1, i.len, stderr);

#undef HTTP___WRITE_FIOBJ2LOG
#undef HTTP___WRITE_STATIC2LOG
}

/* *****************************************************************************
HTTP Time related helper functions that could be used globally
***************************************************************************** */

static const char *FIO___DAY_NAMES[] = {"Sun", "Mon", "Tue", "Wed",
                                        "Thu", "Fri", "Sat"};
static const char *FIO___MONTH_NAMES[] = {"Jan ", "Feb ", "Mar ", "Apr ",
                                          "May ", "Jun ", "Jul ", "Aug ",
                                          "Sep ", "Oct ", "Nov ", "Dec "};
static const char *FIO___GMT_STR = "GMT";

/**
A faster (yet less localized) alternative to `gmtime_r`.

See the libc `gmtime_r` documentation for details.

Falls back to `gmtime_r` for dates before epoch.
*/
struct tm *http_gmtime(time_t timer, struct tm *tmbuf) {
  static const uint8_t month_len[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, // nonleap year
      31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31  // leap year
  };
  if (timer < 0)
    return gmtime_r(&timer, tmbuf);
  ssize_t a, b;
#if HAVE_TM_TM_ZONE
  *tmbuf = (struct tm){
      .tm_isdst = 0,
      .tm_year = 70, // tm_year == The number of years since 1900
      .tm_mon = 0,
      .tm_zone = "UTC",
  };
#else
  *tmbuf = (struct tm){
      .tm_isdst = 0,
      .tm_year = 70, // tm_year == The number of years since 1900
      .tm_mon = 0,
  };
#endif
  // for seconds up to weekdays, we build up, as small values clean up
  // larger values.
  a = (ssize_t)timer;
  b = a / 60;
  tmbuf->tm_sec = a - (b * 60);
  a = b / 60;
  tmbuf->tm_min = b - (a * 60);
  b = a / 24;
  tmbuf->tm_hour = a - (b * 24);
  // day of epoch was a thursday. Add + 4 so sunday == 0...
  tmbuf->tm_wday = (b + 4) % 7;
// tmp == number of days since epoch
#define DAYS_PER_400_YEARS ((400 * 365) + 97)
  while (b >= DAYS_PER_400_YEARS) {
    tmbuf->tm_year += 400;
    b -= DAYS_PER_400_YEARS;
  }
#undef DAYS_PER_400_YEARS
#define DAYS_PER_100_YEARS ((100 * 365) + 24)
  while (b >= DAYS_PER_100_YEARS) {
    tmbuf->tm_year += 100;
    b -= DAYS_PER_100_YEARS;
    if (((tmbuf->tm_year / 100) & 3) ==
        0) // leap century divisable by 400 => add leap
      --b;
  }
#undef DAYS_PER_100_YEARS
#define DAYS_PER_32_YEARS ((32 * 365) + 8)
  while (b >= DAYS_PER_32_YEARS) {
    tmbuf->tm_year += 32;
    b -= DAYS_PER_32_YEARS;
  }
#undef DAYS_PER_32_YEARS
#define DAYS_PER_8_YEARS ((8 * 365) + 2)
  while (b >= DAYS_PER_8_YEARS) {
    tmbuf->tm_year += 8;
    b -= DAYS_PER_8_YEARS;
  }
#undef DAYS_PER_8_YEARS
#define DAYS_PER_4_YEARS ((4 * 365) + 1)
  while (b >= DAYS_PER_4_YEARS) {
    tmbuf->tm_year += 4;
    b -= DAYS_PER_4_YEARS;
  }
#undef DAYS_PER_4_YEARS
  while (b >= 365) {
    tmbuf->tm_year += 1;
    b -= 365;
    if ((tmbuf->tm_year & 3) == 0) { // leap year
      if (b > 0) {
        --b;
        continue;
      } else {
        b += 365;
        --tmbuf->tm_year;
        break;
      }
    }
  }
  b++; /* day 1 of the year is 1, not 0. */
  tmbuf->tm_yday = b;
  if ((tmbuf->tm_year & 3) == 1) {
    // regular year
    for (size_t i = 0; i < 12; i++) {
      if (b <= month_len[i])
        break;
      b -= month_len[i];
      ++tmbuf->tm_mon;
    }
  } else {
    // leap year
    for (size_t i = 12; i < 24; i++) {
      if (b <= month_len[i])
        break;
      b -= month_len[i];
      ++tmbuf->tm_mon;
    }
  }
  tmbuf->tm_mday = b;
  return tmbuf;
}

/** Writes an RFC 7231 date representation (HTTP date format) to target. */
size_t http_date2rfc7231(char *target, struct tm *tmbuf) {
  /* note: day of month is always 2 digits */
  char *pos = target;
  uint16_t tmp;
  pos[0] = FIO___DAY_NAMES[tmbuf->tm_wday][0];
  pos[1] = FIO___DAY_NAMES[tmbuf->tm_wday][1];
  pos[2] = FIO___DAY_NAMES[tmbuf->tm_wday][2];
  pos[3] = ',';
  pos[4] = ' ';
  pos += 5;
  tmp = tmbuf->tm_mday / 10;
  pos[0] = '0' + tmp;
  pos[1] = '0' + (tmbuf->tm_mday - (tmp * 10));
  pos += 2;
  *(pos++) = ' ';
  pos[0] = FIO___MONTH_NAMES[tmbuf->tm_mon][0];
  pos[1] = FIO___MONTH_NAMES[tmbuf->tm_mon][1];
  pos[2] = FIO___MONTH_NAMES[tmbuf->tm_mon][2];
  pos[3] = ' ';
  pos += 4;
  // write year.
  pos += fio_ltoa(pos, tmbuf->tm_year + 1900, 10);
  *(pos++) = ' ';
  tmp = tmbuf->tm_hour / 10;
  pos[0] = '0' + tmp;
  pos[1] = '0' + (tmbuf->tm_hour - (tmp * 10));
  pos[2] = ':';
  tmp = tmbuf->tm_min / 10;
  pos[3] = '0' + tmp;
  pos[4] = '0' + (tmbuf->tm_min - (tmp * 10));
  pos[5] = ':';
  tmp = tmbuf->tm_sec / 10;
  pos[6] = '0' + tmp;
  pos[7] = '0' + (tmbuf->tm_sec - (tmp * 10));
  pos += 8;
  pos[0] = ' ';
  pos[1] = FIO___GMT_STR[0];
  pos[2] = FIO___GMT_STR[1];
  pos[3] = FIO___GMT_STR[2];
  pos[4] = 0;
  pos += 4;
  return pos - target;
}
/** Writes an RFC 2109 date representation to target. */
size_t http_date2rfc2109(char *target, struct tm *tmbuf) {
  /* note: day of month is always 2 digits */
  char *pos = target;
  uint16_t tmp;
  pos[0] = FIO___DAY_NAMES[tmbuf->tm_wday][0];
  pos[1] = FIO___DAY_NAMES[tmbuf->tm_wday][1];
  pos[2] = FIO___DAY_NAMES[tmbuf->tm_wday][2];
  pos[3] = ',';
  pos[4] = ' ';
  pos += 5;
  tmp = tmbuf->tm_mday / 10;
  pos[0] = '0' + tmp;
  pos[1] = '0' + (tmbuf->tm_mday - (tmp * 10));
  pos += 2;
  *(pos++) = ' ';
  pos[0] = FIO___MONTH_NAMES[tmbuf->tm_mon][0];
  pos[1] = FIO___MONTH_NAMES[tmbuf->tm_mon][1];
  pos[2] = FIO___MONTH_NAMES[tmbuf->tm_mon][2];
  pos[3] = ' ';
  pos += 4;
  // write year.
  pos += fio_ltoa(pos, tmbuf->tm_year + 1900, 10);
  *(pos++) = ' ';
  tmp = tmbuf->tm_hour / 10;
  pos[0] = '0' + tmp;
  pos[1] = '0' + (tmbuf->tm_hour - (tmp * 10));
  pos[2] = ':';
  tmp = tmbuf->tm_min / 10;
  pos[3] = '0' + tmp;
  pos[4] = '0' + (tmbuf->tm_min - (tmp * 10));
  pos[5] = ':';
  tmp = tmbuf->tm_sec / 10;
  pos[6] = '0' + tmp;
  pos[7] = '0' + (tmbuf->tm_sec - (tmp * 10));
  pos += 8;
  *pos++ = ' ';
  *pos++ = '-';
  *pos++ = '0';
  *pos++ = '0';
  *pos++ = '0';
  *pos++ = '0';
  *pos = 0;
  return pos - target;
}

/** Writes an RFC 2822 date representation to target. */
size_t http_date2rfc2822(char *target, struct tm *tmbuf) {
  /* note: day of month is either 1 or 2 digits */
  char *pos = target;
  uint16_t tmp;
  pos[0] = FIO___DAY_NAMES[tmbuf->tm_wday][0];
  pos[1] = FIO___DAY_NAMES[tmbuf->tm_wday][1];
  pos[2] = FIO___DAY_NAMES[tmbuf->tm_wday][2];
  pos[3] = ',';
  pos[4] = ' ';
  pos += 5;
  if (tmbuf->tm_mday < 10) {
    *pos = '0' + tmbuf->tm_mday;
    ++pos;
  } else {
    tmp = tmbuf->tm_mday / 10;
    pos[0] = '0' + tmp;
    pos[1] = '0' + (tmbuf->tm_mday - (tmp * 10));
    pos += 2;
  }
  *(pos++) = '-';
  pos[0] = FIO___MONTH_NAMES[tmbuf->tm_mon][0];
  pos[1] = FIO___MONTH_NAMES[tmbuf->tm_mon][1];
  pos[2] = FIO___MONTH_NAMES[tmbuf->tm_mon][2];
  pos += 3;
  *(pos++) = '-';
  // write year.
  pos += fio_ltoa(pos, tmbuf->tm_year + 1900, 10);
  *(pos++) = ' ';
  tmp = tmbuf->tm_hour / 10;
  pos[0] = '0' + tmp;
  pos[1] = '0' + (tmbuf->tm_hour - (tmp * 10));
  pos[2] = ':';
  tmp = tmbuf->tm_min / 10;
  pos[3] = '0' + tmp;
  pos[4] = '0' + (tmbuf->tm_min - (tmp * 10));
  pos[5] = ':';
  tmp = tmbuf->tm_sec / 10;
  pos[6] = '0' + tmp;
  pos[7] = '0' + (tmbuf->tm_sec - (tmp * 10));
  pos += 8;
  pos[0] = ' ';
  pos[1] = FIO___GMT_STR[0];
  pos[2] = FIO___GMT_STR[1];
  pos[3] = FIO___GMT_STR[2];
  pos[4] = 0;
  pos += 4;
  return pos - target;
}

/* *****************************************************************************
HTTP URL decoding helper functions that might be used globally
***************************************************************************** */

/** Decodes a URL encoded string, no buffer overflow protection. */
ssize_t http_decode_url_unsafe(char *dest, const char *url_data) {
  char *pos = dest;
  while (*url_data) {
    if (*url_data == '+') {
      // decode space
      *(pos++) = ' ';
      ++url_data;
    } else if (*url_data == '%') {
      // decode hex value
      // this is a percent encoded value.
      if (hex2byte((uint8_t *)pos, (uint8_t *)&url_data[1]))
        return -1;
      pos++;
      url_data += 3;
    } else
      *(pos++) = *(url_data++);
  }
  *pos = 0;
  return pos - dest;
}

/** Decodes a URL encoded string (query / form data). */
ssize_t http_decode_url(char *dest, const char *url_data, size_t length) {
  char *pos = dest;
  const char *end = url_data + length;
  while (url_data < end) {
    if (*url_data == '+') {
      // decode space
      *(pos++) = ' ';
      ++url_data;
    } else if (*url_data == '%') {
      // decode hex value
      // this is a percent encoded value.
      if (hex2byte((uint8_t *)pos, (uint8_t *)&url_data[1]))
        return -1;
      pos++;
      url_data += 3;
    } else
      *(pos++) = *(url_data++);
  }
  *pos = 0;
  return pos - dest;
}

/** Decodes the "path" part of a request, no buffer overflow protection. */
ssize_t http_decode_path_unsafe(char *dest, const char *url_data) {
  char *pos = dest;
  while (*url_data) {
    if (*url_data == '%') {
      // decode hex value
      // this is a percent encoded value.
      if (hex2byte((uint8_t *)pos, (uint8_t *)&url_data[1]))
        return -1;
      pos++;
      url_data += 3;
    } else
      *(pos++) = *(url_data++);
  }
  *pos = 0;
  return pos - dest;
}

/**
 * Decodes the "path" part of an HTTP request, no buffer overflow protection.
 */
ssize_t http_decode_path(char *dest, const char *url_data, size_t length) {
  char *pos = dest;
  const char *end = url_data + length;
  while (url_data < end) {
    if (*url_data == '%') {
      // decode hex value
      // this is a percent encoded value.
      if (hex2byte((uint8_t *)pos, (uint8_t *)&url_data[1]))
        return -1;
      pos++;
      url_data += 3;
    } else
      *(pos++) = *(url_data++);
  }
  *pos = 0;
  return pos - dest;
}

#if DEBUG
void http_tests(void);
#endif
