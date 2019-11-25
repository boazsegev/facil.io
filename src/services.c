#include <fio_cli.h>
#include "db/db.h"
#include "rmfio.h"
#include "main.h"


static void example_end_point_1(http_s* h);
static void example_end_point_2(http_s* h);
static void example_ws_1_onopen(ws_s* ws);
static void example_ws_1_onmsg(ws_s *ws, fio_str_info_s msg, u8 is_text);
static void example_ws_1_onshutdown(ws_s *ws);
static void example_ws_1_onclose(sptr uuid, void *udata);



static const char* const http_paths[] = {
	"/api/v1/example-end-point-1",
	"/api/v1/example-end-point-2"
};

static const http_request_fn_t http_callbacks[] = {
	example_end_point_1,
	example_end_point_2
};


static const char* const ws_paths[] = {
	"/api/v1/example-ws-1"
};

static const struct ws_callback_pack ws_callbacks[] = {
	{ 
		.onopen = example_ws_1_onopen,
		.onmsg  = example_ws_1_onmsg,
		.onshutdown = example_ws_1_onshutdown,
		.onclose = example_ws_1_onclose
	}
};


STATIC_ASSERT(
		route_arrays_size,
		STATIC_ARRAY_SIZE(http_paths) == STATIC_ARRAY_SIZE(http_callbacks) &&
		STATIC_ARRAY_SIZE(ws_paths) == STATIC_ARRAY_SIZE(ws_callbacks)
);



/* HTTP EXAMPLE */
static void example_end_point_1(http_s* h)
{
	const char* str = "Hello Example 1";
	http_send_body(h, (void*) str, strlen(str));
}

static void example_end_point_2(http_s* h)
{
	const char* str = "Hello Example 2";
	http_send_body(h, (void*) str, strlen(str));
}



/* WS EXAMPLE */
static void example_ws_1_onopen(ws_s *ws)
{
	const char* channelstr = websocket_udata_get(ws);
	websocket_subscribe(
		ws,
		.channel = (fio_str_info_s) {
			.data = (char*) channelstr,
			.len = strlen(channelstr)
		},
	);

	const char* msg = "WS Hello World";
	websocket_write(
			ws,
			(fio_str_info_s){
				.data = (char*) msg, 
				.len = strlen(msg) 
			},
			1
	);

	FIO_LOG_INFO("EXAMPLE WS 1 ON OPEN");
}

static void example_ws_1_onmsg(ws_s *ws, fio_str_info_s msg, u8 is_text)
{
	FIO_LOG_INFO("EXAMPLE WS 1 ON MSG: %s", msg.data);
}

static void example_ws_1_onshutdown(ws_s *ws)
{
	FIO_LOG_INFO("EXAMPLE WS 1 SHUTTING DOWN");
}

static void example_ws_1_onclose(sptr uuid, void *udata)
{
	const char* channelstr = udata;
	FIO_LOG_INFO("CLOSING CONNECTION %s", channelstr);
}



/* REQUEST HANDLERS */

static void on_http_request(http_s *h) 
{
	/* set a response and send it (finnish vs. destroy). */
	const size_t size = STATIC_ARRAY_SIZE(http_paths);
	size_t i;


	const char* path = fiobj_obj2cstr(h->path).data;
	FIO_LOG_INFO("HTTP REQUEST %s", path);
	for (i = 0; i < size; ++i) {
		if (strcmp(http_paths[i], path) == 0) {
			http_callbacks[i](h);
		}
	}

	/* route not found */
	if (i == size) 
		http_send_error(h, 404);
}

/* HTTP upgrade callback */
static void on_http_upgrade(http_s *h, char *requested_protocol, size_t len) 
{
	const size_t size = STATIC_ARRAY_SIZE(ws_paths);
	size_t i;
	const struct ws_callback_pack* clbks = NULL;

	const char* const path = fiobj_obj2cstr(h->path).data;
	FIO_LOG_INFO("WS REQUEST: %s", path);

	for (i = 0; i < size; ++i) {
		if (strcmp(ws_paths[i], path) == 0) {
			clbks = &ws_callbacks[i];
			break;
		}
	}

	if (clbks == NULL) {
		http_send_error(h, 404);
		return;
	}

	FIO_LOG_INFO("UPGRADING: %s to %s", ws_paths[i], requested_protocol);

	/* Test for upgrade protocol (websocket vs. sse) */
	if (len == 9 && requested_protocol[1] == 'e') {
		if (fio_cli_get_bool("-v")) {
			FIO_LOG_ERROR(
				"* (%d) new WebSocket connection: %s.\n",
				getpid(),
				ws_paths[i]
			);
		}
		http_upgrade2ws(
			h,
			.on_open = clbks->onopen,
			.on_message = clbks->onmsg,
			.on_shutdown = clbks->onshutdown,
			.on_close = clbks->onclose,
			.udata = (void*) ws_paths[i],
		);
	} else {
		FIO_LOG_ERROR(
				"WARNING: unrecognized HTTP upgrade request: %s to %s\n",
				ws_paths[i],
				requested_protocol
		);
		http_send_error(h, 400);
	}
}

/* starts a listeninng socket for HTTP connections. */
void initialize_services(void)
{
	/* optimize WebSocket pub/sub for multi-connection broadcasting */
	websocket_optimize4broadcasts(WEBSOCKET_OPTIMIZE_PUBSUB, 1);
	
	/* listen for inncoming connections */
	if (http_listen(fio_cli_get("-p"), fio_cli_get("-b"),
				.on_request = on_http_request,
				.on_upgrade = on_http_upgrade,
				.max_body_size = fio_cli_get_i("-maxbd") * 1024 * 1024,
				.ws_max_msg_size = fio_cli_get_i("-max-msg") * 1024,
				.public_folder = fio_cli_get("-public"),
				.log = fio_cli_get_bool("-log"),
				.timeout = fio_cli_get_i("-keep-alive"),
				.ws_timeout = fio_cli_get_i("-ping")) == -1) {
		/* listen failed ?*/
		perror("ERROR: facil couldn't initialize HTTP service (already running?)");
		exit(1);
	}
}

