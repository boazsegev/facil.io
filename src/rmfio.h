#ifndef RMFIO_H_
#define RMFIO_H_
#include <stdint.h>
#include <limits.h>
#include <inttypes.h>
#include "fio.h"
#include "fiobj.h"
#include "http.h"


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;


typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;


typedef void (*http_callback_fn_t)(http_s* h);

#define STATIC_ARRAY_SIZE(array) (sizeof((array))/sizeof((array[0])))
#define STATIC_ASSERT(ident, cond) struct static_assert_##ident { \
	u8 check[(cond) ? 1 : -1]; \
}

#endif
