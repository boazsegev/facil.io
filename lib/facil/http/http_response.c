/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "base64.h"
#include "http.h"
#include "http1_response.h"
#include "siphash.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
/* *****************************************************************************
Fallbacks
***************************************************************************** */

static http_response_s *fallback_http_response_create(http_request_s *request) {
  (void)request;
  return NULL;
}
static void fallback_http_response_dest(http_response_s *res) {
  (void)res;
  return;
}

/* *****************************************************************************
Initialization
***************************************************************************** */

/** Creates / allocates a protocol version's response object. */
http_response_s *http_response_create(http_request_s *request) {
  static http_response_s *(*const vtable[2])(http_request_s *) = {
      http1_response_create /* HTTP_V1 */,
      fallback_http_response_create /* HTTP_V2 */};
  return vtable[request->http_version](request);
}
/** Destroys the response object. No data is sent.*/
void http_response_destroy(http_response_s *response) {
  if (!response)
    return;
  static void (*const vtable[2])(http_response_s *) = {
      http1_response_destroy /* HTTP_V1 */,
      fallback_http_response_dest /* HTTP_V2 */};
  vtable[response->http_version](response);
}

/* we declare it in advance, because we reference it soon. */
static void http_response_log_finish(http_response_s *response);
/** Sends the data and destroys the response object.*/
void http_response_finish(http_response_s *response) {
  static void (*const vtable[2])(http_response_s *) = {
      http1_response_finish /* HTTP_V1 */,
      fallback_http_response_dest /* HTTP_V2 */};
  if (response->logged)
    http_response_log_finish(response);
  vtable[response->http_version](response);
}

/* *****************************************************************************
Writing data to the response object
***************************************************************************** */
#define is_num(c) ((c) >= '0' && (c) <= '9')
#define num_val(c) ((c)-48)

#define invalid_cookie_char(c)                                                 \
  ((c) < '!' || (c) > '~' || (c) == '=' || (c) == ' ' || (c) == ',' ||         \
   (c) == ';')

/**
If the header buffer is full or the headers were already sent (new headers
cannot be sent), the function will return -1.

On success, the function returns 0.
*/
int http_response_write_header_fn(http_response_s *response,
                                  http_header_s header) {
  static int (*const vtable[2])(http_response_s *, http_header_s) = {
      http1_response_write_header_fn /* HTTP_V1 */, NULL /* HTTP_V2 */,
  };
  if (!header.name || response->headers_sent)
    return -1;
  if (header.value && !header.value_len)
    header.value_len = strlen(header.value);
  if (header.name && !header.name_len)
    header.name_len = strlen(header.name);
  if (header.name_len == 4 && !strncasecmp(header.name, "Date", 4))
    response->date_written = 1;
  else if (header.name_len == 14 &&
           !strncasecmp(header.name, "content-length", 14))
    response->content_length_written = 1;
  else if (header.name_len == 13 &&
           !strncasecmp(header.name, "Last-Modified", 13))
    response->date_written = 1;
  else if (header.name_len == 10 &&
           !strncasecmp(header.name, "connection", 10)) {
    response->connection_written = 1;
    if (header.value_len == 5 && !strncasecmp(header.value, "close", 5))
      response->should_close = 1;
  }

  return vtable[response->http_version](response, header);
}

/**
Set / Delete a cookie using this helper function.

If the header buffer is full or the headers were already sent (new headers
cannot be sent), the function will return -1.

On success, the function returns 0.
*/
#undef http_response_set_cookie
int http_response_set_cookie(http_response_s *response, http_cookie_s cookie) {
  /* validate common requirements. */
  if (!cookie.name || response->headers_sent)
    return -1;
  ssize_t tmp = cookie.name_len;
  if (cookie.name_len) {
    do {
      tmp--;
      if (!cookie.name[tmp] || invalid_cookie_char(cookie.name[tmp]))
        goto error;
    } while (tmp);
  } else {
    while (cookie.name[cookie.name_len] &&
           !invalid_cookie_char(cookie.name[cookie.name_len]))
      cookie.name_len++;
    if (cookie.name[cookie.name_len])
      goto error;
  }
  if (cookie.value_len) {
    ssize_t tmp = cookie.value_len;
    do {
      tmp--;
      if (!cookie.value[tmp] || invalid_cookie_char(cookie.value[tmp]))
        goto error;
    } while (tmp);
  } else {
    while (cookie.value[cookie.value_len] &&
           !invalid_cookie_char(cookie.value[cookie.value_len]))
      cookie.value_len++;
    if (cookie.value[cookie.value_len])
      return -1;
  }

  static int (*const vtable[2])(http_response_s *, http_cookie_s) = {
      http1_response_set_cookie /* HTTP_V1 */, NULL /* HTTP_V2 */,
  };
  return vtable[response->http_version](response, cookie);
error:
  fprintf(stderr, "ERROR: Invalid cookie value cookie value character: %c\n",
          cookie.value[tmp]);
  return -1;
}

/**
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

The body will be copied to the server's outgoing buffer.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
int http_response_write_body(http_response_s *response, const char *body,
                             size_t length) {
  static int (*const vtable[2])(http_response_s *, const char *, size_t) = {
      http1_response_write_body /* HTTP_V1 */, NULL /* HTTP_V2 */,
  };
  if (!response->content_length)
    response->content_length = length;
  return vtable[response->http_version](response, body, length);
}

/**
Sends the headers (if they weren't previously sent) and writes the data to the
underlying socket.

If the connection was already closed, the function will return -1. On success,
the function returns 0.
*/
int http_response_sendfile(http_response_s *response, int source_fd,
                           off_t offset, size_t length) {
  static int (*const vtable[2])(http_response_s *, int, off_t, size_t) = {
      http1_response_sendfile /* HTTP_V1 */, NULL /* HTTP_V2 */,
  };
  if (!response->content_length)
    response->content_length = length;
  return vtable[response->http_version](response, source_fd, offset, length);
}
/**
Attempts to send the file requested using an **optional** response object (if
no response object is pointed to, a temporary response object will be
created).

This function will honor Ranged requests by setting the byte range
appropriately.

On failure, the function will return -1 (no response will be sent).

On success, the function returns 0.
*/
int http_response_sendfile2(http_response_s *response, http_request_s *request,
                            const char *file_path_safe, size_t path_safe_len,
                            const char *file_path_unsafe,
                            size_t path_unsafe_len, uint8_t log) {
  static char *HEAD = "HEAD";
  char buffer[64]; /* we'll need this a few times along the way */
  if (request == NULL || (file_path_safe == NULL && file_path_unsafe == NULL))
    return -1;

  if (file_path_safe && path_safe_len == 0)
    path_safe_len = strlen(file_path_safe);

  if (file_path_unsafe && path_unsafe_len == 0) {
    path_unsafe_len = strlen(file_path_unsafe);
    if (!path_unsafe_len)
      return -1;
  }

  const char *mime = NULL;
  const char *ext = NULL;
  int8_t should_free_response = 0;
  struct stat file_data = {.st_size = 0};
  // fprintf(stderr, "\n\noriginal request path: %s\n", req->path);
  // char *fname = malloc(path_safe_len + path_unsafe_len + 1 + 11);
  if ((path_safe_len + path_unsafe_len) >= (PATH_MAX - 1 - 11))
    return -1;
  char fname[path_safe_len + path_unsafe_len + (1 + 11 + 3)];
  // if (fname == NULL)
  //   return -1;
  if (file_path_safe)
    memcpy(fname, file_path_safe, path_safe_len);
  fname[path_safe_len] = 0;
  // if the last character is a '/', step back.
  if (file_path_unsafe) {
    if (fname[path_safe_len - 1] == '/' && file_path_unsafe[0] == '/')
      path_safe_len--;
    else if (fname[path_safe_len - 1] != '/' && file_path_unsafe[0] != '/')
      fname[path_safe_len++] = '/';
    ssize_t tmp = http_decode_path(fname + path_safe_len, file_path_unsafe,
                                   path_unsafe_len);
    if (tmp < 0)
      goto no_file;
    path_safe_len += tmp;
    if (fname[path_safe_len - 1] == '/') {
      memcpy(fname + path_safe_len, "index.html", 10);
      fname[path_safe_len += 10] = 0;
    }

    // scan path string for double dots (security - prevent path manipulation)
    // set the extention point value, while were doing so.
    tmp = 0;
    while (fname[tmp]) {
      if (fname[tmp] == '.')
        ext = fname + tmp;
      // return false if we found a "/.." or "/" in our string.
      if (fname[tmp++] == '/' &&
          ((fname[tmp++] == '.' && fname[tmp++] == '.') || fname[tmp] == '/'))
        goto no_file;
    }
  }
  // fprintf(stderr, "file name: %s\noriginal request path: %s\n", fname,
  //         req->path);
  // get file data (prevent folder access and get modification date)
  {
    http_header_s accept =
        http_request_header_find(request, "accept-encoding", 15);
    if (accept.data && strcasestr(accept.data, "gzip")) {
      buffer[0] = 1;
      fname[path_safe_len] = '.';
      fname[path_safe_len + 1] = 'g';
      fname[path_safe_len + 2] = 'z';
      fname[path_safe_len + 3] = 0;
      if (stat(fname, &file_data)) {
        buffer[0] = 0;
        fname[path_safe_len] = 0;
        if (stat(fname, &file_data))
          goto no_file;
      }
    } else {
      buffer[0] = 0;
      if (stat(fname, &file_data))
        goto no_file;
    }
  }
  // check that we have a file and not something else
  if (!S_ISREG(file_data.st_mode) && !S_ISLNK(file_data.st_mode))
    goto no_file;

  if (response == NULL) {
    should_free_response = 1;
    response = http_response_create(request);
    if (log)
      http_response_log_start(response);
  }
  // we have a file, time to handle response details.
  int file = open(fname, O_RDONLY);

  // free the allocated fname memory
  // free(fname);
  // fname = NULL;
  if (file == -1) {
    goto no_fd_available;
  }

  if (buffer[0]) {
    fname[path_safe_len] = 0;
    http_response_write_header(response, .name = "Content-Encoding",
                               .name_len = 16, .value = "gzip", .value_len = 4);
  }

  // get the mime type (we have an ext pointer and the string isn't empty)
  if (ext && ext[1]) {
    mime = http_response_ext2mime(ext + 1);
    if (mime) {
      http_response_write_header(response, .name = "Content-Type",
                                 .name_len = 12, .value = mime);
    }
  }
  /* add ETag */
  uint64_t sip = (uint64_t)file_data.st_size;
  sip ^= (uint64_t)file_data.st_mtime;
  sip = siphash24(&sip, sizeof(uint64_t), SIPHASH_DEFAULT_KEY);
  bscrypt_base64_encode(buffer, (void *)&sip, 8);
  http_response_write_header(response, .name = "ETag", .name_len = 4,
                             .value = buffer, .value_len = 12);

  response->last_modified = file_data.st_mtime;
  http_response_write_header(response, .name = "Cache-Control", .name_len = 13,
                             .value = "max-age=3600", .value_len = 12);

  /* check etag */
  if ((ext = http_request_header_find(request, "if-none-match", 13).value) &&
      memcmp(ext, buffer, 12) == 0) {
    /* send back 304 */
    response->status = 304;
    close(file);
    http_response_finish(response);
    return 0;
  }

  // Range handling
  if ((ext = http_request_header_find(request, "range", 5).value) &&
      (ext[0] | 32) == 'b' && (ext[1] | 32) == 'y' && (ext[2] | 32) == 't' &&
      (ext[3] | 32) == 'e' && (ext[4] | 32) == 's' && (ext[5] | 32) == '=') {
    // ext holds the first range, starting on index 6 i.e. RANGE: bytes=0-1
    // "HTTP/1.1 206 Partial content\r\n"
    // "Accept-Ranges: bytes\r\n"
    // "Content-Range: bytes %lu-%lu/%lu\r\n"
    // fprintf(stderr, "Got a range request %s\n", ext);
    size_t start = 0, finish = 0;
    ext = ext + 6;
    while (is_num(*ext)) {
      start = start * 10;
      start += num_val(*ext);
      ext++;
    }
    // fprintf(stderr, "Start: %lu / %lld\n", start, file_data.st_size);
    if ((off_t)start >= file_data.st_size - 1)
      goto invalid_range;
    ext++;
    while (is_num(*ext)) {
      finish = finish * 10;
      finish += num_val(*ext);
      ext++;
    }
    // going to the EOF (big chunk or EOL requested) - send as file
    if ((off_t)finish >= file_data.st_size)
      finish = file_data.st_size;
    char *pos = buffer + 6;
    memcpy(buffer, "bytes ", 6);
    pos += http_ul2a(pos, start);
    *(pos++) = '-';
    pos += http_ul2a(pos, finish);
    *(pos++) = '/';
    pos += http_ul2a(pos, file_data.st_size);
    http_response_write_header(response, .name = "Content-Range",
                               .name_len = 13, .value = buffer,
                               .value_len = pos - buffer);
    response->status = 206;
    http_response_write_header(response, .name = "Accept-Ranges",
                               .name_len = 13, .value = "bytes",
                               .value_len = 5);

    if (*((uint32_t *)request->method) == *((uint32_t *)HEAD)) {
      response->content_length = 0;
      close(file);
      http_response_finish(response);
      return 0;
    }

    http_response_sendfile(response, file, start, finish - start + 1);
    http_response_finish(response);
    return 0;
  }

invalid_range:
  http_response_write_header(response, .name = "Accept-Ranges", .name_len = 13,
                             .value = "none", .value_len = 4);

  if (*((uint32_t *)request->method) == *((uint32_t *)HEAD)) {
    response->content_length = 0;
    close(file);
    http_response_finish(response);
    return 0;
  }

  http_response_sendfile(response, file, 0, file_data.st_size);
  http_response_finish(response);
  return 0;

no_fd_available:
  response->status = 503;
  const char *body = http_response_status_str(503);
  http_response_write_body(response, body, strlen(body));
  http_response_finish(response);

no_file:
  if (should_free_response && response)
    http_response_destroy(response);
  // free(fname);
  return -1;
}
/* *****************************************************************************
Logging
***************************************************************************** */

#ifdef RUSAGE_SELF
static const size_t CLOCK_RESOLUTION = 1000; /* in miliseconds */
static size_t get_clock_mili(void) {
  struct rusage rusage;
  getrusage(RUSAGE_SELF, &rusage);
  return ((rusage.ru_utime.tv_sec + rusage.ru_stime.tv_sec) * 1000000) +
         (rusage.ru_utime.tv_usec + rusage.ru_stime.tv_usec);
}
#elif defined CLOCKS_PER_SEC
#define get_clock_mili() (size_t) clock()
#define CLOCK_RESOLUTION (CLOCKS_PER_SEC / 1000)
#else
#define get_clock_mili() 0
#define CLOCK_RESOLUTION 1
#endif

/**
Starts counting miliseconds for log results.
*/
void http_response_log_start(http_response_s *response) {
  response->clock_start = get_clock_mili();
  response->logged = 1;
}
/**
prints out the log to stderr.
*/
static void http_response_log_finish(http_response_s *response) {
#define HTTP_REQUEST_LOG_LIMIT 128 /* Log message length limit */
  char buffer[HTTP_REQUEST_LOG_LIMIT];
  http_request_s *request = response->request;
  intptr_t bytes_sent = response->content_length;

  size_t mili =
      response->logged
          ? ((get_clock_mili() - response->clock_start) / CLOCK_RESOLUTION)
          : 0;

  // TODO Guess IP address from headers (forwarded) where possible
  sock_peer_addr_s addrinfo = sock_peer_addr(response->fd);

  size_t pos = 0;
  if (addrinfo.addrlen) {
    if (inet_ntop(
            addrinfo.addr->sa_family,
            addrinfo.addr->sa_family == AF_INET
                ? (void *)&((struct sockaddr_in *)addrinfo.addr)->sin_addr
                : (void *)&((struct sockaddr_in6 *)addrinfo.addr)->sin6_addr,
            buffer, 128))
      pos = strlen(buffer);
    // pos = addrinfo.addr->sa_family == AF_INET ?: fmt_ip6()
  }
  if (pos == 0) {
    memcpy(buffer, "[unknown]", 9);
    pos = 9;
  }
  memcpy(buffer + pos, " - - [", 6);
  pos += 6;
  pos += http_time2str(buffer + pos, facil_last_tick());
  buffer[pos++] = ']';
  buffer[pos++] = ' ';
  buffer[pos++] = '"';
  // limit method to 10 chars
  if (request->method_len <= 10) {
    memcpy(buffer + pos, request->method, request->method_len);
    pos += request->method_len;
  } else {
    const char *j = request->method;
    // copy first 7 chars
    while (j < request->method + 7)
      buffer[pos++] = *(j++);
    // add three dots.
    buffer[pos++] = '.';
    buffer[pos++] = '.';
    buffer[pos++] = '.';
  }
  buffer[pos++] = ' ';
  // limit path to 24 chars
  if (request->path_len <= 24) {
    memcpy(buffer + pos, request->path, request->path_len);
    pos += request->path_len;
  } else {
    const char *j = request->path;
    // copy first 7 chars
    while (j < request->path + 21)
      buffer[pos++] = *(j++);
    // add three dots.
    buffer[pos++] = '.';
    buffer[pos++] = '.';
    buffer[pos++] = '.';
  }
  buffer[pos++] = ' ';
  // limit version to 10 chars
  if (request->version_len <= 10) {
    memcpy(buffer + pos, request->version, request->version_len);
    pos += request->version_len;
  } else {
    const char *j = request->version;
    // copy first 7 chars
    while (j < request->version + 7)
      buffer[pos++] = *(j++);
    // add three dots.
    buffer[pos++] = '.';
    buffer[pos++] = '.';
    buffer[pos++] = '.';
  }
  buffer[pos++] = '"';
  buffer[pos++] = ' ';
  pos += http_ul2a(buffer + pos, response->status > 0 && response->status < 1000
                                     ? response->status
                                     : 0);

  buffer[pos++] = ' ';
  if (bytes_sent > 0)
    pos += http_ul2a(buffer + pos, bytes_sent);
  else {
    buffer[pos++] = '-';
    buffer[pos++] = '-';
  }
  if (response->logged) {
    buffer[pos++] = ' ';
    pos += http_ul2a(buffer + pos, mili);
    buffer[pos++] = 'm';
    buffer[pos++] = 's';
  }
  buffer[pos++] = '\n';
  response->logged = 0;
  fwrite(buffer, 1, pos, stderr);
}
/* *****************************************************************************
List matching (status + mime-type)
*****************************************************************************
*/

/** Gets a response status, as a string */
const char *http_response_status_str(uint16_t status) {
  static struct {
    int i_status;
    char *s_status;
  } List[] = {{200, "OK"},
              {301, "Moved Permanently"},
              {302, "Found"},
              {100, "Continue"},
              {101, "Switching Protocols"},
              {403, "Forbidden"},
              {404, "Not Found"},
              {400, "Bad Request"},
              {500, "Internal Server Error"},
              {501, "Not Implemented"},
              {502, "Bad Gateway"},
              {503, "Service Unavailable"},
              {102, "Processing"},
              {201, "Created"},
              {202, "Accepted"},
              {203, "Non-Authoritative Information"},
              {204, "No Content"},
              {205, "Reset Content"},
              {206, "Partial Content"},
              {207, "Multi-Status"},
              {208, "Already Reported"},
              {226, "IM Used"},
              {300, "Multiple Choices"},
              {303, "See Other"},
              {304, "Not Modified"},
              {305, "Use Proxy"},
              {306, "(Unused)	"},
              {307, "Temporary Redirect"},
              {308, "Permanent Redirect"},
              {401, "Unauthorized"},
              {402, "Payment Required"},
              {405, "Method Not Allowed"},
              {406, "Not Acceptable"},
              {407, "Proxy Authentication Required"},
              {408, "Request Timeout"},
              {409, "Conflict"},
              {410, "Gone"},
              {411, "Length Required"},
              {412, "Precondition Failed"},
              {413, "Payload Too Large"},
              {414, "URI Too Long"},
              {415, "Unsupported Media Type"},
              {416, "Range Not Satisfiable"},
              {417, "Expectation Failed"},
              {421, "Misdirected Request"},
              {422, "Unprocessable Entity"},
              {423, "Locked"},
              {424, "Failed Dependency"},
              {425, "Unassigned"},
              {426, "Upgrade Required"},
              {427, "Unassigned"},
              {428, "Precondition Required"},
              {429, "Too Many Requests"},
              {430, "Unassigned"},
              {431, "Request Header Fields Too Large"},
              {504, "Gateway Timeout"},
              {505, "HTTP Version Not Supported"},
              {506, "Variant Also Negotiates"},
              {507, "Insufficient Storage"},
              {508, "Loop Detected"},
              {509, "Unassigned"},
              {510, "Not Extended"},
              {511, "Network Authentication Required"},
              {0, 0}};
  int pos = 0;
  while (List[pos].i_status) {
    if (List[pos].i_status == status)
      return List[pos].s_status;
    pos++;
  }
  return NULL;
}

/** Gets the mime-type string (C string) associated with the file extension.
 */
const char *http_response_ext2mime(const char *ext) {
  static struct {
    char ext[12];
    char *mime;
  } List[] = {
      {"123", "application/vnd.lotus-1-2-3"},
      {"3dml", "text/vnd.in3d.3dml"},
      {"3ds", "image/x-3ds"},
      {"3g2", "video/3gpp2"},
      {"3gp", "video/3gpp"},
      {"7z", "application/x-7z-compressed"},
      {"aab", "application/x-authorware-bin"},
      {"aac", "audio/x-aac"},
      {"aam", "application/x-authorware-map"},
      {"aas", "application/x-authorware-seg"},
      {"abw", "application/x-abiword"},
      {"ac", "application/pkix-attr-cert"},
      {"acc", "application/vnd.americandynamics.acc"},
      {"ace", "application/x-ace-compressed"},
      {"acu", "application/vnd.acucobol"},
      {"acutc", "application/vnd.acucorp"},
      {"adp", "audio/adpcm"},
      {"aep", "application/vnd.audiograph"},
      {"afm", "application/x-font-type1"},
      {"afp", "application/vnd.ibm.modcap"},
      {"ahead", "application/vnd.ahead.space"},
      {"ai", "application/postscript"},
      {"aif", "audio/x-aiff"},
      {"aifc", "audio/x-aiff"},
      {"aiff", "audio/x-aiff"},
      {"air", "application/vnd.adobe.air-application-installer-package+zip"},
      {"ait", "application/vnd.dvb.ait"},
      {"ami", "application/vnd.amiga.ami"},
      {"apk", "application/vnd.android.package-archive"},
      {"appcache", "text/cache-manifest"},
      {"application", "application/x-ms-application"},
      {
          "pptx",
          "application/"
          "vnd.openxmlformats-officedocument.presentationml.presentation",
      },
      {"apr", "application/vnd.lotus-approach"},
      {"arc", "application/x-freearc"},
      {"asc", "application/pgp-signature"},
      {"asf", "video/x-ms-asf"},
      {"asm", "text/x-asm"},
      {"aso", "application/vnd.accpac.simply.aso"},
      {"asx", "video/x-ms-asf"},
      {"atc", "application/vnd.acucorp"},
      {"atom", "application/atom+xml"},
      {"atomcat", "application/atomcat+xml"},
      {"atomsvc", "application/atomsvc+xml"},
      {"atx", "application/vnd.antix.game-component"},
      {"au", "audio/basic"},
      {"avi", "video/x-msvideo"},
      {"aw", "application/applixware"},
      {"azf", "application/vnd.airzip.filesecure.azf"},
      {"azs", "application/vnd.airzip.filesecure.azs"},
      {"azw", "application/vnd.amazon.ebook"},
      {"bat", "application/x-msdownload"},
      {"bcpio", "application/x-bcpio"},
      {"bdf", "application/x-font-bdf"},
      {"bdm", "application/vnd.syncml.dm+wbxml"},
      {"bed", "application/vnd.realvnc.bed"},
      {"bh2", "application/vnd.fujitsu.oasysprs"},
      {"bin", "application/octet-stream"},
      {"blb", "application/x-blorb"},
      {"blorb", "application/x-blorb"},
      {"bmi", "application/vnd.bmi"},
      {"bmp", "image/bmp"},
      {"book", "application/vnd.framemaker"},
      {"box", "application/vnd.previewsystems.box"},
      {"boz", "application/x-bzip2"},
      {"bpk", "application/octet-stream"},
      {"btif", "image/prs.btif"},
      {"bz", "application/x-bzip"},
      {"bz2", "application/x-bzip2"},
      {"c", "text/x-c"},
      {"c11amc", "application/vnd.cluetrust.cartomobile-config"},
      {"c11amz", "application/vnd.cluetrust.cartomobile-config-pkg"},
      {"c4d", "application/vnd.clonk.c4group"},
      {"c4f", "application/vnd.clonk.c4group"},
      {"c4g", "application/vnd.clonk.c4group"},
      {"c4p", "application/vnd.clonk.c4group"},
      {"c4u", "application/vnd.clonk.c4group"},
      {"cab", "application/vnd.ms-cab-compressed"},
      {"caf", "audio/x-caf"},
      {"cap", "application/vnd.tcpdump.pcap"},
      {"car", "application/vnd.curl.car"},
      {"cat", "application/vnd.ms-pki.seccat"},
      {"cb7", "application/x-cbr"},
      {"cba", "application/x-cbr"},
      {"cbr", "application/x-cbr"},
      {"cbt", "application/x-cbr"},
      {"cbz", "application/x-cbr"},
      {"cc", "text/x-c"},
      {"cct", "application/x-director"},
      {"ccxml", "application/ccxml+xml"},
      {"cdbcmsg", "application/vnd.contact.cmsg"},
      {"cdf", "application/x-netcdf"},
      {"cdkey", "application/vnd.mediastation.cdkey"},
      {"cdmia", "application/cdmi-capability"},
      {"cdmic", "application/cdmi-container"},
      {"cdmid", "application/cdmi-domain"},
      {"cdmio", "application/cdmi-object"},
      {"cdmiq", "application/cdmi-queue"},
      {"cdx", "chemical/x-cdx"},
      {"cdxml", "application/vnd.chemdraw+xml"},
      {"cdy", "application/vnd.cinderella"},
      {"cer", "application/pkix-cert"},
      {"cfs", "application/x-cfs-compressed"},
      {"cgm", "image/cgm"},
      {"chat", "application/x-chat"},
      {"chm", "application/vnd.ms-htmlhelp"},
      {"chrt", "application/vnd.kde.kchart"},
      {"cif", "chemical/x-cif"},
      {"cii", "application/vnd.anser-web-certificate-issue-initiation"},
      {"cil", "application/vnd.ms-artgalry"},
      {"cla", "application/vnd.claymore"},
      {"class", "application/java-vm"},
      {"clkk", "application/vnd.crick.clicker.keyboard"},
      {"clkp", "application/vnd.crick.clicker.palette"},
      {"clkt", "application/vnd.crick.clicker.template"},
      {"clkw", "application/vnd.crick.clicker.wordbank"},
      {"clkx", "application/vnd.crick.clicker"},
      {"clp", "application/x-msclip"},
      {"cmc", "application/vnd.cosmocaller"},
      {"cmdf", "chemical/x-cmdf"},
      {"cml", "chemical/x-cml"},
      {"cmp", "application/vnd.yellowriver-custom-menu"},
      {"cmx", "image/x-cmx"},
      {"cod", "application/vnd.rim.cod"},
      {"com", "application/x-msdownload"},
      {"conf", "text/plain"},
      {"cpio", "application/x-cpio"},
      {"cpp", "text/x-c"},
      {"cpt", "application/mac-compactpro"},
      {"crd", "application/x-mscardfile"},
      {"crl", "application/pkix-crl"},
      {"crt", "application/x-x509-ca-cert"},
      {"cryptonote", "application/vnd.rig.cryptonote"},
      {"csh", "application/x-csh"},
      {"csml", "chemical/x-csml"},
      {"csp", "application/vnd.commonspace"},
      {"css", "text/css"},
      {"cst", "application/x-director"},
      {"csv", "text/csv"},
      {"cu", "application/cu-seeme"},
      {"curl", "text/vnd.curl"},
      {"cww", "application/prs.cww"},
      {"cxt", "application/x-director"},
      {"cxx", "text/x-c"},
      {"dae", "model/vnd.collada+xml"},
      {"daf", "application/vnd.mobius.daf"},
      {"dart", "application/vnd.dart"},
      {"dataless", "application/vnd.fdsn.seed"},
      {"davmount", "application/davmount+xml"},
      {"dbk", "application/docbook+xml"},
      {"dcr", "application/x-director"},
      {"dcurl", "text/vnd.curl.dcurl"},
      {"dd2", "application/vnd.oma.dd2+xml"},
      {"ddd", "application/vnd.fujixerox.ddd"},
      {"deb", "application/x-debian-package"},
      {"def", "text/plain"},
      {"deploy", "application/octet-stream"},
      {"der", "application/x-x509-ca-cert"},
      {"dfac", "application/vnd.dreamfactory"},
      {"dgc", "application/x-dgc-compressed"},
      {"dic", "text/x-c"},
      {"dir", "application/x-director"},
      {"dis", "application/vnd.mobius.dis"},
      {"dist", "application/octet-stream"},
      {"distz", "application/octet-stream"},
      {"djv", "image/vnd.djvu"},
      {"djvu", "image/vnd.djvu"},
      {"dll", "application/x-msdownload"},
      {"dmg", "application/x-apple-diskimage"},
      {"dmp", "application/vnd.tcpdump.pcap"},
      {"dms", "application/octet-stream"},
      {"dna", "application/vnd.dna"},
      {"doc", "application/msword"},
      {"docm", "application/vnd.ms-word.document.macroenabled.12"},
      {"docx", "application/"
               "vnd.openxmlformats-officedocument.wordprocessingml.document"},
      {"dot", "application/msword"},
      {"dotm", "application/vnd.ms-word.template.macroenabled.12"},
      {"dotx", "application/"
               "vnd.openxmlformats-officedocument.wordprocessingml.template"},
      {"dp", "application/vnd.osgi.dp"},
      {"dpg", "application/vnd.dpgraph"},
      {"dra", "audio/vnd.dra"},
      {"dsc", "text/prs.lines.tag"},
      {"dssc", "application/dssc+der"},
      {"dtb", "application/x-dtbook+xml"},
      {"dtd", "application/xml-dtd"},
      {"dts", "audio/vnd.dts"},
      {"dtshd", "audio/vnd.dts.hd"},
      {"dump", "application/octet-stream"},
      {"dvb", "video/vnd.dvb.file"},
      {"dvi", "application/x-dvi"},
      {"dwf", "model/vnd.dwf"},
      {"dwg", "image/vnd.dwg"},
      {"dxf", "image/vnd.dxf"},
      {"dxp", "application/vnd.spotfire.dxp"},
      {"dxr", "application/x-director"},
      {"ecelp4800", "audio/vnd.nuera.ecelp4800"},
      {"ecelp7470", "audio/vnd.nuera.ecelp7470"},
      {"ecelp9600", "audio/vnd.nuera.ecelp9600"},
      {"ecma", "application/ecmascript"},
      {"edm", "application/vnd.novadigm.edm"},
      {"edx", "application/vnd.novadigm.edx"},
      {"efif", "application/vnd.picsel"},
      {"ei6", "application/vnd.pg.osasli"},
      {"elc", "application/octet-stream"},
      {"emf", "application/x-msmetafile"},
      {"eml", "message/rfc822"},
      {"emma", "application/emma+xml"},
      {"emz", "application/x-msmetafile"},
      {"eol", "audio/vnd.digital-winds"},
      {"eot", "application/vnd.ms-fontobject"},
      {"eps", "application/postscript"},
      {"epub", "application/epub+zip"},
      {"es3", "application/vnd.eszigno3+xml"},
      {"esa", "application/vnd.osgi.subsystem"},
      {"esf", "application/vnd.epson.esf"},
      {"et3", "application/vnd.eszigno3+xml"},
      {"etx", "text/x-setext"},
      {"eva", "application/x-eva"},
      {"evy", "application/x-envoy"},
      {"exe", "application/x-msdownload"},
      {"exi", "application/exi"},
      {"ext", "application/vnd.novadigm.ext"},
      {"ez", "application/andrew-inset"},
      {"ez2", "application/vnd.ezpix-album"},
      {"ez3", "application/vnd.ezpix-package"},
      {"f", "text/x-fortran"},
      {"f4v", "video/x-f4v"},
      {"f77", "text/x-fortran"},
      {"f90", "text/x-fortran"},
      {"fbs", "image/vnd.fastbidsheet"},
      {"fcdt", "application/vnd.adobe.formscentral.fcdt"},
      {"fcs", "application/vnd.isac.fcs"},
      {"fdf", "application/vnd.fdf"},
      {"fe_launch", "application/vnd.denovo.fcselayout-link"},
      {"fg5", "application/vnd.fujitsu.oasysgp"},
      {"fgd", "application/x-director"},
      {"fh", "image/x-freehand"},
      {"fh4", "image/x-freehand"},
      {"fh5", "image/x-freehand"},
      {"fh7", "image/x-freehand"},
      {"fhc", "image/x-freehand"},
      {"fig", "application/x-xfig"},
      {"flac", "audio/x-flac"},
      {"fli", "video/x-fli"},
      {"flo", "application/vnd.micrografx.flo"},
      {"flv", "video/x-flv"},
      {"flw", "application/vnd.kde.kivio"},
      {"flx", "text/vnd.fmi.flexstor"},
      {"fly", "text/vnd.fly"},
      {"fm", "application/vnd.framemaker"},
      {"fnc", "application/vnd.frogans.fnc"},
      {"for", "text/x-fortran"},
      {"fpx", "image/vnd.fpx"},
      {"frame", "application/vnd.framemaker"},
      {"fsc", "application/vnd.fsc.weblaunch"},
      {"fst", "image/vnd.fst"},
      {"ftc", "application/vnd.fluxtime.clip"},
      {"fti", "application/vnd.anser-web-funds-transfer-initiation"},
      {"fvt", "video/vnd.fvt"},
      {"fxp", "application/vnd.adobe.fxp"},
      {"fxpl", "application/vnd.adobe.fxp"},
      {"fzs", "application/vnd.fuzzysheet"},
      {"g2w", "application/vnd.geoplan"},
      {"g3", "image/g3fax"},
      {"g3w", "application/vnd.geospace"},
      {"gac", "application/vnd.groove-account"},
      {"gam", "application/x-tads"},
      {"gbr", "application/rpki-ghostbusters"},
      {"gca", "application/x-gca-compressed"},
      {"gdl", "model/vnd.gdl"},
      {"geo", "application/vnd.dynageo"},
      {"gex", "application/vnd.geometry-explorer"},
      {"ggb", "application/vnd.geogebra.file"},
      {"ggt", "application/vnd.geogebra.tool"},
      {"ghf", "application/vnd.groove-help"},
      {"gif", "image/gif"},
      {"gim", "application/vnd.groove-identity-message"},
      {"gml", "application/gml+xml"},
      {"gmx", "application/vnd.gmx"},
      {"gnumeric", "application/x-gnumeric"},
      {"gph", "application/vnd.flographit"},
      {"gpx", "application/gpx+xml"},
      {"gqf", "application/vnd.grafeq"},
      {"gqs", "application/vnd.grafeq"},
      {"gram", "application/srgs"},
      {"gramps", "application/x-gramps-xml"},
      {"gre", "application/vnd.geometry-explorer"},
      {"grv", "application/vnd.groove-injector"},
      {"grxml", "application/srgs+xml"},
      {"gsf", "application/x-font-ghostscript"},
      {"gtar", "application/x-gtar"},
      {"gtm", "application/vnd.groove-tool-message"},
      {"gtw", "model/vnd.gtw"},
      {"gv", "text/vnd.graphviz"},
      {"gxf", "application/gxf"},
      {"gxt", "application/vnd.geonext"},
      {"h", "text/x-c"},
      {"h261", "video/h261"},
      {"h263", "video/h263"},
      {"h264", "video/h264"},
      {"hal", "application/vnd.hal+xml"},
      {"hbci", "application/vnd.hbci"},
      {"hdf", "application/x-hdf"},
      {"hh", "text/x-c"},
      {"hlp", "application/winhlp"},
      {"hpgl", "application/vnd.hp-hpgl"},
      {"hpid", "application/vnd.hp-hpid"},
      {"hps", "application/vnd.hp-hps"},
      {"hqx", "application/mac-binhex40"},
      {"htke", "application/vnd.kenameaapp"},
      {"htm", "text/html"},
      {"html", "text/html"},
      {"hvd", "application/vnd.yamaha.hv-dic"},
      {"hvp", "application/vnd.yamaha.hv-voice"},
      {"hvs", "application/vnd.yamaha.hv-script"},
      {"i2g", "application/vnd.intergeo"},
      {"icc", "application/vnd.iccprofile"},
      {"ice", "x-conference/x-cooltalk"},
      {"icm", "application/vnd.iccprofile"},
      {"ico", "image/x-icon"},
      {"ics", "text/calendar"},
      {"ief", "image/ief"},
      {"ifb", "text/calendar"},
      {"ifm", "application/vnd.shana.informed.formdata"},
      {"iges", "model/iges"},
      {"igl", "application/vnd.igloader"},
      {"igm", "application/vnd.insors.igm"},
      {"igs", "model/iges"},
      {"igx", "application/vnd.micrografx.igx"},
      {"iif", "application/vnd.shana.informed.interchange"},
      {"imp", "application/vnd.accpac.simply.imp"},
      {"ims", "application/vnd.ms-ims"},
      {"in", "text/plain"},
      {"ink", "application/inkml+xml"},
      {"inkml", "application/inkml+xml"},
      {"install", "application/x-install-instructions"},
      {"iota", "application/vnd.astraea-software.iota"},
      {"ipfix", "application/ipfix"},
      {"ipk", "application/vnd.shana.informed.package"},
      {"irm", "application/vnd.ibm.rights-management"},
      {"irp", "application/vnd.irepository.package+xml"},
      {"iso", "application/x-iso9660-image"},
      {"itp", "application/vnd.shana.informed.formtemplate"},
      {"ivp", "application/vnd.immervision-ivp"},
      {"ivu", "application/vnd.immervision-ivu"},
      {"jad", "text/vnd.sun.j2me.app-descriptor"},
      {"jam", "application/vnd.jam"},
      {"jar", "application/java-archive"},
      {"java", "text/x-java-source"},
      {"jisp", "application/vnd.jisp"},
      {"jlt", "application/vnd.hp-jlyt"},
      {"jnlp", "application/x-java-jnlp-file"},
      {"joda", "application/vnd.joost.joda-archive"},
      {"jpe", "image/jpeg"},
      {"jpeg", "image/jpeg"},
      {"jpg", "image/jpeg"},
      {"jpgm", "video/jpm"},
      {"jpgv", "video/jpeg"},
      {"jpm", "video/jpm"},
      {"js", "application/javascript"},
      {"json", "application/json"},
      {"jsonml", "application/jsonml+json"},
      {"kar", "audio/midi"},
      {"karbon", "application/vnd.kde.karbon"},
      {"kfo", "application/vnd.kde.kformula"},
      {"kia", "application/vnd.kidspiration"},
      {"kml", "application/vnd.google-earth.kml+xml"},
      {"kmz", "application/vnd.google-earth.kmz"},
      {"kne", "application/vnd.kinar"},
      {"knp", "application/vnd.kinar"},
      {"kon", "application/vnd.kde.kontour"},
      {"kpr", "application/vnd.kde.kpresenter"},
      {"kpt", "application/vnd.kde.kpresenter"},
      {"kpxx", "application/vnd.ds-keypoint"},
      {"ksp", "application/vnd.kde.kspread"},
      {"ktr", "application/vnd.kahootz"},
      {"ktx", "image/ktx"},
      {"ktz", "application/vnd.kahootz"},
      {"kwd", "application/vnd.kde.kword"},
      {"kwt", "application/vnd.kde.kword"},
      {"lasxml", "application/vnd.las.las+xml"},
      {"latex", "application/x-latex"},
      {"lbd", "application/vnd.llamagraphics.life-balance.desktop"},
      {"lbe", "application/vnd.llamagraphics.life-balance.exchange+xml"},
      {"les", "application/vnd.hhe.lesson-player"},
      {"lha", "application/x-lzh-compressed"},
      {"link66", "application/vnd.route66.link66+xml"},
      {"list", "text/plain"},
      {"list3820", "application/vnd.ibm.modcap"},
      {"listafp", "application/vnd.ibm.modcap"},
      {"lnk", "application/x-ms-shortcut"},
      {"log", "text/plain"},
      {"lostxml", "application/lost+xml"},
      {"lrf", "application/octet-stream"},
      {"lrm", "application/vnd.ms-lrm"},
      {"ltf", "application/vnd.frogans.ltf"},
      {"lvp", "audio/vnd.lucent.voice"},
      {"lwp", "application/vnd.lotus-wordpro"},
      {"lzh", "application/x-lzh-compressed"},
      {"m13", "application/x-msmediaview"},
      {"m14", "application/x-msmediaview"},
      {"m1v", "video/mpeg"},
      {"m21", "application/mp21"},
      {"m2a", "audio/mpeg"},
      {"m2v", "video/mpeg"},
      {"m3a", "audio/mpeg"},
      {"m3u", "audio/x-mpegurl"},
      {"m3u8", "application/vnd.apple.mpegurl"},
      {"m4a", "audio/mp4"},
      {"m4u", "video/vnd.mpegurl"},
      {"m4v", "video/x-m4v"},
      {"ma", "application/mathematica"},
      {"mads", "application/mads+xml"},
      {"mag", "application/vnd.ecowin.chart"},
      {"maker", "application/vnd.framemaker"},
      {"man", "text/troff"},
      {"mar", "application/octet-stream"},
      {"mathml", "application/mathml+xml"},
      {"mb", "application/mathematica"},
      {"mbk", "application/vnd.mobius.mbk"},
      {"mbox", "application/mbox"},
      {"mc1", "application/vnd.medcalcdata"},
      {"mcd", "application/vnd.mcd"},
      {"mcurl", "text/vnd.curl.mcurl"},
      {"mdb", "application/x-msaccess"},
      {"mdi", "image/vnd.ms-modi"},
      {"me", "text/troff"},
      {"mesh", "model/mesh"},
      {"meta4", "application/metalink4+xml"},
      {"metalink", "application/metalink+xml"},
      {"mets", "application/mets+xml"},
      {"mfm", "application/vnd.mfmp"},
      {"mft", "application/rpki-manifest"},
      {"mgp", "application/vnd.osgeo.mapguide.package"},
      {"mgz", "application/vnd.proteus.magazine"},
      {"mid", "audio/midi"},
      {"midi", "audio/midi"},
      {"mie", "application/x-mie"},
      {"mif", "application/vnd.mif"},
      {"mime", "message/rfc822"},
      {"mj2", "video/mj2"},
      {"mjp2", "video/mj2"},
      {"mk3d", "video/x-matroska"},
      {"mka", "audio/x-matroska"},
      {"mks", "video/x-matroska"},
      {"mkv", "video/x-matroska"},
      {"mlp", "application/vnd.dolby.mlp"},
      {"mmd", "application/vnd.chipnuts.karaoke-mmd"},
      {"mmf", "application/vnd.smaf"},
      {"mmr", "image/vnd.fujixerox.edmics-mmr"},
      {"mng", "video/x-mng"},
      {"mny", "application/x-msmoney"},
      {"mobi", "application/x-mobipocket-ebook"},
      {"mods", "application/mods+xml"},
      {"mov", "video/quicktime"},
      {"movie", "video/x-sgi-movie"},
      {"mp2", "audio/mpeg"},
      {"mp21", "application/mp21"},
      {"mp2a", "audio/mpeg"},
      {"mp3", "audio/mpeg"},
      {"mp4", "video/mp4"},
      {"mp4a", "audio/mp4"},
      {"mp4s", "application/mp4"},
      {"mp4v", "video/mp4"},
      {"mpc", "application/vnd.mophun.certificate"},
      {"mpe", "video/mpeg"},
      {"mpeg", "video/mpeg"},
      {"mpg", "video/mpeg"},
      {"mpg4", "video/mp4"},
      {"mpga", "audio/mpeg"},
      {"mpkg", "application/vnd.apple.installer+xml"},
      {"mpm", "application/vnd.blueice.multipass"},
      {"mpn", "application/vnd.mophun.application"},
      {"mpp", "application/vnd.ms-project"},
      {"mpt", "application/vnd.ms-project"},
      {"mpy", "application/vnd.ibm.minipay"},
      {"mqy", "application/vnd.mobius.mqy"},
      {"mrc", "application/marc"},
      {"mrcx", "application/marcxml+xml"},
      {"ms", "text/troff"},
      {"mscml", "application/mediaservercontrol+xml"},
      {"mseed", "application/vnd.fdsn.mseed"},
      {"mseq", "application/vnd.mseq"},
      {"msf", "application/vnd.epson.msf"},
      {"msh", "model/mesh"},
      {"msi", "application/x-msdownload"},
      {"msl", "application/vnd.mobius.msl"},
      {"msty", "application/vnd.muvee.style"},
      {"mts", "model/vnd.mts"},
      {"mus", "application/vnd.musician"},
      {"musicxml", "application/vnd.recordare.musicxml+xml"},
      {"mvb", "application/x-msmediaview"},
      {"mwf", "application/vnd.mfer"},
      {"mxf", "application/mxf"},
      {"mxl", "application/vnd.recordare.musicxml"},
      {"mxml", "application/xv+xml"},
      {"mxs", "application/vnd.triscape.mxs"},
      {"mxu", "video/vnd.mpegurl"},
      {"n-gage", "application/vnd.nokia.n-gage.symbian.install"},
      {"n3", "text/n3"},
      {"nb", "application/mathematica"},
      {"nbp", "application/vnd.wolfram.player"},
      {"nc", "application/x-netcdf"},
      {"ncx", "application/x-dtbncx+xml"},
      {"nfo", "text/x-nfo"},
      {"ngdat", "application/vnd.nokia.n-gage.data"},
      {"nitf", "application/vnd.nitf"},
      {"nlu", "application/vnd.neurolanguage.nlu"},
      {"nml", "application/vnd.enliven"},
      {"nnd", "application/vnd.noblenet-directory"},
      {"nns", "application/vnd.noblenet-sealer"},
      {"nnw", "application/vnd.noblenet-web"},
      {"npx", "image/vnd.net-fpx"},
      {"nsc", "application/x-conference"},
      {"nsf", "application/vnd.lotus-notes"},
      {"ntf", "application/vnd.nitf"},
      {"nzb", "application/x-nzb"},
      {"oa2", "application/vnd.fujitsu.oasys2"},
      {"oa3", "application/vnd.fujitsu.oasys3"},
      {"oas", "application/vnd.fujitsu.oasys"},
      {"obd", "application/x-msbinder"},
      {"obj", "application/x-tgif"},
      {"oda", "application/oda"},
      {"odb", "application/vnd.oasis.opendocument.database"},
      {"odc", "application/vnd.oasis.opendocument.chart"},
      {"odf", "application/vnd.oasis.opendocument.formula"},
      {"odft", "application/vnd.oasis.opendocument.formula-template"},
      {"odg", "application/vnd.oasis.opendocument.graphics"},
      {"odi", "application/vnd.oasis.opendocument.image"},
      {"odm", "application/vnd.oasis.opendocument.text-master"},
      {"odp", "application/vnd.oasis.opendocument.presentation"},
      {"ods", "application/vnd.oasis.opendocument.spreadsheet"},
      {"odt", "application/vnd.oasis.opendocument.text"},
      {"oga", "audio/ogg"},
      {"ogg", "audio/ogg"},
      {"ogv", "video/ogg"},
      {"ogx", "application/ogg"},
      {"omdoc", "application/omdoc+xml"},
      {"onepkg", "application/onenote"},
      {"onetmp", "application/onenote"},
      {"onetoc", "application/onenote"},
      {"onetoc2", "application/onenote"},
      {"opf", "application/oebps-package+xml"},
      {"opml", "text/x-opml"},
      {"oprc", "application/vnd.palm"},
      {"org", "application/vnd.lotus-organizer"},
      {"osf", "application/vnd.yamaha.openscoreformat"},
      {"osfpvg", "application/vnd.yamaha.openscoreformat.osfpvg+xml"},
      {"otc", "application/vnd.oasis.opendocument.chart-template"},
      {"otf", "application/x-font-otf"},
      {"otg", "application/vnd.oasis.opendocument.graphics-template"},
      {"oth", "application/vnd.oasis.opendocument.text-web"},
      {"oti", "application/vnd.oasis.opendocument.image-template"},
      {"otp", "application/vnd.oasis.opendocument.presentation-template"},
      {"ots", "application/vnd.oasis.opendocument.spreadsheet-template"},
      {"ott", "application/vnd.oasis.opendocument.text-template"},
      {"oxps", "application/oxps"},
      {"oxt", "application/vnd.openofficeorg.extension"},
      {"p", "text/x-pascal"},
      {"p10", "application/pkcs10"},
      {"p12", "application/x-pkcs12"},
      {"p7b", "application/x-pkcs7-certificates"},
      {"p7c", "application/pkcs7-mime"},
      {"p7m", "application/pkcs7-mime"},
      {"p7r", "application/x-pkcs7-certreqresp"},
      {"p7s", "application/pkcs7-signature"},
      {"p8", "application/pkcs8"},
      {"pas", "text/x-pascal"},
      {"paw", "application/vnd.pawaafile"},
      {"pbd", "application/vnd.powerbuilder6"},
      {"pbm", "image/x-portable-bitmap"},
      {"pcap", "application/vnd.tcpdump.pcap"},
      {"pcf", "application/x-font-pcf"},
      {"pcl", "application/vnd.hp-pcl"},
      {"pclxl", "application/vnd.hp-pclxl"},
      {"pct", "image/x-pict"},
      {"pcurl", "application/vnd.curl.pcurl"},
      {"pcx", "image/x-pcx"},
      {"pdb", "application/vnd.palm"},
      {"pdf", "application/pdf"},
      {"pfa", "application/x-font-type1"},
      {"pfb", "application/x-font-type1"},
      {"pfm", "application/x-font-type1"},
      {"pfr", "application/font-tdpfr"},
      {"pfx", "application/x-pkcs12"},
      {"pgm", "image/x-portable-graymap"},
      {"pgn", "application/x-chess-pgn"},
      {"pgp", "application/pgp-encrypted"},
      {"pic", "image/x-pict"},
      {"pkg", "application/octet-stream"},
      {"pki", "application/pkixcmp"},
      {"pkipath", "application/pkix-pkipath"},
      {"plb", "application/vnd.3gpp.pic-bw-large"},
      {"plc", "application/vnd.mobius.plc"},
      {"plf", "application/vnd.pocketlearn"},
      {"pls", "application/pls+xml"},
      {"pml", "application/vnd.ctc-posml"},
      {"png", "image/png"},
      {"pnm", "image/x-portable-anymap"},
      {"portpkg", "application/vnd.macports.portpkg"},
      {"pot", "application/vnd.ms-powerpoint"},
      {"potm", "application/vnd.ms-powerpoint.template.macroenabled.12"},
      {"potx", "application/"
               "vnd.openxmlformats-officedocument.presentationml.template"},
      {"ppam", "application/vnd.ms-powerpoint.addin.macroenabled.12"},
      {"ppd", "application/vnd.cups-ppd"},
      {"ppm", "image/x-portable-pixmap"},
      {"pps", "application/vnd.ms-powerpoint"},
      {"ppsm", "application/vnd.ms-powerpoint.slideshow.macroenabled.12"},
      {"ppsx", "application/"
               "vnd.openxmlformats-officedocument.presentationml.slideshow"},
      {"ppt", "application/vnd.ms-powerpoint"},
      {"pptm", "application/vnd.ms-powerpoint.presentation.macroenabled.12"},
      {"pqa", "application/vnd.palm"},
      {"prc", "application/x-mobipocket-ebook"},
      {"pre", "application/vnd.lotus-freelance"},
      {"prf", "application/pics-rules"},
      {"ps", "application/postscript"},
      {"psb", "application/vnd.3gpp.pic-bw-small"},
      {"psd", "image/vnd.adobe.photoshop"},
      {"psf", "application/x-font-linux-psf"},
      {"pskcxml", "application/pskc+xml"},
      {"ptid", "application/vnd.pvi.ptid1"},
      {"pub", "application/x-mspublisher"},
      {"pvb", "application/vnd.3gpp.pic-bw-var"},
      {"pwn", "application/vnd.3m.post-it-notes"},
      {"pya", "audio/vnd.ms-playready.media.pya"},
      {"pyv", "video/vnd.ms-playready.media.pyv"},
      {"qam", "application/vnd.epson.quickanime"},
      {"qbo", "application/vnd.intu.qbo"},
      {"qfx", "application/vnd.intu.qfx"},
      {"qps", "application/vnd.publishare-delta-tree"},
      {"qt", "video/quicktime"},
      {"qwd", "application/vnd.quark.quarkxpress"},
      {"qwt", "application/vnd.quark.quarkxpress"},
      {"qxb", "application/vnd.quark.quarkxpress"},
      {"qxd", "application/vnd.quark.quarkxpress"},
      {"qxl", "application/vnd.quark.quarkxpress"},
      {"qxt", "application/vnd.quark.quarkxpress"},
      {"ra", "audio/x-pn-realaudio"},
      {"ram", "audio/x-pn-realaudio"},
      {"rar", "application/x-rar-compressed"},
      {"ras", "image/x-cmu-raster"},
      {"rcprofile", "application/vnd.ipunplugged.rcprofile"},
      {"rdf", "application/rdf+xml"},
      {"rdz", "application/vnd.data-vision.rdz"},
      {"rep", "application/vnd.businessobjects"},
      {"res", "application/x-dtbresource+xml"},
      {"rgb", "image/x-rgb"},
      {"rif", "application/reginfo+xml"},
      {"rip", "audio/vnd.rip"},
      {"ris", "application/x-research-info-systems"},
      {"rl", "application/resource-lists+xml"},
      {"rlc", "image/vnd.fujixerox.edmics-rlc"},
      {"rld", "application/resource-lists-diff+xml"},
      {"rm", "application/vnd.rn-realmedia"},
      {"rmi", "audio/midi"},
      {"rmp", "audio/x-pn-realaudio-plugin"},
      {"rms", "application/vnd.jcp.javame.midlet-rms"},
      {"rmvb", "application/vnd.rn-realmedia-vbr"},
      {"rnc", "application/relax-ng-compact-syntax"},
      {"roa", "application/rpki-roa"},
      {"roff", "text/troff"},
      {"rp9", "application/vnd.cloanto.rp9"},
      {"rpss", "application/vnd.nokia.radio-presets"},
      {"rpst", "application/vnd.nokia.radio-preset"},
      {"rq", "application/sparql-query"},
      {"rs", "application/rls-services+xml"},
      {"rsd", "application/rsd+xml"},
      {"rss", "application/rss+xml"},
      {"rtf", "application/rtf"},
      {"rtx", "text/richtext"},
      {"s", "text/x-asm"},
      {"s3m", "audio/s3m"},
      {"saf", "application/vnd.yamaha.smaf-audio"},
      {"sbml", "application/sbml+xml"},
      {"sc", "application/vnd.ibm.secure-container"},
      {"scd", "application/x-msschedule"},
      {"scm", "application/vnd.lotus-screencam"},
      {"scq", "application/scvp-cv-request"},
      {"scs", "application/scvp-cv-response"},
      {"scurl", "text/vnd.curl.scurl"},
      {"sda", "application/vnd.stardivision.draw"},
      {"sdc", "application/vnd.stardivision.calc"},
      {"sdd", "application/vnd.stardivision.impress"},
      {"sdkd", "application/vnd.solent.sdkm+xml"},
      {"sdkm", "application/vnd.solent.sdkm+xml"},
      {"sdp", "application/sdp"},
      {"sdw", "application/vnd.stardivision.writer"},
      {"see", "application/vnd.seemail"},
      {"seed", "application/vnd.fdsn.seed"},
      {"sema", "application/vnd.sema"},
      {"semd", "application/vnd.semd"},
      {"semf", "application/vnd.semf"},
      {"ser", "application/java-serialized-object"},
      {"setpay", "application/set-payment-initiation"},
      {"setreg", "application/set-registration-initiation"},
      {"sfd-hdstx", "application/vnd.hydrostatix.sof-data"},
      {"sfs", "application/vnd.spotfire.sfs"},
      {"sfv", "text/x-sfv"},
      {"sgi", "image/sgi"},
      {"sgl", "application/vnd.stardivision.writer-global"},
      {"sgm", "text/sgml"},
      {"sgml", "text/sgml"},
      {"sh", "application/x-sh"},
      {"shar", "application/x-shar"},
      {"shf", "application/shf+xml"},
      {"sid", "image/x-mrsid-image"},
      {"sig", "application/pgp-signature"},
      {"sil", "audio/silk"},
      {"silo", "model/mesh"},
      {"sis", "application/vnd.symbian.install"},
      {"sisx", "application/vnd.symbian.install"},
      {"sit", "application/x-stuffit"},
      {"sitx", "application/x-stuffitx"},
      {"skd", "application/vnd.koan"},
      {"skm", "application/vnd.koan"},
      {"skp", "application/vnd.koan"},
      {"skt", "application/vnd.koan"},
      {"sldm", "application/vnd.ms-powerpoint.slide.macroenabled.12"},
      {"sldx",
       "application/vnd.openxmlformats-officedocument.presentationml.slide"},
      {"slt", "application/vnd.epson.salt"},
      {"sm", "application/vnd.stepmania.stepchart"},
      {"smf", "application/vnd.stardivision.math"},
      {"smi", "application/smil+xml"},
      {"smil", "application/smil+xml"},
      {"smv", "video/x-smv"},
      {"smzip", "application/vnd.stepmania.package"},
      {"snd", "audio/basic"},
      {"snf", "application/x-font-snf"},
      {"so", "application/octet-stream"},
      {"spc", "application/x-pkcs7-certificates"},
      {"spf", "application/vnd.yamaha.smaf-phrase"},
      {"spl", "application/x-futuresplash"},
      {"spot", "text/vnd.in3d.spot"},
      {"spp", "application/scvp-vp-response"},
      {"spq", "application/scvp-vp-request"},
      {"spx", "audio/ogg"},
      {"sql", "application/x-sql"},
      {"src", "application/x-wais-source"},
      {"srt", "application/x-subrip"},
      {"sru", "application/sru+xml"},
      {"srx", "application/sparql-results+xml"},
      {"ssdl", "application/ssdl+xml"},
      {"sse", "application/vnd.kodak-descriptor"},
      {"ssf", "application/vnd.epson.ssf"},
      {"ssml", "application/ssml+xml"},
      {"st", "application/vnd.sailingtracker.track"},
      {"stc", "application/vnd.sun.xml.calc.template"},
      {"std", "application/vnd.sun.xml.draw.template"},
      {"stf", "application/vnd.wt.stf"},
      {"sti", "application/vnd.sun.xml.impress.template"},
      {"stk", "application/hyperstudio"},
      {"stl", "application/vnd.ms-pki.stl"},
      {"str", "application/vnd.pg.format"},
      {"stw", "application/vnd.sun.xml.writer.template"},
      {"sub", "image/vnd.dvb.subtitle"},
      {"sub", "text/vnd.dvb.subtitle"},
      {"sus", "application/vnd.sus-calendar"},
      {"susp", "application/vnd.sus-calendar"},
      {"sv4cpio", "application/x-sv4cpio"},
      {"sv4crc", "application/x-sv4crc"},
      {"svc", "application/vnd.dvb.service"},
      {"svd", "application/vnd.svd"},
      {"svg", "image/svg+xml"},
      {"svgz", "image/svg+xml"},
      {"swa", "application/x-director"},
      {"swf", "application/x-shockwave-flash"},
      {"swi", "application/vnd.aristanetworks.swi"},
      {"sxc", "application/vnd.sun.xml.calc"},
      {"sxd", "application/vnd.sun.xml.draw"},
      {"sxg", "application/vnd.sun.xml.writer.global"},
      {"sxi", "application/vnd.sun.xml.impress"},
      {"sxm", "application/vnd.sun.xml.math"},
      {"sxw", "application/vnd.sun.xml.writer"},
      {"t", "text/troff"},
      {"t3", "application/x-t3vm-image"},
      {"taglet", "application/vnd.mynfc"},
      {"tao", "application/vnd.tao.intent-module-archive"},
      {"tar", "application/x-tar"},
      {"tcap", "application/vnd.3gpp2.tcap"},
      {"tcl", "application/x-tcl"},
      {"teacher", "application/vnd.smart.teacher"},
      {"tei", "application/tei+xml"},
      {"teicorpus", "application/tei+xml"},
      {"tex", "application/x-tex"},
      {"texi", "application/x-texinfo"},
      {"texinfo", "application/x-texinfo"},
      {"text", "text/plain"},
      {"tfi", "application/thraud+xml"},
      {"tfm", "application/x-tex-tfm"},
      {"tga", "image/x-tga"},
      {"thmx", "application/vnd.ms-officetheme"},
      {"tif", "image/tiff"},
      {"tiff", "image/tiff"},
      {"tmo", "application/vnd.tmobile-livetv"},
      {"torrent", "application/x-bittorrent"},
      {"tpl", "application/vnd.groove-tool-template"},
      {"tpt", "application/vnd.trid.tpt"},
      {"tr", "text/troff"},
      {"tra", "application/vnd.trueapp"},
      {"trm", "application/x-msterminal"},
      {"tsd", "application/timestamped-data"},
      {"tsv", "text/tab-separated-values"},
      {"ttc", "application/x-font-ttf"},
      {"ttf", "application/x-font-ttf"},
      {"ttl", "text/turtle"},
      {"twd", "application/vnd.simtech-mindmapper"},
      {"twds", "application/vnd.simtech-mindmapper"},
      {"txd", "application/vnd.genomatix.tuxedo"},
      {"txf", "application/vnd.mobius.txf"},
      {"txt", "text/plain"},
      {"u32", "application/x-authorware-bin"},
      {"udeb", "application/x-debian-package"},
      {"ufd", "application/vnd.ufdl"},
      {"ufdl", "application/vnd.ufdl"},
      {"ulx", "application/x-glulx"},
      {"umj", "application/vnd.umajin"},
      {"unityweb", "application/vnd.unity"},
      {"uoml", "application/vnd.uoml+xml"},
      {"uri", "text/uri-list"},
      {"uris", "text/uri-list"},
      {"urls", "text/uri-list"},
      {"ustar", "application/x-ustar"},
      {"utz", "application/vnd.uiq.theme"},
      {"uu", "text/x-uuencode"},
      {"uva", "audio/vnd.dece.audio"},
      {"uvd", "application/vnd.dece.data"},
      {"uvf", "application/vnd.dece.data"},
      {"uvg", "image/vnd.dece.graphic"},
      {"uvh", "video/vnd.dece.hd"},
      {"uvi", "image/vnd.dece.graphic"},
      {"uvm", "video/vnd.dece.mobile"},
      {"uvp", "video/vnd.dece.pd"},
      {"uvs", "video/vnd.dece.sd"},
      {"uvt", "application/vnd.dece.ttml+xml"},
      {"uvu", "video/vnd.uvvu.mp4"},
      {"uvv", "video/vnd.dece.video"},
      {"uvva", "audio/vnd.dece.audio"},
      {"uvvd", "application/vnd.dece.data"},
      {"uvvf", "application/vnd.dece.data"},
      {"uvvg", "image/vnd.dece.graphic"},
      {"uvvh", "video/vnd.dece.hd"},
      {"uvvi", "image/vnd.dece.graphic"},
      {"uvvm", "video/vnd.dece.mobile"},
      {"uvvp", "video/vnd.dece.pd"},
      {"uvvs", "video/vnd.dece.sd"},
      {"uvvt", "application/vnd.dece.ttml+xml"},
      {"uvvu", "video/vnd.uvvu.mp4"},
      {"uvvv", "video/vnd.dece.video"},
      {"uvvx", "application/vnd.dece.unspecified"},
      {"uvvz", "application/vnd.dece.zip"},
      {"uvx", "application/vnd.dece.unspecified"},
      {"uvz", "application/vnd.dece.zip"},
      {"vcard", "text/vcard"},
      {"vcd", "application/x-cdlink"},
      {"vcf", "text/x-vcard"},
      {"vcg", "application/vnd.groove-vcard"},
      {"vcs", "text/x-vcalendar"},
      {"vcx", "application/vnd.vcx"},
      {"vis", "application/vnd.visionary"},
      {"viv", "video/vnd.vivo"},
      {"vob", "video/x-ms-vob"},
      {"vor", "application/vnd.stardivision.writer"},
      {"vox", "application/x-authorware-bin"},
      {"vrml", "model/vrml"},
      {"vsd", "application/vnd.visio"},
      {"vsf", "application/vnd.vsf"},
      {"vss", "application/vnd.visio"},
      {"vst", "application/vnd.visio"},
      {"vsw", "application/vnd.visio"},
      {"vtu", "model/vnd.vtu"},
      {"vxml", "application/voicexml+xml"},
      {"w3d", "application/x-director"},
      {"wad", "application/x-doom"},
      {"wav", "audio/x-wav"},
      {"wax", "audio/x-ms-wax"},
      {"wbmp", "image/vnd.wap.wbmp"},
      {"wbs", "application/vnd.criticaltools.wbs+xml"},
      {"wbxml", "application/vnd.wap.wbxml"},
      {"wcm", "application/vnd.ms-works"},
      {"wdb", "application/vnd.ms-works"},
      {"wdp", "image/vnd.ms-photo"},
      {"weba", "audio/webm"},
      {"webm", "video/webm"},
      {"webp", "image/webp"},
      {"wg", "application/vnd.pmi.widget"},
      {"wgt", "application/widget"},
      {"wks", "application/vnd.ms-works"},
      {"wm", "video/x-ms-wm"},
      {"wma", "audio/x-ms-wma"},
      {"wmd", "application/x-ms-wmd"},
      {"wmf", "application/x-msmetafile"},
      {"wml", "text/vnd.wap.wml"},
      {"wmlc", "application/vnd.wap.wmlc"},
      {"wmls", "text/vnd.wap.wmlscript"},
      {"wmlsc", "application/vnd.wap.wmlscriptc"},
      {"wmv", "video/x-ms-wmv"},
      {"wmx", "video/x-ms-wmx"},
      {"wmz", "application/x-ms-wmz"},
      {"wmz", "application/x-msmetafile"},
      {"woff", "application/font-woff"},
      {"wpd", "application/vnd.wordperfect"},
      {"wpl", "application/vnd.ms-wpl"},
      {"wps", "application/vnd.ms-works"},
      {"wqd", "application/vnd.wqd"},
      {"wri", "application/x-mswrite"},
      {"wrl", "model/vrml"},
      {"wsdl", "application/wsdl+xml"},
      {"wspolicy", "application/wspolicy+xml"},
      {"wtb", "application/vnd.webturbo"},
      {"wvx", "video/x-ms-wvx"},
      {"x32", "application/x-authorware-bin"},
      {"x3d", "model/x3d+xml"},
      {"x3db", "model/x3d+binary"},
      {"x3dbz", "model/x3d+binary"},
      {"x3dv", "model/x3d+vrml"},
      {"x3dvz", "model/x3d+vrml"},
      {"x3dz", "model/x3d+xml"},
      {"xaml", "application/xaml+xml"},
      {"xap", "application/x-silverlight-app"},
      {"xar", "application/vnd.xara"},
      {"xbap", "application/x-ms-xbap"},
      {"xbd", "application/vnd.fujixerox.docuworks.binder"},
      {"xbm", "image/x-xbitmap"},
      {"xdf", "application/xcap-diff+xml"},
      {"xdm", "application/vnd.syncml.dm+xml"},
      {"xdp", "application/vnd.adobe.xdp+xml"},
      {"xdssc", "application/dssc+xml"},
      {"xdw", "application/vnd.fujixerox.docuworks"},
      {"xenc", "application/xenc+xml"},
      {"xer", "application/patch-ops-error+xml"},
      {"xfdf", "application/vnd.adobe.xfdf"},
      {"xfdl", "application/vnd.xfdl"},
      {"xht", "application/xhtml+xml"},
      {"xhtml", "application/xhtml+xml"},
      {"xhvml", "application/xv+xml"},
      {"xif", "image/vnd.xiff"},
      {"xla", "application/vnd.ms-excel"},
      {"xlam", "application/vnd.ms-excel.addin.macroenabled.12"},
      {"xlc", "application/vnd.ms-excel"},
      {"xlf", "application/x-xliff+xml"},
      {"xlm", "application/vnd.ms-excel"},
      {"xls", "application/vnd.ms-excel"},
      {"xlsb", "application/vnd.ms-excel.sheet.binary.macroenabled.12"},
      {"xlsm", "application/vnd.ms-excel.sheet.macroenabled.12"},
      {"xlsx",
       "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
      {"xlt", "application/vnd.ms-excel"},
      {"xltm", "application/vnd.ms-excel.template.macroenabled.12"},
      {"xltx", "application/"
               "vnd.openxmlformats-officedocument.spreadsheetml.template"},
      {"xlw", "application/vnd.ms-excel"},
      {"xm", "audio/xm"},
      {"xml", "application/xml"},
      {"xo", "application/vnd.olpc-sugar"},
      {"xop", "application/xop+xml"},
      {"xpi", "application/x-xpinstall"},
      {"xpl", "application/xproc+xml"},
      {"xpm", "image/x-xpixmap"},
      {"xpr", "application/vnd.is-xpr"},
      {"xps", "application/vnd.ms-xpsdocument"},
      {"xpw", "application/vnd.intercon.formnet"},
      {"xpx", "application/vnd.intercon.formnet"},
      {"xsl", "application/xml"},
      {"xslt", "application/xslt+xml"},
      {"xsm", "application/vnd.syncml+xml"},
      {"xspf", "application/xspf+xml"},
      {"xul", "application/vnd.mozilla.xul+xml"},
      {"xvm", "application/xv+xml"},
      {"xvml", "application/xv+xml"},
      {"xwd", "image/x-xwindowdump"},
      {"xyz", "chemical/x-xyz"},
      {"xz", "application/x-xz"},
      {"yang", "application/yang"},
      {"yin", "application/yin+xml"},
      {"z1", "application/x-zmachine"},
      {"z2", "application/x-zmachine"},
      {"z3", "application/x-zmachine"},
      {"z4", "application/x-zmachine"},
      {"z5", "application/x-zmachine"},
      {"z6", "application/x-zmachine"},
      {"z7", "application/x-zmachine"},
      {"z8", "application/x-zmachine"},
      {"zaz", "application/vnd.zzazz.deck+xml"},
      {"zip", "application/zip"},
      {"zir", "application/vnd.zul"},
      {"zirz", "application/vnd.zul"},
      {"zmm", "application/vnd.handheld-entertainment+xml"},
      {{0}, NULL},
  };
  // Copy 8 bytes of the requested extension.
  // (8 byte comparison in enough to avoid collisions)
  uint64_t ext8byte = 0;
  char *extlow = (void *)(&ext8byte);
  // change the copy to lowercase
  size_t pos = 0;
  while (pos < 8 && ext[pos]) {
    extlow[pos] =
        (ext[pos] >= 'A' && ext[pos] <= 'Z') ? (ext[pos] | 32) : ext[pos];
    ++pos;
  }
  // optimize starting position
  uint8_t start = (uint8_t)extlow[0];
  // skip unnecessary reviews
  if (start >= 'u')
    pos = 800;
  else if (start >= 'r')
    pos = 640;
  else if (start >= 'n')
    pos = 499;
  else if (start >= 'm')
    pos = 400;
  else if (start >= 'i')
    pos = 300;
  else if (start >= 'e')
    pos = 190;
  else
    pos = 0;
  // check for 8 byte comparison - should be fast on 64 bit systems.
  uint64_t *lstext;
  while (List[pos].ext[0]) {
    lstext = (uint64_t *)List[pos].ext;
    if (ext8byte == *lstext)
      return List[pos].mime;
    pos++;
  }
  return NULL;
}
