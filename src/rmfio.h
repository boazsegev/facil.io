#ifndef RMFIO_H_
#define RMFIO_H_
#include <stdint.h>
#include <inttypes.h>
#include "fio.h"
#include "fiobj.h"


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;


typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;


#define STATIC_ARRAY_SIZE(array) (sizeof((array))/sizeof((array[0])))


#endif
