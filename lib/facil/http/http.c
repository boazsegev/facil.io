/*
Copyright: Boaz Segev, 2016-2020
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include <http.h>

/* *****************************************************************************
NOOP VTable for HTTP engines (protocols)
***************************************************************************** */
/** send data */
static int http___noop_send(http_s *h,
                            void *body,
                            size_t len,
                            uintptr_t offset,
                            void (*dealloc)(void *),
                            uint8_t finish) {
  if (dealloc)
    dealloc(body);
  return -1;
  (void)h;
  (void)body;
  (void)len;
  (void)offset;
  (void)dealloc;
  (void)finish;
}

static int http___noop_send_fd(http_s *h,
                               int fd,
                               size_t len,
                               uintptr_t offset,
                               uint8_t finish) {
  close(fd);
  return -1;
  (void)h;
  (void)fd;
  (void)len;
  (void)offset;
  (void)finish;
}

static int http___noop_http_push_data(http_s *h,
                                      void *data,
                                      uintptr_t length,
                                      FIOBJ mime_type) {
  fiobj_free(mime_type);
  return -1;
  (void)h;
  (void)data;
  (void)length;
  (void)mime_type;
}

static int http___noop_upgrade2ws(http_s *h, websocket_settings_s *settings) {
  return -1;
  (void)h;
  (void)settings;
}

static int http___noop_push(http_s *h, FIOBJ url, FIOBJ mime_type) {
  fiobj_free(url);
  fiobj_free(mime_type);
  return -1;
  (void)h;
}

static intptr_t http___noop_hijack(http_s *h, fio_str_info_s *leftover) {
  return -1;
  (void)h;
  (void)leftover;
}

static int http___noop_upgrade2sse(http_s *h, http_sse_settings_s *settings) {
  return -1;
  (void)h;
  (void)settings;
}

static int http___noop_sse_write(http_s *sse,
                                 void *body,
                                 size_t len,
                                 uintptr_t offset,
                                 void (*dealloc)(void *)) {
  if (dealloc)
    dealloc(body);
  return -1;
  (void)sse;
  (void)body;
  (void)len;
  (void)offset;
  (void)dealloc;
}

static int http___noop_sse_close(http_s *sse) {
  return -1;
  (void)sse;
}

static int http___noop_free(http_s *h) {
  return -1;
  (void)h;
}

http___vtable_s HTTP_NOOP_VTABLE = {
    .send = http___noop_send,
    .send_fd = http___noop_send_fd,
    .http_push_data = http___noop_http_push_data,
    .upgrade2ws = http___noop_upgrade2ws,
    .push = http___noop_push,
    .hijack = http___noop_hijack,
    .upgrade2sse = http___noop_upgrade2sse,
    .sse_write = http___noop_sse_write,
    .sse_close = http___noop_sse_close,
    .free = http___noop_free,
};
/* *****************************************************************************
Compile Time Settings
***************************************************************************** */

/** When a new connection is accepted, it will be immediately declined with a
 * 503 service unavailable (server busy) response unless the following number of
 * file descriptors is available.*/
#ifndef HTTP_BUSY_UNLESS_HAS_FDS
#define HTTP_BUSY_UNLESS_HAS_FDS 64
#endif

#ifndef HTTP_DEFAULT_BODY_LIMIT
#define HTTP_DEFAULT_BODY_LIMIT (1024 * 1024 * 50)
#endif

#ifndef HTTP_DEFAULT_MAX_HEADER_COUNT
#define HTTP_DEFAULT_MAX_HEADER_COUNT 128
#endif

#ifndef HTTP_MAX_HEADER_LENGTH
/** the default maximum length for a single header line */
#define HTTP_MAX_HEADER_LENGTH 8192
#endif

#ifndef HTTP_MIME_REGISTRY_AUTO
/**
 * When above zero, fills the mime-type registry with known values.
 *
 * When zero (false), no mime-type values will be automatically registered.
 *
 * When negative, minimal mime-type values will be automatically registered,
 * including only mime types for the file extensions: html, htm, txt, js, css
 * and json.
 *
 * On embedded systems, consider filling the mime registy manually or sitting
 * this to a negative value, since the large number of prefilled values could
 * increase memory usage and executable size.
 */
#define HTTP_MIME_REGISTRY_AUTO 1
#endif

#ifndef FIO_HTTP_EXACT_LOGGING
/**
 * By default, facil.io logs the HTTP request cycle using a fuzzy starting point
 * (a close enough timestamp).
 *
 * The fuzzy timestamp includes delays that aren't related to the HTTP request,
 * sometimes including time that was spent waiting on the client. On the other
 * hand, `FIO_HTTP_EXACT_LOGGING` excludes time that the client might have been
 * waiting for facil.io to read data from the network.
 *
 * Due to the preference to err on the side of causion, fuzzy time-stamping is
 * the default.
 */
#define FIO_HTTP_EXACT_LOGGING 0
#endif

/* *****************************************************************************
Cookie setting
***************************************************************************** */

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
int http_cookie_set___(http_s *, http_cookie_args_s);
int http_cookie_set FIO_NOOP(http_s *h, http_cookie_args_s cookie) {
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
  FIO_ASSERT_DEBUG(h, "Can't set cookie for NULL HTTP handler!");
  if (!h || FIOBJ_TYPE_CLASS(http___metadata(h)->headers) != FIOBJ_T_HASH ||
      cookie.name_len >= 32768 || cookie.value_len >= 131072) {
    return -1;
  }
  /* write name and value while auto-correcting encoding issues */
  size_t capa = cookie.name_len + cookie.value_len + cookie.domain_len +
                cookie.path_len + 128;
  size_t len = 0;
  FIOBJ c = fiobj_str_new_buf(capa);
  fio_str_info_s t = fiobj_str_info(c);

#define copy_cookie_ch(ch_var)                                                 \
  if (invalid_cookie_##ch_var##_char[(uint8_t)cookie.ch_var[tmp]]) {           \
    if (!warn_illegal) {                                                       \
      ++warn_illegal;                                                          \
      FIO_LOG_WARNING("illegal char 0x%.2x in cookie " #ch_var " (in %s)\n"    \
                      "         automatic %% encoding applied",                \
                      cookie.ch_var[tmp],                                      \
                      cookie.ch_var);                                          \
    }                                                                          \
    t.buf[len++] = '%';                                                        \
    t.buf[len++] = fio_i2c(((uint8_t)cookie.ch_var[tmp] >> 4) & 0x0F);         \
    t.buf[len++] = fio_i2c((uint8_t)cookie.ch_var[tmp] & 0x0F);                \
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
#undef copy_cookie_ch

  /* client cookie headers are simpler */
  if (http_settings(h) && http_settings(h)->is_client) {
    if (!cookie.value) {
      fiobj_free(c);
      return -1;
    }
    http_header_set(h, HTTP_HEADER_COOKIE, c);
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
  switch (cookie.same_site) {
  case HTTP_COOKIE_SAME_SITE_BROWSER_DEFAULT: /* fallthrough */
  default:
    break;
  case HTTP_COOKIE_SAME_SITE_NONE:
    fiobj_str_write(c, "SameSite=None;", 14);
    break;
  case HTTP_COOKIE_SAME_SITE_LAX:
    fiobj_str_write(c, "SameSite=Lax;", 13);
    break;
  case HTTP_COOKIE_SAME_SITE_STRICT:
    fiobj_str_write(c, "SameSite=Strict;", 16);
    break;
  }
  http_header_set(h, HTTP_HEADER_SET_COOKIE, c);
  return 0;
}

/* *****************************************************************************
Sendfile2
***************************************************************************** */

#define HTTP_SEND_FILE2_MAX_ENCODINGS_TEST 7

/* internal helper - tests for a file and prepers response */
static int http___test_filename(http_s *h,
                                fio_str_info_s filename,
                                fio_str_info_s enc) {
  // FIO_LOG_DEBUG2("(HTTP) sendfile testing: %s", filename.buf);
  struct stat file_data = {.st_size = 0};
  int file = -1;
  if (stat(filename.buf, &file_data) ||
      (!S_ISREG(file_data.st_mode) && !S_ISLNK(file_data.st_mode)))
    return -1;
  /*** file name Okay, handle request ***/
  // FIO_LOG_DEBUG2("(HTTP) sendfile found: %s", filename.buf);
  if (http_settings(h)->static_headers) { /* copy default headers */
    FIOBJ defs = http_settings(h)->static_headers;
    FIO_MAP_EACH(FIO_NAME(fiobj, FIOBJ___NAME_HASH), defs, pos) {
      fiobj_hash_set_if_missing(
          http___metadata(h)->headers,
          fiobj2hash(http___metadata(h)->headers, pos->obj.key),
          pos->obj.key,
          fiobj_dup(pos->obj.value));
    }
  }
  {
    /* set last-modified */
    FIOBJ tmp = fiobj_str_new_buf(42);
    fiobj_str_resize(tmp,
                     fio_time2rfc7231(fiobj_str2ptr(tmp), file_data.st_mtime));
    fiobj_hash_set_if_missing(
        http___metadata(h)->headers,
        fiobj_str_hash(HTTP_HEADER_LAST_MODIFIED,
                       (uintptr_t)http___metadata(h)->headers),
        HTTP_HEADER_LAST_MODIFIED,
        tmp);
  }
  /* set cache-control */
  fiobj_hash_set_if_missing(
      http___metadata(h)->headers,
      fiobj_str_hash(HTTP_HEADER_CACHE_CONTROL,
                     (uintptr_t)http___metadata(h)->headers),
      HTTP_HEADER_CACHE_CONTROL,
      fiobj_dup(HTTP_HVALUE_MAX_AGE));
  FIOBJ etag_str = FIOBJ_INVALID;
  {
    /* set & test etag */
    uint64_t etag = (uint64_t)file_data.st_size;
    etag ^= (uint64_t)file_data.st_mtime;
    etag = fio_risky_hash(&etag, sizeof(uint64_t), 0);
    etag_str = fiobj_str_new();
    fiobj_str_write_base64enc(etag_str, (void *)&etag, sizeof(uint64_t), 0);
    fiobj_hash_set_if_missing(
        http___metadata(h)->headers,
        fiobj_str_hash(HTTP_HEADER_ETAG,
                       (uintptr_t)http___metadata(h)->headers),
        HTTP_HEADER_ETAG,
        etag_str);
    /* test for if-none-match */
    FIOBJ tmp = fiobj_hash_get2(h->headers, HTTP_HEADER_IF_NONE_MATCH);
    if (tmp != FIOBJ_INVALID && fiobj_is_eq(tmp, etag_str)) {
      h->status = 304;
      http_send_data(h, NULL, 0, 0, NULL, 1);
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
          fiobj_str_printf(cranges,
                           "bytes %lu-%lu/%lu",
                           (unsigned long)start_at,
                           (unsigned long)(start_at + length - 1),
                           (unsigned long)file_data.st_size);
          http___header_reset(h, HTTP_HEADER_CONTENT_RANGE, cranges);
        }
        http___header_reset(h,
                            HTTP_HEADER_ACCEPT_RANGES,
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
        http_header_set(h,
                        HTTP_HEADER_ALLOW,
                        fiobj_str_new_cstr("GET, HEAD", 9));
        h->status = 200;
        http_send_data(h, NULL, 0, 0, NULL, 1);
        return 0;
      }
      break;
    case 3:
      if (!strncasecmp("get", s.buf, 3))
        goto open_file;
      break;
    case 4:
      if (!strncasecmp("head", s.buf, 4)) {
        http___header_reset(h,
                            HTTP_HEADER_CONTENT_LENGTH,
                            fiobj_num_new(length));
        http_send_data(h, NULL, 0, 0, NULL, 1);
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
      http___header_reset(h,
                          HTTP_HEADER_CONTENT_ENCODING,
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
      fiobj_hash_set_if_missing(
          http___metadata(h)->headers,
          fiobj_str_hash(HTTP_HEADER_CONTENT_TYPE,
                         (uintptr_t)http___metadata(h)->headers),
          HTTP_HEADER_CONTENT_TYPE,
          mime);
  }
  return http_send_file(h, file, length, offset, 1);
}

static inline int http___test_encoded_path(const char *mem, size_t len) {
  const char *pos = NULL;
  const char *end = mem + len;
  while (mem < end && (pos = memchr(mem, '/', (size_t)len))) {
    len = end - pos;
    mem = pos + 1;
    if (pos[1] == '/')
      return -1;
    if (len > 3 && pos[1] == '.' && pos[2] == '.' && pos[3] == '/')
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
 * The file name will be tested with any supported encoding extensions (i.e.,
 * filename.gz) and with the added default extension "html" (i.e.,
 * "filename.html.gz").
 *
 * The `encoded` string will be URL decoded while the `local` string will used
 * as is.
 *
 * Returns 0 on success. A success value WILL CONSUME the `http_s` handle (it
 * will become invalid, finishing the rresponse).
 *
 * Returns -1 on error (The `http_s` handle should still be used).
 */
int http_send_file2(http_s *h,
                    const char *prefix,
                    size_t prefix_len,
                    const char *encoded,
                    size_t encoded_len) {
  if (!h || !http___metadata(h)->pr || !http___metadata(h)->headers)
    return -1;
  /* stack allocated buffer for filename data */
  char buffer[2048];
  FIOBJ_STR_TEMP_VAR_EXISTING(fn, buffer, 0, 2048);
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
        if (fio_c2i(encoded[1]) > 15 || fio_c2i(encoded[2]) > 15)
          goto path_error;
        tmp[len++] = (fio_c2i(encoded[1]) << 4) | fio_c2i(encoded[2]);
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
    if (http___test_encoded_path(fn_inf.buf + prefix_len,
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

  /* holds default tests + accept-encoding headers */
  fio_str_info_s enc_src[HTTP_SEND_FILE2_MAX_ENCODINGS_TEST];
  fio_str_info_s enc_ext[HTTP_SEND_FILE2_MAX_ENCODINGS_TEST];
  size_t enc_count = 0;
  {
    /* add any supported encoding options, such as gzip, deflate, br, etc' */
    FIOBJ encodeings = fiobj_hash_get2(h->headers, HTTP_HEADER_ACCEPT_ENCODING);
    if (encodeings) {
      fio_str_info_s i = fiobj2cstr(encodeings);
      while (i.len && enc_count < HTTP_SEND_FILE2_MAX_ENCODINGS_TEST) {
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
      if (!http___test_filename(h, n, enc_src[i]))
        goto found_file;
      n = fiobj_str_resize(fn, ext_len);
    }
    /* test filename (without encoding) */
    if (!http___test_filename(h,
                              fiobj_str2cstr(fn),
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

/* *****************************************************************************
Error handling
***************************************************************************** */

/**
 * Sends an HTTP error response, finishing the response.
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 *
 * The `uuid` and `settings` arguments are only required if the `http_s` handle
 * is NULL.
 */
int http_send_error(http_s *h, size_t error_code) {
  return -1;
  (void)h;
  (void)error_code;
}

/**
 * Adds a `Link` header, possibly sending an early link / HTTP2 push (not yet).
 *
 * Returns -1 on error and 0 on success.
 */
int http_push(http_s *h, FIOBJ url, FIOBJ mime_type);

/* *****************************************************************************
HTTP Connections - Listening / Connecting / Hijacking
***************************************************************************** */

/**
 * Listens to HTTP connections at the specified `port`.
 *
 * Leave as NULL to ignore IP binding.
 *
 * Returns -1 on error and the socket's uuid on success.
 *
 * the `on_finish` callback is always called.
 */
intptr_t http_listen___(void); /* Sublime Text marker */
intptr_t http_listen FIO_NOOP(const char *port,
                              const char *binding,
                              http_settings_s);

/* *****************************************************************************
EventSource Support (SSE)
***************************************************************************** */

/**
 * Sets the ping interval for SSE connections.
 */
void http_sse_set_timout(http_s *sse, uint8_t timeout);

// struct http_sse_subscribe_args {
//   /** The channel name used for the subscription. */
//   fio_str_info_s channel;
//   /** The optional on message callback. If missing, Data is directly writen.
//   */ void (*on_message)(http_s *sse,
//                      fio_str_info_s channel,
//                      fio_str_info_s msg,
//                      void *udata);
//   /** An optional callback for when a subscription is fully canceled. */
//   void (*on_unsubscribe)(void *udata);
//   /** Opaque user */
//   void *udata;
//   /** A callback for pattern matching. */
//   uint8_t is_pattern;
// };

uintptr_t http_sse_subscribe___(void); /* sublime text marker */
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
uintptr_t http_sse_subscribe FIO_NOOP(http_s *sse,
                                      struct http_sse_subscribe_args args);

/**
 * Cancels a subscription and invalidates the subscription object.
 */
void http_sse_unsubscribe(http_s *sse, uintptr_t subscription);

int http_sse_write___(void); /* sublime text marker */
/**
 * Writes data to an EventSource (SSE) connection.
 *
 * See the {struct http_sse_write_args} for possible named arguments.
 */
int http_sse_write FIO_NOOP(http_s *sse, struct http_sse_write_args args) {
  if (!sse || !(args.id.len + args.data.len + args.event.len) ||
      fio_is_closed(http_sse2uuid(sse)))
    return -1;
  char *buf = FIOBJ_MEM_REALLOC(NULL,
                                0,
                                (4 + args.id.len + 2 + 7 + args.event.len + 2 +
                                 6 + args.data.len + 2 + 7 + 10 + 4),
                                0);
  char *pos = buf;
  if (args.id.len) {
    FIO_MEMCPY(pos, "id: ", 4);
    pos += 4;
    FIO_MEMCPY(pos, args.id.buf, args.id.len);
    pos += args.id.len;
    (pos++)[0] = '\r';
    (pos++)[0] = '\n';
  }
  if (args.event.len) {
    FIO_MEMCPY(pos, "event: ", 7);
    pos += 7;
    FIO_MEMCPY(pos, args.event.buf, args.event.len);
    pos += args.event.len;
    (pos++)[0] = '\r';
    (pos++)[0] = '\n';
  }
  if (args.retry) {
    FIO_MEMCPY(pos, "retry: ", 7);
    pos += 7;
    pos += fio_ltoa(pos, args.retry, 10);
    (pos++)[0] = '\r';
    (pos++)[0] = '\n';
  }
  if (args.data.len) {
    FIO_MEMCPY(pos, "data: ", 6);
    pos += 6;
    FIO_MEMCPY(pos, args.data.buf, args.data.len);
    pos += args.data.len;
    (pos++)[0] = '\r';
    (pos++)[0] = '\n';
  }
  (pos++)[0] = '\r';
  (pos++)[0] = '\n';
  return http___vtable(sse)->sse_write(sse,
                                       buf,
                                       pos - buf,
                                       0,
                                       FIO_NAME(fiobj_mem, free));
}

/**
 * Get the connection's UUID (for `fio_defer_io_task`, pub/sub, etc').
 */
intptr_t http_sse2uuid(http_s *sse) {
  if (http___metadata(sse)->pr)
    return http___metadata(sse)->pr->uuid;
  return -1;
}

/**
 * Closes an EventSource (SSE) connection.
 */
int http_sse_close(http_s *sse);

/* *****************************************************************************
HTTP GET and POST parsing helpers
***************************************************************************** */

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
int http_parse_body(http_s *h);

/**
 * Parses the query part of an HTTP request/response. Uses `http_add2hash`.
 *
 * This should be called after the `http_parse_body` function, just in case the
 * body is JSON that doesn't have an object at it's root.
 */
void http_parse_query(http_s *h);

/** Parses any Cookie / Set-Cookie headers, using the `http_add2hash` scheme. */
void http_parse_cookies(http_s *h, uint8_t is_url_encoded);

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
int http_add2hash(FIOBJ dest,
                  char *name,
                  size_t name_len,
                  char *value,
                  size_t value_len,
                  uint8_t encoded);

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
int http_add2hash2(FIOBJ dest,
                   char *name,
                   size_t name_len,
                   FIOBJ value,
                   uint8_t encoded);

/* *****************************************************************************
HTTP Status Strings and Mime-Type helpers
***************************************************************************** */

/** Returns a human readable string related to the HTTP status number. */
fio_str_info_s http_status2str(uintptr_t status);

/** Registers a Mime-Type to be associated with the file extension. */
void http_mimetype_register(char *file_ext,
                            size_t file_ext_len,
                            FIOBJ mime_type_str);

/**
 * Finds the mime-type associated with the file extension, returning a String on
 * success and FIOBJ_INVALID on failure.
 *
 * Remember to call `fiobj_free`.
 */
FIOBJ http_mimetype_find(char *file_ext, size_t file_ext_len) {
  return fiobj_null();
  (void)file_ext;
  (void)file_ext_len;
}

/**
 * Returns the mime-type associated with the URL or the default mime-type for
 * HTTP.
 *
 * Remember to call `fiobj_free`.
 */
FIOBJ http_mimetype_find2(FIOBJ url);

/** Clears the Mime-Type registry (it will be empty after this call). */
void http_mimetype_clear(void);

/* *****************************************************************************
Commonly used headers (fiobj Symbol objects)
***************************************************************************** */

FIOBJ HTTP_HEADER_ACCEPT;
FIOBJ HTTP_HEADER_ACCEPT_ENCODING;
FIOBJ HTTP_HEADER_ALLOW;
FIOBJ HTTP_HEADER_CACHE_CONTROL;
FIOBJ HTTP_HEADER_CONNECTION;
FIOBJ HTTP_HEADER_CONTENT_ENCODING;
FIOBJ HTTP_HEADER_CONTENT_LENGTH;
FIOBJ HTTP_HEADER_CONTENT_RANGE;
FIOBJ HTTP_HEADER_CONTENT_TYPE;
FIOBJ HTTP_HEADER_COOKIE;
FIOBJ HTTP_HEADER_DATE;
FIOBJ HTTP_HEADER_ETAG;
FIOBJ HTTP_HEADER_HOST;
FIOBJ HTTP_HEADER_IF_NONE_MATCH;
FIOBJ HTTP_HEADER_IF_RANGE;
FIOBJ HTTP_HEADER_LAST_MODIFIED;
FIOBJ HTTP_HEADER_ORIGIN;
FIOBJ HTTP_HEADER_SET_COOKIE;
FIOBJ HTTP_HEADER_TRANSFER_ENCODING;
FIOBJ HTTP_HEADER_UPGRADE;
FIOBJ HTTP_HEADER_RANGE;

FIOBJ HTTP_HEADER_ACCEPT_RANGES;        /* (!) always fiobj_dup (!) */
FIOBJ HTTP_HEADER_WS_SEC_CLIENT_KEY;    /* (!) always fiobj_dup (!) */
FIOBJ HTTP_HEADER_WS_SEC_KEY;           /* (!) always fiobj_dup (!) */
FIOBJ HTTP_HVALUE_BYTES;                /* (!) always fiobj_dup (!) */
FIOBJ HTTP_HVALUE_CHUNKED_ENCODING;     /* (!) always fiobj_dup (!) */
FIOBJ HTTP_HVALUE_CLOSE;                /* (!) always fiobj_dup (!) */
FIOBJ HTTP_HVALUE_CONTENT_TYPE_DEFAULT; /* (!) always fiobj_dup (!) */
FIOBJ HTTP_HVALUE_IDENTITY;             /* (!) always fiobj_dup (!) */
FIOBJ HTTP_HVALUE_GZIP;                 /* (!) always fiobj_dup (!) */
FIOBJ HTTP_HVALUE_KEEP_ALIVE;           /* (!) always fiobj_dup (!) */
FIOBJ HTTP_HVALUE_MAX_AGE;              /* (!) always fiobj_dup (!) */
FIOBJ HTTP_HVALUE_NO_CACHE;             /* (!) always fiobj_dup (!) */
FIOBJ HTTP_HVALUE_SSE_MIME;             /* (!) always fiobj_dup (!) */
FIOBJ HTTP_HVALUE_WEBSOCKET;            /* (!) always fiobj_dup (!) */
FIOBJ HTTP_HVALUE_WS_SEC_VERSION;       /* (!) always fiobj_dup (!) */
FIOBJ HTTP_HVALUE_WS_UPGRADE;           /* (!) always fiobj_dup (!) */
FIOBJ HTTP_HVALUE_WS_VERSION;           /* (!) always fiobj_dup (!) */

/* *****************************************************************************
HTTP General Helper functions that could be used globally
***************************************************************************** */

/**
 * Returns a String object representing the unparsed HTTP request/response (HTTP
 * version is capped at HTTP/1.1). Mostly usable for proxy usage and debugging.
 */
FIOBJ http2str(http_s *h);

/**
 * Writes a log line to `stdout` about the request / response object.
 *
 * This function is called automatically if the `.log` setting is enabled.
 *
 * See format details at:
 * http://publib.boulder.ibm.com/tividd/td/ITWSA/ITWSA_info45/en_US/HTML/guide/c-logs.html#common
 */
void http_write_log(http_s *h);

/* *****************************************************************************
HTTP URL decoding helper functions that might be used globally
***************************************************************************** */

/** Decodes a URL encoded string, no buffer overflow protection. */
ssize_t http_decode_url_unsafe(char *dest, const char *url_data);

/** Decodes a URL encoded string (query / form data). */
ssize_t http_decode_url(char *dest, const char *url_data, size_t length);

/** Decodes the "path" part of a request, no buffer overflow protection. */
ssize_t http_decode_path_unsafe(char *dest, const char *url_data);

/**
 * Decodes the "path" part of an HTTP request, no buffer overflow protection.
 */
ssize_t http_decode_path(char *dest, const char *url_data, size_t length);

#if DEBUG
void http_tests(void);
#endif
