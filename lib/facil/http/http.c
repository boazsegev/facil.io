/*
Copyright: Boaz segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

#include "spnlock.inc"

#include "http1.h"
#include "http_internal.h"

#include <signal.h>

/* *****************************************************************************
Small Helpers
***************************************************************************** */
static inline void add_content_length(http_s *r, uintptr_t length) {
  static uint64_t cl_hash = 0;
  if (!cl_hash)
    cl_hash = fiobj_sym_hash("content-length", 14);
  if (!fiobj_hash_get3(r->private_data.out_headers, cl_hash)) {
    static fiobj_s *cl;
    if (!cl)
      cl = HTTP_HEADER_CONTENT_LENGTH;
    set_header_if_missing(r, cl, fiobj_num_new(length));
  }
}

static inline void add_date(http_s *r, time_t t) {
  static uint64_t date_hash = 0;
  if (!date_hash)
    date_hash = fiobj_sym_hash("date", 4);
  if (!fiobj_hash_get3(r->private_data.out_headers, date_hash)) {
    static fiobj_s *d;
    if (!d)
      d = HTTP_HEADER_DATE;
    fiobj_s *s = fiobj_str_buf(32);
    fiobj_str_resize(s, http_time2str(fiobj_obj2cstr(s).data, t));
    set_header_if_missing(r, d, s);
  }
}

struct header_writer_s {
  fiobj_s *dest;
  fiobj_s *name;
  fiobj_s *value;
};

static int write_header(fiobj_s *o, void *w_) {
  struct header_writer_s *w = w_;
  if (!o)
    return 0;
  if (o->type == FIOBJ_T_COUPLET) {
    w->name = fiobj_couplet2key(o);
    o = fiobj_couplet2obj(o);
  } else if (o->type == FIOBJ_T_ARRAY) {
    fiobj_each1(o, 0, write_header, w);
    return 0;
  }
  if (!o)
    return 0;
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

/* *****************************************************************************
The Request / Response type and functions
***************************************************************************** */

/**
 * Sets a response header, taking ownership of the value object, but NOT the
 * name object (so name objects could be reused in future responses).
 *
 * Returns -1 on error and 0 on success.
 */
int http_set_header(http_s *r, fiobj_s *name, fiobj_s *value) {
  if (!r || !name || !r->private_data.out_headers)
    return -1;
  set_header_add(r, name, value);
  return 0;
}
/**
 * Sets a response header, taking ownership of the value object, but NOT the
 * name object (so name objects could be reused in future responses).
 *
 * Returns -1 on error and 0 on success.
 */
int http_set_header2(http_s *r, fio_cstr_s n, fio_cstr_s v) {
  if (!r || !n.data || !n.length || (v.data && !v.length) ||
      !r->private_data.out_headers)
    return -1;
  fiobj_s *tmp = fiobj_sym_new(n.data, n.length);
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

  if (!h || !cookie.name || !h->private_data.out_headers)
    return -1;

#define invalid_cookie_char(c)                                                 \
  ((c) < '!' || (c) > '~' || (c) == '=' || (c) == ' ' || (c) == ',' ||         \
   (c) == ';')

  if (cookie.name_len) {
    ssize_t tmp = cookie.name_len;
    do {
      tmp--;
      if (invalid_cookie_char(cookie.name[tmp])) {
        fprintf(stderr, "ERROR: illigal char 0x%.2x in cookie name (in %s)\n",
                cookie.name[tmp], cookie.name);
        return -1;
      }
    } while (tmp);
  } else {
    while (cookie.name[cookie.name_len]) {
      if (invalid_cookie_char(cookie.name[cookie.name_len])) {
        fprintf(stderr, "ERROR: illigal char 0x%.2x in cookie name (in %s)\n",
                cookie.name[cookie.name_len], cookie.name);
        return -1;
      }
      cookie.name_len++;
    }
  }

  if (cookie.value_len) {
    ssize_t tmp = cookie.value_len;
    do {
      tmp--;
      if (invalid_cookie_char(cookie.value[tmp])) {
        fprintf(stderr,
                "ERROR: illigal char 0x%.2x in cookie value (for cookie %s)\n",
                cookie.value[tmp], cookie.name);
        return -1;
      }
    } while (tmp);
  } else {
    while (cookie.value[cookie.value_len]) {
      if (invalid_cookie_char(cookie.value[cookie.value_len])) {
        fprintf(stderr,
                "ERROR: illigal char 0x%.2x in cookie value (for cookie %s)\n",
                cookie.value[cookie.value_len], cookie.name);
        return -1;
      }
      cookie.value_len++;
    }
  }

#undef invalid_cookie_char
  fiobj_s *c = fiobj_str_new(cookie.name, cookie.name_len);
  /* write name and value */
  fiobj_str_write(c, "=", 1);
  if (cookie.value)
    fiobj_str_write(c, cookie.value, cookie.value_len);
  else
    cookie.max_age = -1;
  fiobj_str_write(c, ";", 1);

  if (cookie.max_age) {
    fiobj_str_write2(c, "Max-Age=%d;", cookie.max_age);
  }
  if (cookie.domain && cookie.domain_len) {
    fiobj_str_write(c, "domain=", 7);
    fiobj_str_write(c, cookie.domain, cookie.domain_len);
    fiobj_str_write(c, ";", 1);
  }
  if (cookie.path && cookie.path_len) {
    fiobj_str_write(c, "path=", 5);
    fiobj_str_write(c, cookie.path, cookie.path_len);
    fiobj_str_write(c, ";", 1);
  }
  if (cookie.http_only) {
    fiobj_str_write(c, "HttpOnly;", 9);
  }
  if (cookie.secure) {
    fiobj_str_write(c, "secure;", 7);
  }
  fiobj_s *sym = fiobj_sym_new("set-cookie", 10);
  set_header_add(h, sym, c);
  fiobj_free(sym);
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
  add_content_length(r, length);
  add_date(r, facil_last_tick().tv_sec);
  return ((http_protocol_s *)r->private_data.owner)
      ->vtable->http_send_body(r, data, length);
}
/**
 * Sends the response headers and the specified file (the response's body).
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
int http_sendfile(http_s *r, int fd, uintptr_t length, uintptr_t offset) {
  add_content_length(r, length);
  add_date(r, facil_last_tick().tv_sec);
  return ((http_protocol_s *)r->private_data.owner)
      ->vtable->http_sendfile(r, fd, length, offset);
}
/**
 * Sends the response headers and the specified file (the response's body).
 *
 * Returns -1 on error and 0 on success.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
int http_sendfile2(http_s *r, char *filename, size_t name_length);

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
int http_send_error(http_s *r, intptr_t uuid, size_t error);

/**
 * Sends the response headers and starts streaming. Use `http_defer` to continue
 * straming.
 *
 * Returns -1 on error and 0 on success.
 */
int http_stream(http_s *r, void *data, uintptr_t length) {
  return ((http_protocol_s *)r->private_data.owner)
      ->vtable->http_stream(r, data, length);
}
/**
 * Sends the response headers for a header only response.
 *
 * AFTER THIS FUNCTION IS CALLED, THE `http_s` OBJECT IS NO LONGER VALID.
 */
void http_finish(http_s *r) {
  return ((http_protocol_s *)r->private_data.owner)->vtable->http_finish(r);
}
/**
 * Pushes a data response when supported (HTTP/2 only).
 *
 * Returns -1 on error and 0 on success.
 */
int http_push_data(http_s *r, void *data, uintptr_t length, char *mime_type,
                   uintptr_t type_length) {
  return ((http_protocol_s *)r->private_data.owner)
      ->vtable->http_push_data(r, data, length, mime_type, type_length);
}
/**
 * Pushes a file response when supported (HTTP/2 only).
 *
 * If `mime_type` is NULL, an attempt at automatic detection using `filename`
 * will be made.
 *
 * Returns -1 on error and 0 on success.
 */
int http_push_file(http_s *r, char *filename, size_t name_length,
                   char *mime_type, uintptr_t type_length) {
  return ((http_protocol_s *)r->private_data.owner)
      ->vtable->http_push_file(r, filename, name_length, mime_type,
                               type_length);
}

/**
 * Defers the request / response handling for later.
 *
 * Returns -1 on error and 0 on success.
 */
int http_defer(http_s *h, void (*task)(http_s *h),
               void (*fallback)(http_s *h)) {
  return ((http_protocol_s *)h->private_data.owner)
      ->vtable->http_defer(h, task, fallback);
}

/* *****************************************************************************
Listening to HTTP connections
***************************************************************************** */

static protocol_s *http_on_open(intptr_t uuid, void *set) {
  static __thread ssize_t capa = 0;
  if (!capa)
    capa = sock_max_capacity();
  facil_set_timeout(uuid, ((http_settings_s *)set)->timeout);
  if (sock_uuid2fd(uuid) + HTTP_BUSY_UNLESS_HAS_FDS >= capa) {
    /* TODO: use http_send_error2(uuid) */
    fprintf(stderr, "WARNING: HTTP server at capacity\n");
    sock_close(uuid);
    return NULL;
  }
  return http1_new(uuid, set, NULL, 0);
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
    fprintf(
        stderr,
        "ERROR: http_listen requires the .on_request parameter to be set\n");
    kill(0, SIGINT), exit(11);
  }

  http_settings_s *settings = malloc(sizeof(*settings));
  *settings = arg_settings;

  if (!settings->max_body_size)
    settings->max_body_size = HTTP_DEFAULT_BODY_LIMIT;
  if (!settings->timeout)
    settings->timeout = 5;

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

  return facil_listen(.port = port, .address = binding,
                      .set_rw_hooks = arg_settings.set_rw_hooks,
                      .rw_udata = arg_settings.rw_udata,
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
  return ((http_protocol_s *)r->private_data.owner)->settings;
}

/* *****************************************************************************
TODO: HTTP client mode
***************************************************************************** */

/* *****************************************************************************
HTTP Helper functions that could be used globally
***************************************************************************** */

/**
 * Returns a String object representing the unparsed HTTP request (HTTP version
 * is capped at HTTP/1.1). Mostly usable for proxy usage and debugging.
 */
fiobj_s *http_req2str(http_s *h) {
  if (!h->headers)
    return NULL;

  struct header_writer_s w;
  w.dest = fiobj_str_buf(4096);

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
      fiobj_str_write(w.dest, "HTTP/1.1\r\n", 10);
    else {
      fiobj_str_join(w.dest, h->version);
      fiobj_str_write(w.dest, "\r\n", 2);
    }
  }

  fiobj_each1(h->headers, 0, write_header, &w);
  fiobj_str_write(w.dest, "\r\n", 2);
  if (h->body) {
    fiobj_io_seek(h->body, 0);
    fio_cstr_s t = fiobj_io_read(h->body, 0);
    fiobj_str_write(w.dest, t.data, t.len);
  }
  return w.dest;
}

/**
A faster (yet less localized) alternative to `gmtime_r`.

See the libc `gmtime_r` documentation for details.

Falls back to `gmtime_r` for dates before epoch.
*/
struct tm *http_gmtime(const time_t *timer, struct tm *tmbuf) {
  // static char* DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  // static char * Months = {  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  // "Jul",
  // "Aug", "Sep", "Oct", "Nov", "Dec"};
  static const uint8_t month_len[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, // nonleap year
      31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31  // leap year
  };
  if (*timer < 0)
    return gmtime_r(timer, tmbuf);
  ssize_t a, b;
  tmbuf->tm_gmtoff = 0;
  tmbuf->tm_zone = "UTC";
  tmbuf->tm_isdst = 0;
  tmbuf->tm_year = 70; // tm_year == The number of years since 1900
  tmbuf->tm_mon = 0;
  // for seconds up to weekdays, we build up, as small values clean up larger
  // values.
  a = ((ssize_t)*timer);
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
  *(uint32_t *)pos = *((uint32_t *)DAY_NAMES[tmbuf->tm_wday]);
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
  *(uint32_t *)pos = *((uint32_t *)MONTH_NAMES[tmbuf->tm_mon]);
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
  *((uint32_t *)(pos + 1)) = *((uint32_t *)GMT_STR);
  pos += 4;
  return pos - target;
}

size_t http_date2rfc2822(char *target, struct tm *tmbuf) {
  char *pos = target;
  uint16_t tmp;
  *(uint32_t *)pos = *((uint32_t *)DAY_NAMES[tmbuf->tm_wday]);
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
  *(uint32_t *)pos = *((uint32_t *)MONTH_NAMES[tmbuf->tm_mon]);
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
  *((uint32_t *)(pos + 1)) = *((uint32_t *)GMT_STR);
  pos += 4;
  return pos - target;
}

size_t http_date2rfc2109(char *target, struct tm *tmbuf) {
  char *pos = target;
  uint16_t tmp;
  *(uint32_t *)pos = *((uint32_t *)DAY_NAMES[tmbuf->tm_wday]);
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
  *(uint32_t *)pos = *((uint32_t *)MONTH_NAMES[tmbuf->tm_mon]);
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
    http_gmtime(&t, &tm);
    return http_date2str(target, &tm);
  }
  if (last_tick > cached_tick) {
    struct tm tm;
    cached_tick = last_tick | 1;
    http_gmtime(&last_tick, &tm);
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
