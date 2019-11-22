#include "fio_cli.h"
#include "db/db.h"
#include "rmfio.h"
#include "main.h"

static void example_end_point_1(http_s* h);
static void example_end_point_2(http_s* h);


static const char* const route_paths[] = {
	"/api/v1/example-end-point/1",
	"/api/v1/example-end-point/2"
};

static const http_callback_fn_t route_callbacks[] = {
	example_end_point_1,
	example_end_point_2
};

STATIC_ASSERT(
	route_arrays_size,
	STATIC_ARRAY_SIZE(route_paths) == STATIC_ARRAY_SIZE(route_callbacks)
);

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

/* TODO: edit this function to handle HTTP data and answer Websocket requests.*/
static void on_http_request(http_s *h) 
{
	/* set a response and send it (finnish vs. destroy). */
	const size_t size = STATIC_ARRAY_SIZE(route_paths);
	size_t i;


	const char* path = fiobj_obj2cstr(h->path).data;
	for (i = 0; i < size; ++i) {
		if (strcmp(route_paths[i], path) == 0) {
			route_callbacks[i](h);
		}
	}

	/* route not found */
	if (i == size) 
		http_send_error(h, 404);
}

/* starts a listeninng socket for HTTP connections. */
void initialize_http_service(void)
{
	/* listen for inncoming connections */
	if (http_listen(fio_cli_get("-p"), fio_cli_get("-b"),
			.on_request = on_http_request,
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

