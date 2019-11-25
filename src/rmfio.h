#ifndef RMFIO_H_
#define RMFIO_H_
#include <stdint.h>
#include <limits.h>
#include <inttypes.h>
#include <errno.h>
#include <stdlib.h>
#include <fio.h>
#include <fiobj.h>
#include <http.h>
#include <websockets.h>

#define STATIC_ARRAY_SIZE(array) (sizeof((array))/sizeof((array)[0]))
#define STATIC_ASSERT(ident, cond) struct static_assert_##ident { \
	u8 check[(cond) ? 1 : -1]; \
}


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;


typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef intptr_t sptr;
typedef uintptr_t uptr;


typedef void (*http_request_fn_t)(http_s* h);
typedef void (*ws_open_fn_t)(ws_s* ws);
typedef void (*ws_message_fn_t)(ws_s* ws, fio_str_info_s msg, u8 is_text);
typedef void (*ws_shutdown_fn_t)(ws_s* ws);
typedef void (*ws_close_fn_t)(intptr_t uuid, void* udata);


struct ws_callback_pack {
	ws_open_fn_t onopen;
	ws_message_fn_t onmsg;	
	ws_shutdown_fn_t onshutdown;
	ws_close_fn_t onclose;
};


#endif

