/*
copyright: Boaz segev, 2016-2017
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#define _GNU_SOURCE

#include "http.h"
#include "http1.h"

#include <signal.h>
#include <string.h>
#include <strings.h>
#include <time.h>
/* *****************************************************************************
The global HTTP protocol
***************************************************************************** */

/**
Return the callback used for creating the correct `settings` HTTP protocol.
*/
http_on_open_func http_get_on_open_func(http_settings_s *settings) {
  static void *route[2] = {(void *)http1_on_open};
  return (http_on_open_func)route[settings->version];
}
/**
Return the callback used for freeing the HTTP protocol in the `settings`.
*/
http_on_finish_func http_get_on_finish_func(http_settings_s *settings) {
  static void *route[2] = {NULL};
  return (http_on_finish_func)route[settings->version];
}

void http_on_finish(void *set) {
  http_settings_s *settings = set;
  if (http_get_on_finish_func(set))
    http_get_on_finish_func(set)(set);

  if (settings->on_finish)
    settings->on_finish(settings->udata);

  if (settings->private_metaflags & 2) {
    free((void *)settings->public_folder);
    free(settings);
  }
}

/**
Listens for incoming HTTP connections on the specified posrt and address,
implementing the requested settings.

Since facil.io doesn't support native TLS/SLL
*/
#undef http_listen
int http_listen(const char *port, const char *address,
                http_settings_s arg_settings) {
  if (arg_settings.on_request == NULL) {
    fprintf(
        stderr,
        "ERROR: http_listen requires the .on_request parameter to be set\n");
    kill(0, SIGINT), exit(11);
  }
  http_on_open_func on_open_callback = http_get_on_open_func(&arg_settings);
  if (!on_open_callback) {
    fprintf(stderr, "ERROR: The requested HTTP protocol version isn't "
                    "supported at the moment.\n");
    kill(0, SIGINT), exit(11);
  }
  http_settings_s *settings = malloc(sizeof(*settings));
  *settings = arg_settings;
  settings->private_metaflags = 2;
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

  return facil_listen(.port = port, .address = address,
                      .set_rw_hooks = arg_settings.set_rw_hooks,
                      .rw_udata = arg_settings.rw_udata,
                      .on_finish_rw = arg_settings.on_finish_rw,
                      .on_finish = http_on_finish, .on_open = on_open_callback,
                      .udata = settings);
}

/* *****************************************************************************
HTTP helpers.
***************************************************************************** */

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
  static uint8_t month_len[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, // nonleap year
      31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31  // leap year
  };
  if (*timer < 0)
    return gmtime_r(timer, tmbuf);
  ssize_t tmp;
  tmbuf->tm_gmtoff = 0;
  tmbuf->tm_zone = "UTC";
  tmbuf->tm_isdst = 0;
  tmbuf->tm_year = 70; // tm_year == The number of years since 1900
  tmbuf->tm_mon = 0;
  // for seconds up to weekdays, we build up, as small values clean up larger
  // values.
  tmp = ((ssize_t)*timer);
  tmbuf->tm_sec = tmp % 60;
  tmp = tmp / 60;
  tmbuf->tm_min = tmp % 60;
  tmp = tmp / 60;
  tmbuf->tm_hour = tmp % 24;
  tmp = tmp / 24;
  // day of epoch was a thursday. Add + 3 so sunday == 0...
  tmbuf->tm_wday = (tmp + 3) % 7;
// tmp == number of days since epoch
#define DAYS_PER_400_YEARS ((400 * 365) + 97)
  while (tmp >= DAYS_PER_400_YEARS) {
    tmbuf->tm_year += 400;
    tmp -= DAYS_PER_400_YEARS;
  }
#undef DAYS_PER_400_YEARS
#define DAYS_PER_100_YEARS ((100 * 365) + 24)
  while (tmp >= DAYS_PER_100_YEARS) {
    tmbuf->tm_year += 100;
    tmp -= DAYS_PER_100_YEARS;
    if (((tmbuf->tm_year / 100) & 3) ==
        0) // leap century divisable by 400 => add leap
      --tmp;
  }
#undef DAYS_PER_100_YEARS
#define DAYS_PER_32_YEARS ((32 * 365) + 8)
  while (tmp >= DAYS_PER_32_YEARS) {
    tmbuf->tm_year += 32;
    tmp -= DAYS_PER_32_YEARS;
  }
#undef DAYS_PER_32_YEARS
#define DAYS_PER_8_YEARS ((8 * 365) + 2)
  while (tmp >= DAYS_PER_8_YEARS) {
    tmbuf->tm_year += 8;
    tmp -= DAYS_PER_8_YEARS;
  }
#undef DAYS_PER_8_YEARS
#define DAYS_PER_4_YEARS ((4 * 365) + 1)
  while (tmp >= DAYS_PER_4_YEARS) {
    tmbuf->tm_year += 4;
    tmp -= DAYS_PER_4_YEARS;
  }
#undef DAYS_PER_4_YEARS
  while (tmp >= 365) {
    tmbuf->tm_year += 1;
    tmp -= 365;
    if ((tmbuf->tm_year & 3) == 0) { // leap year
      if (tmp > 0) {
        --tmp;
        continue;
      } else {
        tmp += 365;
        --tmbuf->tm_year;
        break;
      }
    }
  }
  tmbuf->tm_yday = tmp;
  if ((tmbuf->tm_year & 3) == 1) {
    // regular year
    for (size_t i = 0; i < 12; i++) {
      if (tmp < month_len[i])
        break;
      tmp -= month_len[i];
      ++tmbuf->tm_mon;
    }
  } else {
    // leap year
    for (size_t i = 12; i < 24; i++) {
      if (tmp < month_len[i])
        break;
      tmp -= month_len[i];
      ++tmbuf->tm_mon;
    }
  }
  tmbuf->tm_mday = tmp;
  return tmbuf;
}

static char *DAY_NAMES[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static char *MONTH_NAMES[] = {"Jan ", "Feb ", "Mar ", "Apr ", "May ", "Jun ",
                              "Jul ", "Aug ", "Sep ", "Oct ", "Nov ", "Dec "};
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
  pos += http_ul2a(pos, tmbuf->tm_year + 1900);
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
#undef hex_val_tmp
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
#undef hex_val
