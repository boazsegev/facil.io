/**
In this a Hello World example using the bundled HTTP / WebSockets extension.

Compile using:

    NAME=http make

Or test the `poll` engine's performance by compiling with `poll`:

    FIO_POLL=1 NAME=http make

Run with:

    ./tmp/http -t 1

Benchmark with keep-alive:

    ab -c 200 -t 4 -n 1000000 -k http://127.0.0.1:3000/
    wrk -c200 -d4 -t1 http://localhost:3000/

Benchmark with higher load:

    ab -c 4400 -t 4 -n 1000000 -k http://127.0.0.1:3000/
    wrk -c4400 -d4 -t1 http://localhost:3000/

Use a javascript console to connect to the WebSocket chat service... maybe using
the following javascript code:

    // run 1st client app on port 3000.
    ws = new WebSocket("ws://localhost:3000/Mitchel");
    ws.onmessage = function(e) { console.log(e.data); };
    ws.onclose = function(e) { console.log("closed"); };
    ws.onopen = function(e) { e.target.send("Yo!"); };

    // run 2nd client app on port 3030, to test Redis
    ws = new WebSocket("ws://localhost:3030/Johana");
    ws.onmessage = function(e) { console.log(e.data); };
    ws.onclose = function(e) { console.log("closed"); };
    ws.onopen = function(e) { e.target.send("Brut."); };

It's possible to use SSE (Server-Sent-Events / EventSource) for listening in on
the chat:

    var source = new EventSource("/Watcher");
    source.addEventListener('message', (e) => { console.log(e.data); });
    source.addEventListener('open', (e) => {
      console.log("SSE Connection open.");
    }); source.addEventListener('close', (e) => {
      console.log("SSE Connection lost."); });

Remember that published messages will now be printed to the console both by
Mitchel and Johana, which means messages will be delivered twice unless using
two different browser windows.
*/

/* Include the core library */
#include <fio.h>

/* Include the CLI and HTTP / WebSockets extensions (and the FIOBJ library) */
#include <fio_cli.h>
#include <http.h>

/* *****************************************************************************
The main function
***************************************************************************** */

/* HTTP request handler */
static void on_http_request(http_s *h);
/* HTTP upgrade request handler */
static void on_http_upgrade(http_s *h, char *requested_protocol, size_t len);
/* Command Line Arguments Management */
static void initialize_cli(int argc, char const *argv[]);

int main(int argc, char const *argv[]) {
  initialize_cli(argc, argv);
  /* optimize WebSocket pub/sub for multi-connection broadcasting */
  websocket_optimize4broadcasts(WEBSOCKET_OPTIMIZE_PUBSUB, 1);
  /* listen for inncoming connections */
  if (http_listen(fio_cli_get("-p"), fio_cli_get("-b"),
                  .on_request = on_http_request, .on_upgrade = on_http_upgrade,
                  .max_body_size = (fio_cli_get_i("-maxbd") * 1024 * 1024),
                  .ws_max_msg_size = (fio_cli_get_i("-maxms") * 1024),
                  .public_folder = fio_cli_get("-public"),
                  .log = fio_cli_get_bool("-log"),
                  .timeout = fio_cli_get_i("-keep-alive"),
                  .ws_timeout = fio_cli_get_i("-ping")) == -1) {
    /* listen failed ?*/
    perror(
        "ERROR: facil.io couldn't initialize HTTP service (already running?)");
    exit(1);
  }
  fio_start(.threads = fio_cli_get_i("-t"), .workers = fio_cli_get_i("-w"));
  fio_cli_end();
  return 0;
}

/* *****************************************************************************
HTTP Request / Response Handling
***************************************************************************** */

static void on_http_request(http_s *h) {
  /* set a response and send it (finnish vs. destroy). */
  http_send_body(h, "Hello World!", 12);
}

/* *****************************************************************************
HTTP Upgrade Handling
***************************************************************************** */

/* Server Sent Event Handlers */
static void sse_on_open(http_sse_s *sse);
static void sse_on_close(http_sse_s *sse);

/* WebSocket Handlers */
static void ws_on_open(ws_s *ws);
static void ws_on_message(ws_s *ws, fio_str_info_s msg, uint8_t is_text);
static void ws_on_shutdown(ws_s *ws);
static void ws_on_close(intptr_t uuid, void *udata);

/* HTTP upgrade callback */
static void on_http_upgrade(http_s *h, char *requested_protocol, size_t len) {
  /* Upgrade to SSE or WebSockets and set the request path as a nickname. */
  FIOBJ nickname;
  if (fiobj_obj2cstr(h->path).len > 1) {
    nickname = fiobj_str_new(fiobj_obj2cstr(h->path).data + 1,
                             fiobj_obj2cstr(h->path).len - 1);
  } else {
    nickname = fiobj_str_new("Guest", 5);
  }
  /* Test for upgrade protocol (websocket vs. sse) */
  if (len == 3 && requested_protocol[1] == 's') {
    http_upgrade2sse(h, .on_open = sse_on_open, .on_close = sse_on_close,
                     .udata = (void *)nickname);
  } else if (len == 9 && requested_protocol[1] == 'e') {
    http_upgrade2ws(h, .on_message = ws_on_message, .on_open = ws_on_open,
                    .on_shutdown = ws_on_shutdown, .on_close = ws_on_close,
                    .udata = (void *)nickname);
  } else {
    fprintf(stderr, "WARNING: unrecognized HTTP upgrade request: %s\n",
            requested_protocol);
    http_send_error(h, 400);
    fiobj_free(nickname); // we didn't use this
  }
}

/* *****************************************************************************
Globals
***************************************************************************** */

static fio_str_info_s CHAT_CANNEL = {.data = "chat", .len = 4};

/* *****************************************************************************
HTTP SSE (Server Sent Events) Callbacks
***************************************************************************** */

/**
 * The (optional) on_open callback will be called once the EventSource
 * connection is established.
 */
static void sse_on_open(http_sse_s *sse) {
  http_sse_write(sse, .data = {.data = "Welcome to the SSE chat channel.\r\n"
                                       "You can only listen, not write.",
                               .len = 65});
  http_sse_subscribe(sse, .channel = CHAT_CANNEL);
  http_sse_set_timout(sse, fio_cli_get_i("-ping"));
  FIOBJ tmp = fiobj_str_copy((FIOBJ)sse->udata);
  fiobj_str_write(tmp, " joind the chat only to listen.", 31);
  fio_publish(.channel = CHAT_CANNEL, .message = fiobj_obj2cstr(tmp));
  fiobj_free(tmp);
}

static void sse_on_close(http_sse_s *sse) {
  /* Let everyone know we left the chat */
  fiobj_str_write((FIOBJ)sse->udata, " left the chat.", 15);
  fio_publish(.channel = CHAT_CANNEL,
              .message = fiobj_obj2cstr((FIOBJ)sse->udata));
  /* free the nickname */
  fiobj_free((FIOBJ)sse->udata);
}

/* *****************************************************************************
WebSockets Callbacks
***************************************************************************** */

static void ws_on_message(ws_s *ws, fio_str_info_s msg, uint8_t is_text) {
  // Add the Nickname to the message
  FIOBJ str = fiobj_str_copy((FIOBJ)websocket_udata(ws));
  fiobj_str_write(str, ": ", 2);
  fiobj_str_write(str, msg.data, msg.len);
  // publish
  fio_publish(.channel = CHAT_CANNEL, .message = fiobj_obj2cstr(str));
  // free the string
  fiobj_free(str);
  (void)is_text; // we don't care.
  (void)ws;      // this could be used to send an ACK, but we don't.
}
static void ws_on_open(ws_s *ws) {
  websocket_subscribe(ws, .channel = CHAT_CANNEL);
  websocket_write(
      ws, (fio_str_info_s){.data = "Welcome to the chat-room.", .len = 25}, 1);
  FIOBJ tmp = fiobj_str_copy((FIOBJ)websocket_udata(ws));
  fiobj_str_write(tmp, " joind the chat.", 16);
  fio_publish(.channel = CHAT_CANNEL, .message = fiobj_obj2cstr(tmp));
  fiobj_free(tmp);
}
static void ws_on_shutdown(ws_s *ws) {
  websocket_write(
      ws, (fio_str_info_s){.data = "Server shutting down, goodbye.", .len = 30},
      1);
}

static void ws_on_close(intptr_t uuid, void *udata) {
  /* Let everyone know we left the chat */
  fiobj_str_write((FIOBJ)udata, " left the chat.", 15);
  fio_publish(.channel = CHAT_CANNEL, .message = fiobj_obj2cstr((FIOBJ)udata));
  /* free the nickname */
  fiobj_free((FIOBJ)udata);
  (void)uuid; // we don't use the ID
}

/* *****************************************************************************
CLI helpers
***************************************************************************** */
static void initialize_cli(int argc, char const *argv[]) {
  /*     ****  Command line arguments ****     */
  fio_cli_start(
      argc, argv, 0, NULL,
      "-bind -b address to listen to. defaults any available.",
      "-port -p port number to listen to. defaults port 3000", FIO_CLI_TYPE_INT,
      "-workers -w number of processes to use.", FIO_CLI_TYPE_INT,
      "-threads -t number of threads per process.", FIO_CLI_TYPE_INT,
      "-log -v request verbosity (logging).", FIO_CLI_TYPE_BOOL,
      "-public -www public folder, for static file service.",
      "-keep-alive -k HTTP keep-alive timeout (0..255). default: 10s",
      FIO_CLI_TYPE_INT, "-ping websocket ping interval (0..255). default: 40s",
      FIO_CLI_TYPE_INT,
      "-max-body -maxbd HTTP upload limit in Mega Bytes. default: 50Mb",
      FIO_CLI_TYPE_INT,
      "-max-message -maxms incoming websocket message size limit in Kilo "
      "Bytes. default: 250Kb",
      FIO_CLI_TYPE_INT,
      "-redis -r an optional Redis URL server address. i.e.: "
      "redis://user:password@localhost:6379/");

  /* Test and set any default options */
  if (!fio_cli_get("-p")) {
    /* Test environment as well */
    char *tmp = getenv("PORT");
    if (!tmp)
      tmp = "3000";
    /* Set default (unlike cmd line arguments, aliases are manually set) */
    fio_cli_set("-p", tmp);
    fio_cli_set("-port", tmp);
  }
  if (!fio_cli_get("-b")) {
    char *tmp = getenv("ADDRESS");
    if (tmp) {
      fio_cli_set("-b", tmp);
      fio_cli_set("-bind", tmp);
    }
  }
  if (!fio_cli_get("-public")) {
    char *tmp = getenv("HTTP_PUBLIC_FOLDER");
    if (tmp) {
      fio_cli_set("-public", tmp);
      fio_cli_set("-www", tmp);
    }
  }

  if (!fio_cli_get("-redis")) {
    char *tmp = getenv("REDIS_URL");
    if (tmp) {
      fio_cli_set("-redis", tmp);
      fio_cli_set("-r", tmp);
    }
  }

  fio_cli_set_default("-ping", "40");

  fio_cli_set_default("-k", "10");
  fio_cli_set_default("-keep-alive", "10");

  fio_cli_set_default("-max-body", "50");
  fio_cli_set_default("-maxbd", "50");
  fio_cli_set_default("-max-message", "250");
  fio_cli_set_default("-maxms", "250");
}
