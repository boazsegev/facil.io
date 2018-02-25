/*
Copyright: Boaz Segev, 2016-2018
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "spnlock.inc"

#include "fio_base64.h"
#include "http1.h"
#include "http_internal.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "fio_mem.h"

/* *****************************************************************************
Small Helpers
***************************************************************************** */
static inline int hex2byte(uint8_t *dest, const uint8_t *source);

static inline void add_content_length(http_s *r, uintptr_t length) {
  static uint64_t cl_hash = 0;
  if (!cl_hash)
    cl_hash = fio_siphash("content-length", 14);
  if (!fiobj_hash_get2(r->private_data.out_headers, cl_hash)) {
    static FIOBJ cl;
    if (!cl)
      cl = HTTP_HEADER_CONTENT_LENGTH;
    fiobj_hash_set(r->private_data.out_headers, cl, fiobj_num_new(length));
  }
}

static FIOBJ current_date;
static time_t last_date_added;
static spn_lock_i date_lock;
static inline void add_date(http_s *r) {
  static uint64_t date_hash = 0;
  if (!date_hash)
    date_hash = fio_siphash("date", 4);
  static uint64_t mod_hash = 0;
  if (!mod_hash)
    mod_hash = fio_siphash("last-modified", 13);

  if (facil_last_tick().tv_sec > last_date_added) {
    FIOBJ tmp = FIOBJ_INVALID;
    spn_lock(&date_lock);
    if (facil_last_tick().tv_sec > last_date_added) { /* retest inside lock */
      tmp = fiobj_str_buf(32);
      fiobj_str_resize(tmp, http_time2str(fiobj_obj2cstr(tmp).data,
                                          facil_last_tick().tv_sec));
      last_date_added = facil_last_tick().tv_sec;
      FIOBJ other = current_date;
      current_date = tmp;
      tmp = other;
    }
    spn_unlock(&date_lock);
    fiobj_free(tmp);
  }

  if (!fiobj_hash_get2(r->private_data.out_headers, date_hash)) {
    fiobj_hash_set(r->private_data.out_headers, HTTP_HEADER_DATE,
                   fiobj_dup(current_date));
  }
  if (r->status_str == FIOBJ_INVALID &&
      !fiobj_hash_get2(r->private_data.out_headers, mod_hash)) {
    fiobj_hash_set(r->private_data.out_headers, HTTP_HEADER_LAST_MODIFIED,
                   fiobj_dup(current_date));
  }
}

struct header_writer_s {
  FIOBJ dest;
  FIOBJ name;
  FIOBJ value;
};

static int write_header(FIOBJ o, void *w_) {
  struct header_writer_s *w = w_;
  if (!o)
    return 0;
  if (fiobj_hash_key_in_loop()) {
    w->name = fiobj_hash_key_in_loop();
  }
  if (FIOBJ_TYPE_IS(o, FIOBJ_T_ARRAY)) {
    fiobj_each1(o, 0, write_header, w);
    return 0;
  }
  fio_cstr_s name = fiobj_obj2cstr(w->name);
  fio_cstr_s str = fiobj_obj2cstr(o);
  if (!str.data)
    return 0;
  fiobj_str_write(w->dest, name.data, name.len);
  fiobj_str_write(w->dest, ":", 1);
  fiobj_str_write(w->dest, str.data, str.len);
  fiobj_str_write(w->dest, "\r\n", 2);
  return 0;
}

static char invalid_cookie_name_char[256];

static char invalid_cookie_value_char[256];
/* *****************************************************************************
The Request / Response type and functions
***************************************************************************** */
static const char hex_chars[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                 '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

#define HTTP_INVALID_HANDLE(h)                                                 \
  (!h || (!h->method && !h->status_str && h->status))
/**
 * Sets a response header, taking ownership of the value object, but NOT the
 * name object (so name objects could be reused in future responses).
 *
 * Returns -1 on error and 0 on success.
 */
int http_set_header(http_s *r, FIOBJ name, FIOBJ value) {
  if (HTTP_INVALID_HANDLE(r) || !name) {
    fiobj_free(value);
    return -1;
  }
  set_header_add(r->private_data.out_headers, name, value);
  return 0;
}
/**
 * Sets a response header, taking ownership of the value object, but NOT the
 * name object (so name objects could be reused in future responses).
 *
 * Returns -1 on error and 0 on success.
 */
int http_set_header2(http_s *r, fio_cstr_s n, fio_cstr_s v) {
  if (HTTP_INVALID_HANDLE(r) || !n.data || !n.length || (v.data && !v.length))
    return -1;
  FIOBJ tmp = fiobj_str_new(n.data, n.length);
  int ret = http_set_header(r, tmp, fiobj_str_new(v.data, v.length));
  fiobj_free(tmp);
  return ret;
}
/**
 * Sets a response cookie, taking ownership of the value object, but NOT the
 * name object (so name objects could be reused in future responses).
 *
 * Returns -1 on error and 0 on success.
 */
#undef http_set_cookie
int http_set_cookie(http_s *h, http_cookie_args_s cookie) {
#if DEBUG
  HTTP_ASSERT(h, "Can't set cookie for NULL HTTP handler!");
#endif
  if (HTTP_INVALID_HANDLE(h) || cookie.name_len >= 32768 ||
      cookie.value_len >= 131072)
    return -1;

  static int warn_illegal = 0;

  /* write name and value while auto-correcting encoding issues */
  size_t capa = cookie.name_len + cookie.value_len + 128;
  size_t len = 0;
  FIOBJ c = fiobj_str_buf(capa);
  fio_cstr_s t = fiobj_obj2cstr(c);
  if (cookie.name) {
    if (cookie.name_len) {
      size_t tmp = 0;
      while (tmp < cookie.name_len) {
        if (invalid_cookie_name_char[(uint8_t)cookie.name[tmp]]) {
          if (!warn_illegal) {
            ++warn_illegal;
            fprintf(stderr,
                    "WARNING: illegal char 0x%.2x in cookie name (in %s)\n"
                    "         automatic %% encoding applied\n",
                    cookie.name[tmp], cookie.name);
          }
          t.data[len++] = '%';
          t.data[len++] = hex_chars[(cookie.name[tmp] >> 4) & 0x0F];
          t.data[len++] = hex_chars[cookie.name[tmp] & 0x0F];
        } else {
          t.data[len++] = cookie.name[tmp];
        }
        tmp += 1;
        if (capa <= len + 3) {
          capa += 32;
          fiobj_str_capa_assert(c, capa);
          t = fiobj_obj2cstr(c);
        }
      }
    } else {
      size_t tmp = 0;
      while (cookie.name[tmp]) {
        if (invalid_cookie_name_char[(uint8_t)cookie.name[tmp]]) {
          if (!warn_illegal) {
            ++warn_illegal;
            fprintf(stderr,
                    "WARNING: illegal char 0x%.2x in cookie name (in %s)\n"
                    "         automatic %% encoding applied\n",
                    cookie.name[tmp], cookie.name);
          }
          t.data[len++] = '%';
          t.data[len++] = hex_chars[(cookie.name[tmp] >> 4) & 0x0F];
          t.data[len++] = hex_chars[cookie.name[tmp] & 0x0F];
        } else {
          t.data[len++] = cookie.name[tmp];
        }
        tmp += 1;
        if (capa <= len + 4) {
          capa += 32;
          fiobj_str_capa_assert(c, capa);
          t = fiobj_obj2cstr(c);
        }
      }
    }
  }
  t.data[len++] = '=';
  if (cookie.value) {
    if (cookie.value_len) {
      size_t tmp = 0;
      while (tmp < cookie.value_len) {
        if (invalid_cookie_value_char[(uint8_t)cookie.value[tmp]]) {
          if (!warn_illegal) {
            ++warn_illegal;
            fprintf(stderr,
                    "WARNING: illegal char 0x%.2x in cookie value (in %s)\n"
                    "         automatic %% encoding applied\n",
                    cookie.value[tmp], cookie.name);
          }
          t.data[len++] = '%';
          t.data[len++] = hex_chars[(cookie.value[tmp] >> 4) & 0x0F];
          t.data[len++] = hex_chars[cookie.value[tmp] & 0x0F];
        } else {
          t.data[len++] = cookie.value[tmp];
        }
        tmp += 1;
        if (capa <= len + 3) {
          capa += 32;
          fiobj_str_capa_assert(c, capa);
          t = fiobj_obj2cstr(c);
        }
      }
    } else {
      size_t tmp = 0;
      while (cookie.value[tmp]) {
        if (invalid_cookie_value_char[(uint8_t)cookie.value[tmp]]) {
          if (!warn_illegal) {
            ++warn_illegal;
            fprintf(stderr,
                    "WARNING: illegal char 0x%.2x in cookie value (in %s)\n"
                    "         automatic %% encoding applied\n",
                    cookie.value[tmp], cookie.name);
          }
          t.data[len++] = '%';
          t.data[len++] = hex_chars[(cookie.value[tmp] >> 4) & 0x0F];
          t.data[len++] = hex_chars[cookie.value[tmp] & 0x0F];
        } else {
          t.data[len++] = cookie.value[tmp];
        }
        tmp += 1;
        if (capa <= len + 3) {
          capa += 32;
          fiobj_str_capa_assert(c, capa);
          t = fiobj_obj2cstr(c);
        }
      }
    }
  } else
    cookie.max_age = -1;
  t.data[len++] = ';';
  t.data[len++] = ' ';

  if (h->status_str || !h->status) { /* on first request status == 0 */
    static uint64_t cookie_hash;
    if (!cookie_hash)
      cookie_hash = fio_siphash("cookie", 6);
    FIOBJ tmp = fiobj_hash_get2(h->private_data.out_headers, cookie_hash);
    if (!tmp) {
      set_header_add(h->private_data.out_headers, HTTP_HEADER_COOKIE, c);
    } else {
      fiobj_str_join(tmp, c);
      fiobj_free(c);
    }
    return 0;
  }

  if (capa <= len + 40) {
    capa = len + 40;
    fiobj_str_capa_assert(c, capa);
    t = fiobj_obj2cstr(c);
  }
  if (cookie.max_age) {
    memcpy(t.data + len, "Max-Age=", 8);
    len += 8;
    len += fio_ltoa(t.data + len, cookie.max_age, 10);
    t.data[len++] = ';';
    t.data[len++] = ' ';
  }
  fiobj_str_resize(c, len);

  if (cookie.domain && cookie.domain_len) {
    fiobj_str_write(c, "domain=", 7);
    fiobj_str_write(c, cookie.domain, cookie.domain_len);
    fiobj_str_write(c, ";", 1);
    t.data[len++] = ' ';
  }
  if (cookie.path && cookie.path_len) {
    fiobj_str_write(c, "path=", 5);
    fiobj_str_write(c, cookie.path, cookie.path_len);
    fiobj_str_write(c, ";", 1);
    t.data[len++] = ' ';
  }
  if (cookie.http_only) {
    fiobj_str_write(c, "HttpOnly;", 9);
  }
  if (cookie.secure) {
    fiobj_str_write(c, "secure;", 7);
  }
  set_header_add(h->private_data.out_headers, HTTP_HEADER_SET_COOKIE, c);
  return 0;
}
#define http_set_cookie(http__req__, ...)                                      \
  http_set_cookie((http__req__), (http_cookie_args_s){__VA_ARGS__})

/**
 * Sends the response headers and body.
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
int http_send_body(http_s *r, void *data, uintptr_t length) {
  if (!length || !data) {
    http_finish(r);
    return 0;
  }
  if (HTTP_INVALID_HANDLE(r))
    return -1;
  add_content_length(r, length);
  add_date(r);
  return ((http_vtable_s *)r->private_data.vtbl)
      ->http_send_body(r, data, length);
}
/**
 * Sends the response headers and the specified file (the response's body).
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
int http_sendfile(http_s *r, int fd, uintptr_t length, uintptr_t offset) {
  if (HTTP_INVALID_HANDLE(r)) {
    close(fd);
    return -1;
  };
  add_content_length(r, length);
  add_date(r);
  return ((http_vtable_s *)r->private_data.vtbl)
      ->http_sendfile(r, fd, length, offset);
}
/**
 * Sends the response headers and the specified file (the response's body).
 *
 * Returns -1 eton error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
int http_sendfile2(http_s *h, const char *prefix, size_t prefix_len,
                   const char *encoded, size_t encoded_len) {
  if (HTTP_INVALID_HANDLE(h))
    return -1;
  struct stat file_data = {.st_size = 0};
  static uint64_t accept_enc_hash = 0;
  if (!accept_enc_hash)
    accept_enc_hash = fio_siphash("accept-encoding", 15);
  static uint64_t range_hash = 0;
  if (!range_hash)
    range_hash = fio_siphash("range", 5);

  /* create filename string */
  FIOBJ filename = fiobj_str_tmp();
  if (prefix && prefix_len) {
    if (encoded && prefix[prefix_len - 1] == '/' && encoded[0] == '/')
      --prefix_len;
    fiobj_str_capa_assert(filename, prefix_len + encoded_len + 4);
    fiobj_str_write(filename, prefix, prefix_len);
  }
  {
    fio_cstr_s tmp = fiobj_obj2cstr(filename);
    if (encoded) {
      char *pos = (char *)encoded;
      const char *end = encoded + encoded_len;
      while (pos < end) {
        /* test for path manipulations while decoding */
        if (*pos == '/' && (pos[1] == '/' ||
                            (((uintptr_t)end - (uintptr_t)pos >= 4) &&
                             pos[1] == '.' && pos[2] == '.' && pos[3] == '/')))
          return -1;
        if (*pos == '%') {
          // decode hex value
          // this is a percent encoded value.
          if (hex2byte(tmp.bytes + tmp.len, (uint8_t *)pos + 1))
            return -1;
          tmp.len++;
          pos += 3;
        } else
          tmp.data[tmp.len++] = *(pos++);
      }
      tmp.data[tmp.len] = 0;
      fiobj_str_resize(filename, tmp.len);
    }
    if (tmp.data[tmp.len - 1] == '/')
      fiobj_str_write(filename, "index.html", 10);
  }
  /* test for file existance  */

  int file = -1;
  uint8_t is_gz = 0;

  fio_cstr_s s = fiobj_obj2cstr(filename);
  {
    FIOBJ tmp = fiobj_hash_get2(h->headers, accept_enc_hash);
    if (!tmp)
      goto no_gzip_support;
    fio_cstr_s ac_str = fiobj_obj2cstr(tmp);
    if (!ac_str.data || !strstr(ac_str.data, "gzip"))
      goto no_gzip_support;
    if (s.data[s.len - 3] != '.' || s.data[s.len - 2] != 'g' ||
        s.data[s.len - 1] != 'z') {
      fiobj_str_write(filename, ".gz", 3);
      fio_cstr_s s = fiobj_obj2cstr(filename);
      if (!stat(s.data, &file_data) &&
          (S_ISREG(file_data.st_mode) || S_ISLNK(file_data.st_mode))) {
        is_gz = 1;
        goto found_file;
      }
      fiobj_str_resize(filename, s.len - 3);
    }
  }
no_gzip_support:
  if (stat(s.data, &file_data) ||
      !(S_ISREG(file_data.st_mode) || S_ISLNK(file_data.st_mode)))
    return -1;
found_file:
  /* set last-modified */
  {
    FIOBJ tmp = fiobj_str_buf(32);
    fiobj_str_resize(
        tmp, http_time2str(fiobj_obj2cstr(tmp).data, file_data.st_mtime));
    http_set_header(h, HTTP_HEADER_LAST_MODIFIED, tmp);
  }
  /* set cache-control */
  http_set_header(h, HTTP_HEADER_CACHE_CONTROL, fiobj_dup(HTTP_HVALUE_MAX_AGE));
  /* set & test etag */
  uint64_t etag = (uint64_t)file_data.st_size;
  etag ^= (uint64_t)file_data.st_mtime;
  etag = fio_siphash(&etag, sizeof(uint64_t));
  FIOBJ etag_str = fiobj_str_buf(32);
  fiobj_str_resize(etag_str,
                   fio_base64_encode(fiobj_obj2cstr(etag_str).data,
                                     (void *)&etag, sizeof(uint64_t)));
  /* set */
  http_set_header(h, HTTP_HEADER_ETAG, etag_str);
  /* test */
  {
    static uint64_t none_match_hash = 0;
    if (!none_match_hash)
      none_match_hash = fio_siphash("if-none-match", 13);
    FIOBJ tmp2 = fiobj_hash_get2(h->headers, none_match_hash);
    if (tmp2 && fiobj_iseq(tmp2, etag_str)) {
      h->status = 304;
      http_finish(h);
      return 0;
    }
  }
  /* handle range requests */
  int64_t offset = 0;
  int64_t length = file_data.st_size;
  {
    static uint64_t ifrange_hash = 0;
    if (!ifrange_hash)
      ifrange_hash = fio_siphash("if-range", 8);
    FIOBJ tmp = fiobj_hash_get2(h->headers, ifrange_hash);
    if (tmp && fiobj_iseq(tmp, etag_str)) {
      fiobj_hash_delete2(h->headers, range_hash);
    } else {
      tmp = fiobj_hash_get2(h->headers, range_hash);
      if (tmp) {
        /* range ahead... */
        if (FIOBJ_TYPE_IS(tmp, FIOBJ_T_ARRAY))
          tmp = fiobj_ary_index(tmp, 0);
        fio_cstr_s range = fiobj_obj2cstr(tmp);
        if (!range.data || memcmp("bytes=", range.data, 6))
          goto open_file;
        char *pos = range.data + 6;
        int64_t start_at = 0, end_at = 0;
        start_at = fio_atol(&pos);
        if (start_at >= file_data.st_size)
          goto open_file;
        if (start_at >= 0) {
          pos++;
          end_at = fio_atol(&pos);
          if (end_at <= 0)
            goto open_file;
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

        http_set_header(h, HTTP_HEADER_CONTENT_RANGE,
                        fiobj_strprintf("bytes %lu-%lu/%lu",
                                        (unsigned long)start_at,
                                        (unsigned long)(start_at + length - 1),
                                        (unsigned long)file_data.st_size));
        http_set_header(h, HTTP_HEADER_ACCEPT_RANGES,
                        fiobj_dup(HTTP_HVALUE_BYTES));
      }
    }
  }
  /* test for an OPTIONS request or invalid methods */
  s = fiobj_obj2cstr(h->method);
  switch (s.len) {
  case 7:
    if (!strncasecmp("options", s.data, 7)) {
      http_set_header2(h, (fio_cstr_s){.data = "allow", .len = 5},
                       (fio_cstr_s){.data = "GET, HEAD", .len = 9});
      h->status = 200;
      http_finish(h);
      return 0;
    }
    break;
  case 3:
    if (!strncasecmp("get", s.data, 3))
      goto open_file;
    break;
  case 4:
    if (!strncasecmp("head", s.data, 4)) {
      http_set_header(h, HTTP_HEADER_CONTENT_LENGTH, fiobj_num_new(length));
      http_finish(h);
      return 0;
    }
    break;
  }
  http_send_error(h, 403);
  return 0;
open_file:
  s = fiobj_obj2cstr(filename);
  file = open(s.data, O_RDONLY);
  if (file == -1) {
    fprintf(stderr, "ERROR: Couldn't open file %s!\n", s.data);
    http_send_error(h, 500);
    return 0;
  }
  {
    FIOBJ tmp = 0;
    uintptr_t pos = 0;
    if (is_gz) {
      http_set_header(h, HTTP_HEADER_CONTENT_ENCODING,
                      fiobj_dup(HTTP_HVALUE_GZIP));

      pos = s.len - 4;
      while (pos && s.data[pos] != '.')
        pos--;
      pos++; /* assuming, but that's fine. */
      tmp = http_mimetype_find(s.data + pos, s.len - pos - 3);

    } else {
      pos = s.len - 1;
      while (pos && s.data[pos] != '.')
        pos--;
      pos++; /* assuming, but that's fine. */
      tmp = http_mimetype_find(s.data + pos, s.len - pos);
    }
    if (tmp)
      http_set_header(h, HTTP_HEADER_CONTENT_TYPE, tmp);
  }
  http_sendfile(h, file, length, offset);
  return 0;
}

/**
 * Sends an HTTP error response.
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 *
 * The `uuid` argument is optional and will be used only if the `http_s`
 * argument is set to NULL.
 */
int http_send_error(http_s *r, size_t error) {
  if (!r || !r->private_data.out_headers) {
    return -1;
  }
  if (error < 100 || error >= 1000)
    error = 500;
  r->status = error;
  char buffer[16];
  buffer[0] = '/';
  size_t pos = 1 + fio_ltoa(buffer + 1, error, 10);
  buffer[pos++] = '.';
  buffer[pos++] = 'h';
  buffer[pos++] = 't';
  buffer[pos++] = 'm';
  buffer[pos++] = 'l';
  buffer[pos] = 0;
  if (http_sendfile2(r, http2protocol(r)->settings->public_folder,
                     http2protocol(r)->settings->public_folder_length, buffer,
                     pos)) {
    http_set_header(r, HTTP_HEADER_CONTENT_TYPE, http_mimetype_find("txt", 3));
    fio_cstr_s t = http_status2str(error);
    http_send_body(r, t.data, t.len);
  }
  return 0;
}

/**
 * Sends the response headers for a header only response.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
void http_finish(http_s *r) {
  if (!r || !r->private_data.vtbl) {
    return;
  }
  add_content_length(r, 0);
  add_date(r);
  ((http_vtable_s *)r->private_data.vtbl)->http_finish(r);
}
/**
 * Pushes a data response when supported (HTTP/2 only).
 *
 * Returns -1 on error and 0 on success.
 */
int http_push_data(http_s *r, void *data, uintptr_t length, FIOBJ mime_type) {
  if (!r || !(http_protocol_s *)r->private_data.flag)
    return -1;
  return ((http_vtable_s *)r->private_data.vtbl)
      ->http_push_data(r, data, length, mime_type);
}
/**
 * Pushes a file response when supported (HTTP/2 only).
 *
 * If `mime_type` is NULL, an attempt at automatic detection using
 * `filename` will be made.
 *
 * Returns -1 on error and 0 on success.
 */
int http_push_file(http_s *h, FIOBJ filename, FIOBJ mime_type) {
  if (HTTP_INVALID_HANDLE(h))
    return -1;
  return ((http_vtable_s *)h->private_data.vtbl)
      ->http_push_file(h, filename, mime_type);
}

/**
 * Upgrades an HTTP/1.1 connection to a Websocket connection.
 */
#undef http_upgrade2ws
int http_upgrade2ws(websocket_settings_s args) {
  if (!args.http) {
    fprintf(stderr,
            "ERROR: `http_upgrade2ws` requires a valid `http_s` handle.");
    return -1;
  }
  if (!args.http->headers)
    return -1;
  return ((http_vtable_s *)args.http->private_data.vtbl)->http2websocket(&args);
}

/* *****************************************************************************
Pause / Resume
***************************************************************************** */
typedef struct {
  uintptr_t uuid;
  http_s *h;
  void *udata;
  void (*task)(http_s *);
  void (*fallback)(void *);
} http_pause_handle_s;

/** Returns the `udata` associated with the paused opaque handle */
void *http_paused_udata_get(void *http_) {
  const http_pause_handle_s *http = http_;
  return http->udata;
}

/**
 * Sets the `udata` associated with the paused opaque handle, returning the
 * old value.
 */
void *http_paused_udata_set(void *http_, void *udata) {
  http_pause_handle_s *http = http_;
  void *old = http->udata;
  http->udata = udata;
  return old;
}

/* perform the pause task outside of the connection's lock */
static void http_pause_wrapper(void *h_, void *task_) {
  void (*task)(void *h) = (void (*)(void *h))((uintptr_t)task_);
  task(h_);
}

/* perform the resume task within of the connection's lock */
static void http_resume_wrapper(intptr_t uuid, protocol_s *p_, void *arg) {
  http_protocol_s *p = (http_protocol_s *)p_;
  http_pause_handle_s *http = arg;
  http_s *h = http->h;
  h->udata = http->udata;
  http_vtable_s *vtbl = (http_vtable_s *)h->private_data.vtbl;
  if (http->task)
    http->task(h);
  vtbl->http_on_resume(h, p);
  fio_free(http);
  (void)uuid;
}

/* perform the resume task fallback */
static void http_resume_fallback_wrapper(intptr_t uuid, void *arg) {
  http_pause_handle_s *http = arg;
  if (http->fallback)
    http->fallback(http->udata);
  fio_free(http);
  (void)uuid;
}

/**
 * Defers the request / response handling for later.
 */
void http_pause(http_s *h, void (*task)(void *http)) {
  if (HTTP_INVALID_HANDLE(h)) {
    return;
  }
  http_protocol_s *p = (http_protocol_s *)h->private_data.flag;
  http_vtable_s *vtbl = (http_vtable_s *)h->private_data.vtbl;
  http_pause_handle_s *http = fio_malloc(sizeof(*http));
  *http = (http_pause_handle_s){
      .uuid = p->uuid, .h = h, .udata = h->udata,
  };
  vtbl->http_on_pause(h, p);
  defer(http_pause_wrapper, http, (void *)((uintptr_t)task));
}

/**
 * Defers the request / response handling for later.
 */
void http_resume(void *http_, void (*task)(http_s *h),
                 void (*fallback)(void *udata)) {
  if (!http_)
    return;
  http_pause_handle_s *http = http_;
  http->task = task;
  http->fallback = fallback;
  facil_defer(.uuid = http->uuid, .arg = http, .type = FIO_PR_LOCK_TASK,
              .task = http_resume_wrapper,
              .fallback = http_resume_fallback_wrapper);
}

/**
 * Hijacks the socket away from the HTTP protocol and away from facil.io.
 */
intptr_t http_hijack(http_s *h, fio_cstr_s *leftover) {
  if (!h)
    return -1;
  return ((http_vtable_s *)h->private_data.vtbl)->http_hijack(h, leftover);
}

/* *****************************************************************************
Setting the default settings and allocating a persistent copy
***************************************************************************** */

static void http_on_request_fallback(http_s *h) { http_send_error(h, 404); }
static void http_on_upgrade_fallback(http_s *h, char *p, size_t i) {
  http_send_error(h, 400);
  (void)p;
  (void)i;
}
static void http_on_response_fallback(http_s *h) { http_send_error(h, 400); }

http_settings_s *http_settings_new(http_settings_s arg_settings) {
  if (!arg_settings.on_request)
    arg_settings.on_request = http_on_request_fallback;
  if (!arg_settings.on_response)
    arg_settings.on_response = http_on_response_fallback;
  if (!arg_settings.on_upgrade)
    arg_settings.on_upgrade = http_on_upgrade_fallback;

  if (!arg_settings.max_body_size)
    arg_settings.max_body_size = HTTP_DEFAULT_BODY_LIMIT;
  if (!arg_settings.timeout)
    arg_settings.timeout = 5;
  if (!arg_settings.ws_max_msg_size)
    arg_settings.ws_max_msg_size = 262144; /** defaults to ~250KB */
  if (!arg_settings.ws_timeout)
    arg_settings.ws_timeout = 40; /* defaults to 40 seconds */
  if (!arg_settings.max_header_size)
    arg_settings.max_header_size = 32 * 1024; /* defaults to 32Kib seconds */
  if (arg_settings.max_clients <= 0 ||
      arg_settings.max_clients + HTTP_BUSY_UNLESS_HAS_FDS >
          sock_max_capacity()) {
    arg_settings.max_clients = sock_max_capacity();
    if ((ssize_t)arg_settings.max_clients - HTTP_BUSY_UNLESS_HAS_FDS > 0)
      arg_settings.max_clients -= HTTP_BUSY_UNLESS_HAS_FDS;
  }

  http_settings_s *settings = malloc(sizeof(*settings) + sizeof(void *));
  *settings = arg_settings;

  if (settings->public_folder) {
    settings->public_folder_length = strlen(settings->public_folder);
    if (settings->public_folder[0] == '~' &&
        settings->public_folder[1] == '/' && getenv("HOME")) {
      char *home = getenv("HOME");
      size_t home_len = strlen(home);
      char *tmp = malloc(settings->public_folder_length + home_len + 1);
      memcpy(tmp, home, home_len);
      if (home[home_len - 1] == '/')
        --home_len;
      memcpy(tmp + home_len, settings->public_folder + 1,
             settings->public_folder_length); // copy also the NULL
      settings->public_folder = tmp;
      settings->public_folder_length = strlen(settings->public_folder);
    } else {
      settings->public_folder = malloc(settings->public_folder_length + 1);
      memcpy((void *)settings->public_folder, arg_settings.public_folder,
             settings->public_folder_length);
      ((uint8_t *)settings->public_folder)[settings->public_folder_length] = 0;
    }
  }
  return settings;
}
/* *****************************************************************************
Listening to HTTP connections
***************************************************************************** */

static void http_on_open(intptr_t uuid, void *set) {
  static uint8_t at_capa;
  facil_set_timeout(uuid, ((http_settings_s *)set)->timeout);
  if (sock_uuid2fd(uuid) >= ((http_settings_s *)set)->max_clients) {
    if (!at_capa)
      fprintf(stderr, "WARNING: HTTP server at capacity\n");
    at_capa = 1;
    http_send_error2(uuid, 503, set);
    sock_close(uuid);
    return;
  }
  at_capa = 0;
  protocol_s *pr = http1_new(uuid, set, NULL, 0);
  if (!pr)
    sock_close(uuid);
}

static void http_on_finish(intptr_t uuid, void *set) {
  http_settings_s *settings = set;

  if (settings->on_finish)
    settings->on_finish(settings);

  free((void *)settings->public_folder);
  free(settings);
  (void)uuid;
}

/**
 * Listens to HTTP connections at the specified `port`.
 *
 * Leave as NULL to ignore IP binding.
 *
 * Returns -1 on error and 0 on success.
 */
#undef http_listen
int http_listen(const char *port, const char *binding,
                struct http_settings_s arg_settings) {
  if (arg_settings.on_request == NULL) {
    fprintf(stderr, "ERROR: http_listen requires the .on_request parameter "
                    "to be set\n");
    kill(0, SIGINT);
    exit(11);
  }

  http_settings_s *settings = http_settings_new(arg_settings);
  settings->is_client = 0;

  return facil_listen(.port = port, .address = binding,
                      .on_finish = http_on_finish, .on_open = http_on_open,
                      .udata = settings);
}
/** Listens to HTTP connections at the specified `port` and `binding`. */
#define http_listen(port, binding, ...)                                        \
  http_listen((port), (binding), (struct http_settings_s)(__VA_ARGS__))

/**
 * Returns the settings used to setup the connection.
 *
 * Returns NULL on error (i.e., connection was lost).
 */
struct http_settings_s *http_settings(http_s *r) {
  return ((http_protocol_s *)r->private_data.flag)->settings;
}

/**
 * Returns the direct address of the connected peer (likely an intermediary).
 */
sock_peer_addr_s http_peer_addr(http_s *h) {
  return sock_peer_addr(((http_protocol_s *)h->private_data.flag)->uuid);
}

/* *****************************************************************************
HTTP client connections
***************************************************************************** */

static void http_on_close_client(intptr_t uuid, protocol_s *protocol) {
  http_protocol_s *p = (http_protocol_s *)protocol;
  http_settings_s *set = p->settings;
  void (**original)(intptr_t, protocol_s *) =
      (void (**)(intptr_t, protocol_s *))(set + 1);
  if (set->on_finish)
    set->on_finish(set);

  original[0](uuid, protocol);
  free((void *)set->public_folder);
  free(set);
}

static void http_on_open_client(intptr_t uuid, void *set_) {
  http_settings_s *set = set_;
  http_s *h = set->udata;
  set->udata = h->udata;
  facil_set_timeout(uuid, set->timeout);
  protocol_s *pr = http1_new(uuid, set, NULL, 0);
  if (!pr) {
    sock_close(uuid);
    return;
  }
  { /* store the original on_close at the end of the struct, we wrap it. */
    void (**original)(intptr_t, protocol_s *) =
        (void (**)(intptr_t, protocol_s *))(set + 1);
    *original = pr->on_close;
    pr->on_close = http_on_close_client;
  }
  h->private_data.flag = (uintptr_t)pr;
  h->private_data.vtbl = http1_vtable();
  set->on_response(h);
}

static void http_on_client_failed(intptr_t uuid, void *set_) {
  http_settings_s *set = set_;
  http_s *h = set->udata;
  set->udata = h->udata;
  http_s_destroy(h, 0);
  fio_free(h);
  if (set->on_finish)
    set->on_finish(set);
  free((void *)set->public_folder);
  free(set);
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
#undef http_connect
int http_connect(const char *address, struct http_settings_s arg_settings) {
  if (!arg_settings.on_response && !arg_settings.on_upgrade) {
    fprintf(stderr, "ERROR: http_connect requires either an on_response "
                    " or an on_upgrade callback.\n");
    errno = EINVAL;
    return -1;
  }
  size_t len;
  char *a, *p;
  uint8_t is_websocket = 0;
  uint8_t is_secure = 0;
  FIOBJ path = FIOBJ_INVALID;
  if (!address || (len = strlen(address)) <= 5) {
    fprintf(stderr, "ERROR: http_connect requires a valid address.\n");
    errno = EINVAL;
    return -1;
  }
  if (!strncasecmp(address, "ws", 2)) {
    is_websocket = 1;
    address += 2;
    len -= 2;
  } else if (len >= 7 && !strncasecmp(address, "http", 4)) {
    address += 4;
    len -= 4;
  } else {
    fprintf(stderr, "ERROR: http_connect requires a valid address.\n");
    errno = EINVAL;
    return -1;
  }
  /* parse address */
  if (address[0] == 's') {
    /* TODO: SSL/TLS */
    is_secure = 1;
    fprintf(stderr, "ERROR: http_connect doesn't support TLS/SSL "
                    "just yet.\n");
    errno = EINVAL;
    return -1;
  } else if (len <= 3 || strncmp(address, "://", 3)) {
    fprintf(stderr, "ERROR: http_connect requires a valid address.\n");
    errno = EINVAL;
    return -1;
  } else {
    len -= 3;
    address += 3;
    a = fio_malloc(len + 1);
    if (!a) {
      perror("FATAL ERROR: http_connect couldn't allocate memory "
             "for address parsing");
    }
    memcpy(a, address, len + 1);
  }
  p = memchr(a, '/', len);
  if (p) {
    if (len - (p - a))
      path = fiobj_str_new(p, len - (p - a));
    len = p - a;
    *p = 0;
  }
  p = memchr(a, ':', len);
  if (p) {
    len = p - a;
    *p = 0;
    if (a + len == p + 1) {
      p = NULL;
    } else {
      p++;
      if (!len) {
        a = "localhost";
        len = 9;
      }
    }
  } else {
    if (is_secure)
      p = "443";
    else
      p = "80";
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
  http_s *h = fio_malloc(sizeof(*h));
  HTTP_ASSERT(h, "HTTP Client handler allocation failed");
  http_s_new(h, 0, http1_vtable());
  h->udata = arg_settings.udata;
  h->status = 0;
  h->path = path;
  settings->udata = h;
  http_set_header2(h, (fio_cstr_s){.data = "host", .len = 4},
                   (fio_cstr_s){.data = a, .len = len});
  intptr_t ret;
  if (is_websocket) {
    /* force HTTP/1.1 */
    ret =
        facil_connect(.address = a, .port = p, .on_fail = http_on_client_failed,
                      .on_connect = http_on_open_client, .udata = settings);
    (void)0;
  } else {
    /* Allow for any HTTP version */
    ret =
        facil_connect(.address = a, .port = p, .on_fail = http_on_client_failed,
                      .on_connect = http_on_open_client, .udata = settings);
    (void)0;
  }
  fio_free(a);
  return ret;
}
#define http_connect(address, ...)                                             \
  http_connect((address), (struct http_settings_s){__VA_ARGS__})

/* *****************************************************************************
HTTP Websocket Connect
***************************************************************************** */

#undef http_upgrade2ws
static void on_websocket_http_connected(http_s *h) {
  websocket_settings_s *s = h->udata;
  h->udata = http_settings(h)->udata = NULL;
  s->http = h;
  if (!h->path) {
    fprintf(stderr, "WARNING: (websocket client) path not specified in "
                    "address, assuming root!\n");
    h->path = fiobj_str_new("/", 1);
  }
  http_upgrade2ws(*s);
  free(s);
}

static void on_websocket_http_connection_finished(http_settings_s *settings) {
  websocket_settings_s *s = settings->udata;
  if (s) {
    if (s->on_close)
      s->on_close(0, s->udata);
    free(s);
  }
}

#undef websocket_connect
int websocket_connect(const char *address, websocket_settings_s settings) {
  websocket_settings_s *s = malloc(sizeof(*s));
  *s = settings;
  return http_connect(address, .on_request = on_websocket_http_connected,
                      .on_response = on_websocket_http_connected,
                      .on_finish = on_websocket_http_connection_finished,
                      .udata = s);
}
#define websocket_connect(address, ...)                                        \
  websocket_connect((address), (websocket_settings_s){__VA_ARGS__})

/* *****************************************************************************
HTTP GET and POST parsing helpers
***************************************************************************** */

/** URL decodes a string, returning a `FIOBJ`. */
static inline FIOBJ http_urlstr2fiobj(char *s, size_t len) {
  FIOBJ o = fiobj_str_buf(len);
  ssize_t l = http_decode_url(fiobj_obj2cstr(o).data, s, len);
  if (l < 0) {
    fiobj_free(o);
    return fiobj_str_new(NULL, 0); /* empty string */
  }
  fiobj_str_resize(o, (size_t)l);
  return o;
}

/** converts a string into a `FIOBJ`. */
static inline FIOBJ http_str2fiobj(char *s, size_t len, uint8_t encoded) {
  switch (len) {
  case 0:
    return fiobj_str_new(NULL, 0); /* empty string */
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
  return fiobj_str_new(s, len);
}

/** Parses the query part of an HTTP request/response. Uses `http_add2hash`. */
void http_parse_query(http_s *h) {
  if (!h->query)
    return;
  if (!h->params)
    h->params = fiobj_hash_new();
  fio_cstr_s q = fiobj_obj2cstr(h->query);
  do {
    char *cut = memchr(q.data, '&', q.len);
    if (!cut)
      cut = q.data + q.len;
    char *cut2 = memchr(q.data, '=', (cut - q.data));
    if (cut2) {
      /* we only add named elements... */
      http_add2hash(h->params, q.data, (size_t)(cut2 - q.data), (cut2 + 1),
                    (size_t)(cut - (cut2 + 1)), 1);
    }
    if (cut[0] == '&') {
      /* protecting against some ...less informed... clients */
      if (cut[1] == 'a' && cut[2] == 'm' && cut[3] == 'p' && cut[4] == ';')
        cut += 5;
      else
        cut += 1;
    }
    q.len -= (uintptr_t)(cut - q.data);
    q.data = cut;
  } while (q.len);
}

static inline void http_parse_cookies_cookie_str(FIOBJ dest, FIOBJ str,
                                                 uint8_t is_url_encoded) {
  if (!FIOBJ_TYPE_IS(str, FIOBJ_T_STRING))
    return;
  fio_cstr_s s = fiobj_obj2cstr(str);
  while (s.length) {
    if (s.data[0] == ' ') {
      ++s.data;
      --s.len;
      continue;
    }
    char *cut = memchr(s.data, '=', s.len);
    if (!cut)
      cut = s.data;
    char *cut2 = memchr(cut, ';', s.len - (cut - s.data));
    if (!cut2)
      cut2 = s.data + s.len;
    http_add2hash(dest, s.data, cut - s.data, cut + 1, (cut2 - (cut + 1)),
                  is_url_encoded);
    if ((size_t)((cut2 + 1) - s.data) > s.length)
      s.length = 0;
    else
      s.length -= ((cut2 + 1) - s.data);
    s.data = cut2 + 1;
  }
}
static inline void http_parse_cookies_setcookie_str(FIOBJ dest, FIOBJ str,
                                                    uint8_t is_url_encoded) {
  if (!FIOBJ_TYPE_IS(str, FIOBJ_T_STRING))
    return;
  fio_cstr_s s = fiobj_obj2cstr(str);
  char *cut = memchr(s.data, '=', s.len);
  if (!cut)
    cut = s.data;
  char *cut2 = memchr(cut, ';', s.len - (cut - s.data));
  if (!cut2)
    cut2 = s.data + s.len;
  if (cut2 > cut)
    http_add2hash(dest, s.data, cut - s.data, cut + 1, (cut2 - (cut + 1)),
                  is_url_encoded);
}

/** Parses any Cookie / Set-Cookie headers, using the `http_add2hash` scheme. */
void http_parse_cookies(http_s *h, uint8_t is_url_encoded) {
  if (!h->headers)
    return;
  if (h->cookies && fiobj_hash_count(h->cookies)) {
    fprintf(stderr,
            "WARNING: (http) attempting to parse cookies more than once.\n");
    return;
  }
  static uint64_t setcookie_header_hash;
  if (!setcookie_header_hash)
    setcookie_header_hash = fiobj_obj2hash(HTTP_HEADER_SET_COOKIE);
  FIOBJ c = fiobj_hash_get2(h->headers, fiobj_obj2hash(HTTP_HEADER_COOKIE));
  if (c) {
    if (!h->cookies)
      h->cookies = fiobj_hash_new();
    if (FIOBJ_TYPE_IS(c, FIOBJ_T_ARRAY)) {
      /* Array of Strings */
      size_t count = fiobj_ary_count(c);
      for (size_t i = 0; i < count; ++i) {
        http_parse_cookies_cookie_str(
            h->cookies, fiobj_ary_index(c, (int64_t)i), is_url_encoded);
      }
    } else {
      /* single string */
      http_parse_cookies_cookie_str(h->cookies, c, is_url_encoded);
    }
  }
  c = fiobj_hash_get2(h->headers, fiobj_obj2hash(HTTP_HEADER_SET_COOKIE));
  if (c) {
    if (!h->cookies)
      h->cookies = fiobj_hash_new();
    if (FIOBJ_TYPE_IS(c, FIOBJ_T_ARRAY)) {
      /* Array of Strings */
      size_t count = fiobj_ary_count(c);
      for (size_t i = 0; i < count; ++i) {
        http_parse_cookies_setcookie_str(
            h->cookies, fiobj_ary_index(c, (int64_t)i), is_url_encoded);
      }
    } else {
      /* single string */
      http_parse_cookies_setcookie_str(h->cookies, c, is_url_encoded);
    }
  }
}

/**
 * Adds a named parameter to the hash, resolving nesting references.
 *
 * i.e.:
 *
 * * "name[]" references a nested Array (nested in the Hash).
 * * "name[key]" references a nested Hash.
 * * "name[][key]" references a nested Hash within an array. Hash keys will be
 *   unique (repeating a key advances the hash).
 * * These rules can be nested (i.e. "name[][key1][][key2]...")
 * * "name[][]" is an error (there's no way for the parser to analyse
 *    dimentions)
 *
 * Note: names can't begine with "[" or end with "]" as these are reserved
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
    const uint64_t hash = fio_siphash(name, len); /* hash the current name */
    nested_ary = fiobj_hash_get2(dest, hash);
    if (!nested_ary) {
      /* create a new nested array */
      FIOBJ key =
          encoded ? http_urlstr2fiobj(name, len) : fiobj_str_new(name, len);
      nested_ary = fiobj_ary_new2(4);
      fiobj_hash_set(dest, key, nested_ary);
      fiobj_free(key);
    } else if (!FIOBJ_TYPE_IS(nested_ary, FIOBJ_T_ARRAY)) {
      /* convert existing object to an array - auto error correction */
      FIOBJ key =
          encoded ? http_urlstr2fiobj(name, len) : fiobj_str_new(name, len);
      FIOBJ tmp = fiobj_ary_new2(4);
      fiobj_ary_push(tmp, nested_ary);
      nested_ary = tmp;
      fiobj_hash_set(dest, key, nested_ary);
      fiobj_free(key);
    }

    /* test if last object in the array is a hash - create hash if not */
    dest = fiobj_ary_index(nested_ary, -1);
    if (!dest || !FIOBJ_TYPE_IS(dest, FIOBJ_T_HASH)) {
      dest = fiobj_hash_new();
      fiobj_ary_push(nested_ary, dest);
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
    const uint64_t hash = fio_siphash(name, len); /* hash the current name */
    FIOBJ tmp = fiobj_hash_get2(dest, hash);
    if (!tmp) {
      /* hash doesn't exist, create it */
      FIOBJ key =
          encoded ? http_urlstr2fiobj(name, len) : fiobj_str_new(name, len);
      tmp = fiobj_hash_new();
      fiobj_hash_set(dest, key, tmp);
      fiobj_free(key);
    } else if (!FIOBJ_TYPE_IS(tmp, FIOBJ_T_HASH)) {
      /* type error, referencing an existing object that isn't a Hash */
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
  FIOBJ key = encoded ? http_urlstr2fiobj(name, name_len)
                      : fiobj_str_new(name, name_len);
  FIOBJ old = fiobj_hash_replace(dest, key, val);
  if (old) {
    if (nested_ary) {
      fiobj_hash_replace(dest, key, old);
      old = fiobj_hash_new();
      fiobj_hash_set(old, key, val);
      fiobj_ary_push(nested_ary, old);
    } else {
      if (!FIOBJ_TYPE_IS(old, FIOBJ_T_ARRAY)) {
        FIOBJ tmp = fiobj_ary_new2(4);
        fiobj_ary_push(tmp, old);
        old = tmp;
      }
      fiobj_ary_push(old, val);
      fiobj_hash_replace(dest, key, old);
    }
  }
  fiobj_free(key);
  return 0;

place_in_array:
  if (name[name_len - 1] == ']')
    --name_len;
  uint64_t hash = fio_siphash(name, name_len);
  FIOBJ ary = fiobj_hash_get2(dest, hash);
  if (!ary) {
    FIOBJ key = encoded ? http_urlstr2fiobj(name, name_len)
                        : fiobj_str_new(name, name_len);
    ary = fiobj_ary_new2(4);
    fiobj_hash_set(dest, key, ary);
    fiobj_free(key);
  } else if (!FIOBJ_TYPE_IS(ary, FIOBJ_T_ARRAY)) {
    FIOBJ tmp = fiobj_ary_new2(4);
    fiobj_ary_push(tmp, ary);
    ary = tmp;
    FIOBJ key = encoded ? http_urlstr2fiobj(name, name_len)
                        : fiobj_str_new(name, name_len);
    fiobj_hash_replace(dest, key, ary);
    fiobj_free(key);
  }
  fiobj_ary_push(ary, val);
  return 0;
error:
  fiobj_free(val);
  errno = EOPNOTSUPP;
  return -1;
}

/**
 * Adds a named parameter to the hash, resolving nesting references.
 *
 * i.e.:
 *
 * * "name[]" references a nested Array (nested in the Hash).
 * * "name[key]" references a nested Hash.
 * * "name[][key]" references a nested Hash within an array. Hash keys will be
 *   unique (repeating a key advances the hash).
 * * These rules can be nested (i.e. "name[][key1][][key2]...")
 * * "name[][]" is an error (there's no way for the parser to analyse
 *    dimentions)
 *
 * Note: names can't begine with "[" or end with "]" as these are reserved
 *       characters.
 */
int http_add2hash(FIOBJ dest, char *name, size_t name_len, char *value,
                  size_t value_len, uint8_t encoded) {
  return http_add2hash2(dest, name, name_len,
                        http_str2fiobj(value, value_len, encoded), encoded);
}

/* *****************************************************************************
HTTP Body Parsing
***************************************************************************** */
#include "http_mime_parser.h"

typedef struct {
  http_mime_parser_s p;
  http_s *h;
  fio_cstr_s buffer;
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
  if (!filename) {
    http_add2hash(http_mime_parser2fio(parser)->h->params, name, name_len,
                  value, value_len, 0);
    return;
  }
  FIOBJ n = fiobj_str_new(name, name_len);
  fiobj_str_write(n, "[data]", 6);
  fio_cstr_s tmp = fiobj_obj2cstr(n);
  http_add2hash(http_mime_parser2fio(parser)->h->params, tmp.data, tmp.len,
                value, value_len, 0);
  fiobj_str_resize(n, name_len);
  fiobj_str_write(n, "[type]", 6);
  tmp = fiobj_obj2cstr(n);
  http_add2hash(http_mime_parser2fio(parser)->h->params, tmp.data, tmp.len,
                mimetype, mimetype_len, 0);
  fiobj_str_resize(n, name_len);
  fiobj_str_write(n, "[name]", 6);
  tmp = fiobj_obj2cstr(n);
  fprintf(stderr, "filename length %zu\n", filename_len);
  http_add2hash(http_mime_parser2fio(parser)->h->params, tmp.data, tmp.len,
                filename, filename_len, 0);
  fiobj_free(n);
}

/** Called when the data didn't fit in the buffer. Data will be streamed. */
static void http_mime_parser_on_partial_start(
    http_mime_parser_s *parser, void *name, size_t name_len, void *filename,
    size_t filename_len, void *mimetype, size_t mimetype_len) {
  http_mime_parser2fio(parser)->partial_length = 0;
  http_mime_parser2fio(parser)->partial_offset = 0;
  http_mime_parser2fio(parser)->partial_name = fiobj_str_new(name, name_len);

  if (!filename)
    return;

  fiobj_str_write(http_mime_parser2fio(parser)->partial_name, "[type]", 6);
  fio_cstr_s tmp = fiobj_obj2cstr(http_mime_parser2fio(parser)->partial_name);
  http_add2hash(http_mime_parser2fio(parser)->h->params, tmp.data, tmp.len,
                mimetype, mimetype_len, 0);

  fiobj_str_resize(http_mime_parser2fio(parser)->partial_name, name_len);
  fiobj_str_write(http_mime_parser2fio(parser)->partial_name, "[name]", 6);
  tmp = fiobj_obj2cstr(http_mime_parser2fio(parser)->partial_name);
  http_add2hash(http_mime_parser2fio(parser)->h->params, tmp.data, tmp.len,
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
         (uintptr_t)http_mime_parser2fio(parser)->buffer.data);
  http_mime_parser2fio(parser)->partial_length += value_len;
  (void)value;
}

/** Called when the partial data is complete. */
static void http_mime_parser_on_partial_end(http_mime_parser_s *parser) {

  fio_cstr_s tmp = fiobj_obj2cstr(http_mime_parser2fio(parser)->partial_name);
  http_add2hash2(http_mime_parser2fio(parser)->h->params, tmp.data, tmp.len,
                 fiobj_data_slice(http_mime_parser2fio(parser)->h->body,
                                  http_mime_parser2fio(parser)->partial_offset,
                                  http_mime_parser2fio(parser)->partial_length),
                 0);
  fiobj_free(http_mime_parser2fio(parser)->partial_name);
  http_mime_parser2fio(parser)->partial_name = FIOBJ_INVALID;
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
 */
int http_parse_body(http_s *h) {
  static uint64_t content_type_hash;
  if (!h->body)
    return -1;
  if (!content_type_hash)
    content_type_hash = fio_siphash("content-type", 12);
  FIOBJ ct = fiobj_hash_get2(h->headers, content_type_hash);
  fio_cstr_s content_type = fiobj_obj2cstr(ct);
  if (content_type.len < 16)
    return -1;
  if (content_type.len >= 33 &&
      !strncasecmp("application/x-www-form-urlencoded", content_type.data,
                   33)) {
    if (!h->params)
      h->params = fiobj_hash_new();
    FIOBJ tmp = h->query;
    h->query = h->body;
    http_parse_query(h);
    h->query = tmp;
    return 0;
  }
  if (content_type.len >= 16 &&
      !strncasecmp("application/json", content_type.data, 16)) {
    if (h->params)
      return -1;
    content_type = fiobj_obj2cstr(h->body);
    if (fiobj_json2obj(&h->params, content_type.data, content_type.len) == 0)
      return -1;
    if (FIOBJ_TYPE_IS(h->params, FIOBJ_T_HASH))
      return 0;
    fiobj_free(h->params);
    h->params = FIOBJ_INVALID;
    return -1;
  }

  http_fio_mime_s p = {.h = h};
  if (http_mime_parser_init(&p.p, content_type.data, content_type.len))
    return -1;
  if (!h->params)
    h->params = fiobj_hash_new();

  do {
    size_t cons = http_mime_parse(&p.p, p.buffer.data, p.buffer.len);
    p.pos += cons;
    p.buffer = fiobj_data_pread(h->body, p.pos, 256);
  } while (p.buffer.data && !p.p.done && !p.p.error);
  fiobj_free(p.partial_name);
  p.partial_name = FIOBJ_INVALID;
  return 0;
}

/* *****************************************************************************
HTTP Helper functions that could be used globally
***************************************************************************** */

/**
 * Returns a String object representing the unparsed HTTP request (HTTP
 * version is capped at HTTP/1.1). Mostly usable for proxy usage and
 * debugging.
 */
FIOBJ http_req2str(http_s *h) {
  if (HTTP_INVALID_HANDLE(h) || !fiobj_hash_count(h->headers))
    return FIOBJ_INVALID;

  struct header_writer_s w;
  w.dest = fiobj_str_buf(0);
  if (h->status_str) {
    fiobj_str_join(w.dest, h->version);
    fiobj_str_write(w.dest, " ", 1);
    fiobj_str_join(w.dest, fiobj_num_tmp(h->status));
    fiobj_str_write(w.dest, " ", 1);
    fiobj_str_join(w.dest, h->status_str);
    fiobj_str_write(w.dest, "\r\n", 2);
  } else {
    fiobj_str_join(w.dest, h->method);
    fiobj_str_write(w.dest, " ", 1);
    fiobj_str_join(w.dest, h->path);
    if (h->query) {
      fiobj_str_write(w.dest, "?", 1);
      fiobj_str_join(w.dest, h->query);
    }
    {
      fio_cstr_s t = fiobj_obj2cstr(h->version);
      if (t.len < 6 || t.data[5] != '1')
        fiobj_str_write(w.dest, " HTTP/1.1\r\n", 10);
      else {
        fiobj_str_write(w.dest, " ", 1);
        fiobj_str_join(w.dest, h->version);
        fiobj_str_write(w.dest, "\r\n", 2);
      }
    }
  }

  fiobj_each1(h->headers, 0, write_header, &w);
  fiobj_str_write(w.dest, "\r\n", 2);
  if (h->body) {
    // fiobj_data_seek(h->body, 0);
    // fio_cstr_s t = fiobj_data_read(h->body, 0);
    // fiobj_str_write(w.dest, t.data, t.len);
    fiobj_str_join(w.dest, h->body);
  }
  return w.dest;
}

void http_write_log(http_s *h) {
  FIOBJ l = fiobj_str_buf(128);
  static uint64_t cl_hash = 0;
  if (!cl_hash)
    cl_hash = fio_siphash("content-length", 14);

  intptr_t bytes_sent =
      fiobj_obj2num(fiobj_hash_get2(h->private_data.out_headers, cl_hash));

  struct timespec start, end;
  clock_gettime(CLOCK_REALTIME, &end);
  start = facil_last_tick();

  fio_cstr_s buff = fiobj_obj2cstr(l);

  // TODO Guess IP address from headers (forwarded) where possible
  sock_peer_addr_s addrinfo = sock_peer_addr(
      sock_uuid2fd(((http_protocol_s *)h->private_data.flag)->uuid));
  if (addrinfo.addrlen) {
    if (inet_ntop(
            addrinfo.addr->sa_family,
            addrinfo.addr->sa_family == AF_INET
                ? (void *)&((struct sockaddr_in *)addrinfo.addr)->sin_addr
                : (void *)&((struct sockaddr_in6 *)addrinfo.addr)->sin6_addr,
            buff.data, 128))
      buff.len = strlen(buff.data);
  }
  if (buff.len == 0) {
    memcpy(buff.data, "[unknown]", 9);
    buff.len = 9;
  }
  memcpy(buff.data + buff.len, " - - [", 6);
  buff.len += 6;
  fiobj_str_resize(l, buff.len);
  {
    FIOBJ date;
    spn_lock(&date_lock);
    date = fiobj_dup(current_date);
    spn_unlock(&date_lock);
    fiobj_str_join(l, current_date);
    fiobj_free(date);
  }
  fiobj_str_write(l, "] \"", 3);
  fiobj_str_join(l, h->method);
  fiobj_str_write(l, " ", 1);
  fiobj_str_join(l, h->path);
  fiobj_str_write(l, " ", 1);
  fiobj_str_join(l, h->version);
  fiobj_str_write(l, "\" ", 2);
  if (bytes_sent > 0) {
    fiobj_str_join(l, fiobj_num_tmp(h->status));
    fiobj_str_write(l, " ", 1);
    fiobj_str_join(l, fiobj_num_tmp(bytes_sent));
    fiobj_str_write(l, "b ", 2);
  } else {
    fiobj_str_join(l, fiobj_num_tmp(h->status));
    fiobj_str_write(l, " -- ", 4);
  }

  bytes_sent = ((end.tv_sec - start.tv_sec) * 1000) +
               ((end.tv_nsec - start.tv_nsec) / 1000000);
  fiobj_str_join(l, fiobj_num_tmp(bytes_sent));
  fiobj_str_write(l, "ms\r\n", 4);

  buff = fiobj_obj2cstr(l);

  fwrite(buff.data, 1, buff.len, stderr);
  fiobj_free(l);
}

/**
A faster (yet less localized) alternative to `gmtime_r`.

See the libc `gmtime_r` documentation for details.

Falls back to `gmtime_r` for dates before epoch.
*/
struct tm *http_gmtime(time_t timer, struct tm *tmbuf) {
  // static char* DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri",
  // "Sat"}; static char * Months = {  "Jan", "Feb", "Mar", "Apr", "May",
  // "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  static const uint8_t month_len[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, // nonleap year
      31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31  // leap year
  };
  if (timer < 0)
    return gmtime_r(&timer, tmbuf);
  ssize_t a, b;
  tmbuf->tm_gmtoff = 0;
  tmbuf->tm_zone = "UTC";
  tmbuf->tm_isdst = 0;
  tmbuf->tm_year = 70; // tm_year == The number of years since 1900
  tmbuf->tm_mon = 0;
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

static const char *DAY_NAMES[] = {"Sun", "Mon", "Tue", "Wed",
                                  "Thu", "Fri", "Sat"};
static const char *MONTH_NAMES[] = {"Jan ", "Feb ", "Mar ", "Apr ",
                                    "May ", "Jun ", "Jul ", "Aug ",
                                    "Sep ", "Oct ", "Nov ", "Dec "};
static const char *GMT_STR = "GMT";

size_t http_date2str(char *target, struct tm *tmbuf) {
  char *pos = target;
  uint16_t tmp;
  pos[0] = DAY_NAMES[tmbuf->tm_wday][0];
  pos[1] = DAY_NAMES[tmbuf->tm_wday][1];
  pos[2] = DAY_NAMES[tmbuf->tm_wday][2];
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
  *(pos++) = ' ';
  pos[0] = MONTH_NAMES[tmbuf->tm_mon][0];
  pos[1] = MONTH_NAMES[tmbuf->tm_mon][1];
  pos[2] = MONTH_NAMES[tmbuf->tm_mon][2];
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
  pos[1] = GMT_STR[0];
  pos[2] = GMT_STR[1];
  pos[3] = GMT_STR[2];
  pos[4] = 0;
  pos += 4;
  return pos - target;
}

size_t http_date2rfc2822(char *target, struct tm *tmbuf) {
  char *pos = target;
  uint16_t tmp;
  pos[0] = DAY_NAMES[tmbuf->tm_wday][0];
  pos[1] = DAY_NAMES[tmbuf->tm_wday][1];
  pos[2] = DAY_NAMES[tmbuf->tm_wday][2];
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
  pos[0] = MONTH_NAMES[tmbuf->tm_mon][0];
  pos[1] = MONTH_NAMES[tmbuf->tm_mon][1];
  pos[2] = MONTH_NAMES[tmbuf->tm_mon][2];
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
  pos[1] = GMT_STR[0];
  pos[2] = GMT_STR[1];
  pos[3] = GMT_STR[2];
  pos[4] = 0;
  pos += 4;
  return pos - target;
}

size_t http_date2rfc2109(char *target, struct tm *tmbuf) {
  char *pos = target;
  uint16_t tmp;
  pos[0] = DAY_NAMES[tmbuf->tm_wday][0];
  pos[1] = DAY_NAMES[tmbuf->tm_wday][1];
  pos[2] = DAY_NAMES[tmbuf->tm_wday][2];
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
  *(pos++) = ' ';
  pos[0] = MONTH_NAMES[tmbuf->tm_mon][0];
  pos[1] = MONTH_NAMES[tmbuf->tm_mon][1];
  pos[2] = MONTH_NAMES[tmbuf->tm_mon][2];
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

/**
 * Prints Unix time to a HTTP time formatted string.
 *
 * This variation implements chached results for faster processeing, at the
 * price of a less accurate string.
 */
size_t http_time2str(char *target, const time_t t) {
  /* pre-print time every 1 or 2 seconds or so. */
  static __thread time_t cached_tick;
  static __thread char cached_httpdate[48];
  static __thread size_t chached_len;
  time_t last_tick = facil_last_tick().tv_sec;
  if ((t | 7) < last_tick) {
    /* this is a custom time, not "now", pass through */
    struct tm tm;
    http_gmtime(t, &tm);
    return http_date2str(target, &tm);
  }
  if (last_tick > cached_tick) {
    struct tm tm;
    cached_tick = last_tick; /* refresh every second */
    http_gmtime(last_tick, &tm);
    chached_len = http_date2str(cached_httpdate, &tm);
  }
  memcpy(target, cached_httpdate, chached_len);
  return chached_len;
}

/* Credit to Jonathan Leffler for the idea of a unified conditional */
#define hex_val(c)                                                             \
  (((c) >= '0' && (c) <= '9')                                                  \
       ? ((c)-48)                                                              \
       : (((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))            \
             ? (((c) | 32) - 87)                                               \
             : ({                                                              \
                 return -1;                                                    \
                 0;                                                            \
               }))
static inline int hex2byte(uint8_t *dest, const uint8_t *source) {
  if (source[0] >= '0' && source[0] <= '9')
    *dest = (source[0] - '0');
  else if ((source[0] >= 'a' && source[0] <= 'f') ||
           (source[0] >= 'A' && source[0] <= 'F'))
    *dest = (source[0] | 32) - 87;
  else
    return -1;
  *dest <<= 4;
  if (source[1] >= '0' && source[1] <= '9')
    *dest |= (source[1] - '0');
  else if ((source[1] >= 'a' && source[1] <= 'f') ||
           (source[1] >= 'A' && source[1] <= 'F'))
    *dest |= (source[1] | 32) - 87;
  else
    return -1;
  return 0;
}

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

/* *****************************************************************************
Lookup Tables / functions
***************************************************************************** */
#include "fio_hashmap.h"

static fio_hash_s mime_types;

void http_lib_init(void); /* if library not initialized */

/** Registers a Mime-Type to be associated with the file extension. */
void http_mimetype_register(char *file_ext, size_t file_ext_len,
                            FIOBJ mime_type_str) {
  if (!mime_types.map)
    fio_hash_new(&mime_types);
  uintptr_t hash = fio_siphash(file_ext, file_ext_len);
  FIOBJ old = (FIOBJ)fio_hash_insert(&mime_types, hash, (void *)mime_type_str);
  fiobj_free(old);
}

/**
 * Finds the mime-type associated with the file extension.
 *  Remember to call `fiobj_free`.
 */
FIOBJ http_mimetype_find(char *file_ext, size_t file_ext_len) {
  if (!mime_types.map) {
    http_lib_init();
  }
  uintptr_t hash = fio_siphash(file_ext, file_ext_len);
  return fiobj_dup((FIOBJ)fio_hash_find(&mime_types, hash));
}

/** Clears the Mime-Type registry (it will be emoty afterthis call). */
void http_mimetype_clear(void) {
  if (!mime_types.map)
    return;
  FIO_HASH_FOR_LOOP(&mime_types, obj) { fiobj_free((FIOBJ)obj->obj); }
  fio_hash_free(&mime_types);
  mime_types = (fio_hash_s)FIO_HASH_INIT;
  last_date_added = 0;
  fiobj_free(current_date);
  current_date = FIOBJ_INVALID;
}

/**
* Create with Ruby using:

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
static char invalid_cookie_name_char[256] = {
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

static char invalid_cookie_value_char[256] = {
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

// clang-format off
#define HTTP_SET_STATUS_STR(status, str) [status-100] = { .buffer = (str), .length = (sizeof(str) - 1) }
// clang-format on

/** Returns the status as a C string struct */
fio_cstr_s http_status2str(uintptr_t status) {
  static const fio_cstr_s status2str[] = {
      HTTP_SET_STATUS_STR(100, "Continue"),
      HTTP_SET_STATUS_STR(101, "Switching Protocols"),
      HTTP_SET_STATUS_STR(102, "Processing"),
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
  fio_cstr_s ret = (fio_cstr_s){.length = 0, .buffer = NULL};
  if (status >= 100 && status < sizeof(status2str) / sizeof(status2str[0]))
    ret = status2str[status - 100];
  if (!ret.buffer)
    ret = status2str[400];
  return ret;
}
#undef HTTP_SET_STATUS_STR
