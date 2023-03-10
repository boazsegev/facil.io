#include <fio.h>
#include <fiobject.h>

int fiobj_invalid = FIOBJ_INVALID;

int is_invalid(FIOBJ o) {
    if(o == FIOBJ_INVALID) return 1;
    return 0;
}

void fiobj_free_wrapped(FIOBJ o) {
  if (!FIOBJ_IS_ALLOCATED(o))
    return;
  if (fiobj_ref_dec(o))
    return;
  if (FIOBJECT2VTBL(o)->each && FIOBJECT2VTBL(o)->count(o))
    fiobj_free_complex_object(o);
  else
    FIOBJECT2VTBL(o)->dealloc(o, NULL, NULL);
}

void fio_log_debug(const char* msg) {
    FIO_LOG_DEBUG("%s", msg);
}
void fio_log_info(const char* msg) {
    FIO_LOG_INFO("%s", msg);
}
void fio_log_warning(const char* msg) {
    FIO_LOG_WARNING("%s", msg);
}
void fio_log_error(const char* msg) {
    FIO_LOG_ERROR("%s", msg);
}
void fio_log_fatal(const char* msg) {
    FIO_LOG_FATAL("%s", msg);
}

// Log Levels as ints 

/** Logging level of zero (no logging). */
int fio_log_level_none = FIO_LOG_LEVEL_NONE;
/** Log fatal errors. */
int fio_log_level_fatal = FIO_LOG_LEVEL_FATAL;
/** Log errors and fatal errors. */
int fio_log_level_error = FIO_LOG_LEVEL_ERROR;
/** Log warnings, errors and fatal errors. */
int fio_log_level_warning = FIO_LOG_LEVEL_WARNING;
/** Log every message (info, warnings, errors and fatal errors). */
int fio_log_level_info = FIO_LOG_LEVEL_INFO;
/** Log everything, including debug messages. */
int fio_log_level_debug = FIO_LOG_LEVEL_DEBUG;

void fio_set_log_level(int level) {
    FIO_LOG_LEVEL = level;
}

int fio_get_log_level() {
    return FIO_LOG_LEVEL;
}

void fio_log_print(int level, const char* msg) {
    FIO_LOG_PRINT(level, "%s", msg);
}
