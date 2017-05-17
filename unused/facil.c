/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "facil.h"
#include "evio.h"
#include "spnlock.inc"

#include <string.h>
#include <sys/mman.h>

/* *****************************************************************************
Data Structures
***************************************************************************** */
typedef struct ProtocolMetadata {
  spn_lock_i lock;
  spn_lock_i sub_lock;
  unsigned rsv : 16;
} protocol_metadata_s;

#define prt_meta(prt) (*((protocol_metadata_s *)(&(prt)->rsv)))

struct connection_data_s {
  protocol_s *protocol;
  time_t active;
  uint8_t timeout;
  spn_lock_i lock;
};

struct {
  spn_lock_i global_lock;
  uint8_t need_review;
  size_t capacity;
  time_t last_cycle;
  struct connection_data_s conn[];
} * facil_data;

#define fd_data(fd) (facil_data->conn[(fd)])
#define uuid_data(uuid) fd_data(sock_uuid2fd((uuid)))
#define uuid_prt_meta(uuid) prt_meta(uuid_data((uuid)).protocol)

static inline void clear_connection_data_unsafe(intptr_t uuid,
                                                protocol_s *protocol) {
  uuid_data(uuid) = (struct connection_data_s){.active = facil_data->last_cycle,
                                               .protocol = protocol,
                                               .lock = uuid_data(uuid).lock};
}

/* *****************************************************************************
Deferred event handlers
***************************************************************************** */
static void deferred_on_close(void *arg) {
  protocol_s *pr = arg;
  if (pr->rsv)
    goto postpone;
  pr->on_close(pr);
  return;
postpone:
  defer(deferred_on_close, arg);
}

#define defferred_action(action, lock_name)                                    \
  if (spn_trylock(&uuid_data(arg).lock))                                       \
    goto postpone;                                                             \
  if (!uuid_data(arg).protocol) {                                              \
    spn_unlock(&uuid_data(arg).lock);                                          \
    return;                                                                    \
  }                                                                            \
  if (spn_trylock(&uuid_prt_meta(arg).lock_name)) {                            \
    spn_unlock(&uuid_data(arg).lock);                                          \
    goto postpone;                                                             \
  }                                                                            \
  protocol_s *pr = uuid_data(arg).protocol;                                    \
  spn_unlock(&uuid_data(arg).lock);                                            \
  pr->action((intptr_t)arg, pr);                                               \
  spn_unlock(&prt_meta(pr).lock_name);

static void deferred_on_data(void *arg) {
  defferred_action(on_data, lock);
  return;
postpone:
  defer(deferred_on_data, arg);
}
static void deferred_on_ready(void *arg) {
  defferred_action(on_ready, sub_lock);
  return;
postpone:
  defer(deferred_on_ready, arg);
}
static void deferred_on_shutdown(void *arg) {
  defferred_action(on_shutdown, sub_lock);
  sock_close((intptr_t)arg);
  return;
postpone:
  defer(deferred_on_shutdown, arg);
}
static void deferred_ping(void *arg) {
  if (uuid_data(arg).timeout &&
      (uuid_data(arg).timeout >
       (facil_data->last_cycle - uuid_data(arg).active))) {
    return;
  }
  defferred_action(ping, sub_lock);
  return;
postpone:
  defer(deferred_ping, arg);
}

/* *****************************************************************************
Event Handlers (evio)
***************************************************************************** */
void evio_on_ready(void *arg) {
  defer((void (*)(void *))sock_flush, arg);
  defer((void (*)(void *))deferred_on_ready, arg);
}
void evio_on_close(void *arg) { sock_force_close((intptr_t)arg); }
void evio_on_error(void *arg) { sock_force_close((intptr_t)arg); }
void evio_on_data(void *arg) { defer(deferred_on_data, arg); }

/* *****************************************************************************
Socket callbacks
***************************************************************************** */

void sock_on_close(intptr_t uuid) {
  spn_lock(&uuid_data(uuid).lock);
  protocol_s *old_protocol = uuid_data(uuid).protocol;
  clear_connection_data_unsafe(uuid, NULL);
  spn_unlock(&uuid_data(uuid).lock);
  if (old_protocol)
    defer(deferred_on_close, old_protocol);
}

void sock_touch(intptr_t uuid) {
  uuid_data(uuid).active = facil_data->last_cycle;
}

/* *****************************************************************************
Initialization and Cleanup
***************************************************************************** */
static spn_lock_i facil_libinit_lock = SPN_LOCK_INIT;

static void facil_libcleanup(void) {
  /* free memory */
  spn_lock(&facil_libinit_lock);
  if (facil_data) {
    munmap(facil_data,
           sizeof(*facil_data) +
               (facil_data->capacity * sizeof(struct connection_data_s)));
    facil_data = NULL;
  }
  spn_unlock(&facil_libinit_lock);
}

static void facil_lib_init(void) {
  size_t capa = sock_max_capacity();
  size_t mem_size =
      sizeof(*facil_data) + (capa * sizeof(struct connection_data_s));
  spn_lock(&facil_libinit_lock);
  if (facil_data)
    goto finish;
  facil_data = mmap(NULL, mem_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (!facil_data)
    perror("ERROR: Couldn't initialize the facil.io library"), exit(0);
  memset(facil_data, 0, mem_size);
  facil_data->capacity = capa;
  atexit(facil_libcleanup);
#ifdef DEBUG
  if (FACIL_PRINT_STATE)
    fprintf(stderr,
            "Initialized the facil.io library.\n"
            "facil.io's memory footprint per connection == %lu Bytes X %lu\n"
            "=== facil.io's memory footprint: %lu ===\n\n",
            sizeof(struct connection_data_s), facil_data->capacity, mem_size);
#endif
finish:
  spn_unlock(&facil_libinit_lock);
  time(&facil_data->last_cycle);
}

/* *****************************************************************************
Mock Protocol and service Callbacks
***************************************************************************** */
static void mock_on_ev(intptr_t uuid, protocol_s *protocol) {
  (void)uuid;
  (void)protocol;
}

static void mock_on_close(protocol_s *protocol) { (void)(protocol); }

static void mock_ping(intptr_t uuid, protocol_s *protocol) {
  (void)(protocol);
  sock_force_close(uuid);
}
static void mock_idle(void) {}

/* *****************************************************************************
The listenning protocol
***************************************************************************** */
#undef facil_listen

static const char *listener_protocol_name =
    "listening protocol __facil_internal__";

struct ListenerProtocol {
  protocol_s protocol;
  protocol_s *(*on_open)(intptr_t uuid, void *udata);
  void *udata;
  void (*on_start)(void *udata);
  void (*on_finish)(void *udata);
};

static void listener_ping(intptr_t uuid, protocol_s *plistener) {
  (void)plistener;
  uuid_data(uuid).active = facil_data->last_cycle;
}

static void listener_on_data(intptr_t uuid, protocol_s *plistener) {
  intptr_t new_client;
  protocol_s *pr = NULL;
  struct ListenerProtocol *listener = (void *)plistener;
  while ((new_client = sock_accept(uuid)) != -1) {
    pr = listener->on_open(new_client, listener->udata);
    facil_attach(new_client, pr);
    if (!pr)
      sock_close(new_client);
  }
}

static void free_listenner(void *li) { free(li); }

static void listener_on_close(protocol_s *plistener) {
  struct ListenerProtocol *listener = (void *)plistener;
  listener->on_finish(listener->udata);
  free_listenner(listener);
}

static inline struct ListenerProtocol *
listener_alloc(struct facil_listen_args settings) {
  if (!settings.on_start)
    settings.on_start = (void (*)(void *))mock_on_close;
  if (!settings.on_finish)
    settings.on_finish = (void (*)(void *))mock_on_close;
  struct ListenerProtocol *listener = malloc(sizeof(*listener));
  if (listener) {
    *listener = (struct ListenerProtocol){
        .protocol.service = listener_protocol_name,
        .protocol.on_data = listener_on_data,
        .protocol.on_close = listener_on_close,
        .protocol.ping = listener_ping,
        .on_open = settings.on_open,
        .udata = settings.udata,
        .on_start = settings.on_start,
        .on_finish = settings.on_finish,
    };
    return listener;
  }
  return NULL;
}

inline static void listener_on_server_start(size_t fd) {
  intptr_t uuid = sock_fd2uuid(fd);
  if (uuid < 0)
    fprintf(stderr, "ERROR: listening socket dropped?\n"), exit(4);
  if (evio_add(fd, (void *)uuid) < 0)
    perror("Couldn't register listening socket"), exit(4);
  fd_data(fd).active = facil_data->last_cycle;
  // call the on_init callback
  struct ListenerProtocol *listener =
      (struct ListenerProtocol *)uuid_data(uuid).protocol;
  listener->on_start(listener->udata);
}

/**
Listens to a server with the following server settings (which MUST include
a default protocol).

This method blocks the current thread until the server is stopped (either
though a `srv_stop` function or when a SIGINT/SIGTERM is received).
*/
int facil_listen(struct facil_listen_args settings) {
  if (!facil_data)
    facil_lib_init();
  if (settings.on_open == NULL || settings.port == NULL)
    return -1;
  intptr_t uuid = sock_listen(settings.address, settings.port);
  if (uuid == -1) {
    return -1;
  }
  protocol_s *protocol = (void *)listener_alloc(settings);
  facil_attach(uuid, protocol);
  if (!protocol) {
    sock_close(uuid);
    return -1;
  }
  if (FACIL_PRINT_STATE)
    fprintf(stderr, "* Listening on port %s\n", settings.port);
  return 0;
}

/* *****************************************************************************
Timers
***************************************************************************** */

/* *******
Timer Protocol
******* */
typedef struct {
  protocol_s protocol;
  size_t milliseconds;
  size_t repetitions;
  void (*task)(void *);
  void (*on_finish)(void *);
  void *arg;
} timer_protocol_s;

#define prot2timer(protocol) (*((timer_protocol_s *)(protocol)))

const char *timer_protocol_name = "timer protocol __facil_internal__";

static void timer_on_data(intptr_t uuid, protocol_s *protocol) {
  prot2timer(protocol).task(prot2timer(protocol).arg);
  evio_reset_timer(uuid);
  if (prot2timer(protocol).repetitions) {
    prot2timer(protocol).repetitions -= 1;
    if (prot2timer(protocol).repetitions == 0) {
      evio_remove(sock_uuid2fd(uuid));
      sock_force_close(sock_fd2uuid(uuid));
    }
  }
}

static void timer_on_close(protocol_s *protocol) {
  prot2timer(protocol).on_finish(prot2timer(protocol).arg);
  free(protocol);
}

static inline timer_protocol_s *timer_alloc(void (*task)(void *), void *arg,
                                            size_t milliseconds,
                                            size_t repetitions,
                                            void (*on_finish)(void *)) {
  if (!on_finish)
    on_finish = (void (*)(void *))mock_on_close;
  timer_protocol_s *t = malloc(sizeof(*t));
  if (t)
    *t = (timer_protocol_s){
        .protocol.service = timer_protocol_name,
        .protocol.on_data = timer_on_data,
        .protocol.on_close = timer_on_close,
        .arg = arg,
        .task = task,
        .on_finish = on_finish,
        .milliseconds = milliseconds,
        .repetitions = repetitions,
    };
  return t;
}

inline static void timer_on_server_start(int fd) {
  if (evio_add_timer(
          fd, (void *)sock_fd2uuid(fd),
          prot2timer(uuid_data(sock_fd2uuid(fd)).protocol).milliseconds))
    perror("Couldn't register a required timed event."), exit(4);
}

/**
 * Creates a system timer (at the cost of 1 file descriptor).
 *
 * The task will repeat `repetitions` times. If `repetitions` is set to 0, task
 * will repeat forever.
 *
 * Returns -1 on error or the new file descriptor on succeess.
 */
int facil_run_every(size_t milliseconds, size_t repetitions,
                    void (*task)(void *), void *arg,
                    void (*on_finish)(void *)) {
  if (task == NULL)
    return -1;
  timer_protocol_s *protocol = NULL;
  intptr_t uuid = -1;
  int fd = evio_open_timer();
  if (fd == -1) {
    perror("ERROR: couldn't create a timer fd");
    goto error;
  }
  uuid = sock_open(fd);
  if (uuid == -1)
    goto error;
  protocol = timer_alloc(task, arg, milliseconds, repetitions, on_finish);
  if (protocol == NULL)
    goto error;
  facil_attach(uuid, (protocol_s *)protocol);
  if (evio_isactive() && evio_add_timer(fd, (void *)uuid, milliseconds) < 0)
    goto error;
  return 0;
error:
  if (uuid != -1)
    sock_close(uuid);
  else if (fd != -1)
    close(fd);
  return -1;
}

/* *****************************************************************************
Running the server
***************************************************************************** */

static void print_pid(void *arg) {
  (void)arg;
  fprintf(stderr, "* %d is running.\n", getpid());
}

static void facil_review_timeout(void *arg) {
  protocol_s *tmp;
  time_t review = facil_data->last_cycle;
  uintptr_t fd = (uintptr_t)arg;

  uint16_t timeout = fd_data(fd).timeout;
  if (!timeout)
    timeout = 300; /* enforced timout settings */

  if (!fd_data(fd).protocol || (fd_data(fd).active + timeout >= review))
    goto finish;
  if (spn_trylock(&fd_data(fd).lock))
    goto reschedule;
  tmp = fd_data(fd).protocol;
  if (!tmp || spn_trylock(&prt_meta(tmp).sub_lock)) {
    spn_unlock(&fd_data(fd).lock);
    goto finish;
  }
  spn_unlock(&fd_data(fd).lock);
  if (prt_meta(tmp).lock)
    goto unlock;
  defer(deferred_ping, (void *)sock_fd2uuid(fd));
unlock:
  spn_unlock(&prt_meta(tmp).sub_lock);
finish:
  do {
    fd++;
  } while (!fd_data(fd).protocol && (fd < facil_data->capacity));

  if (facil_data->capacity <= fd) {
    facil_data->need_review = 1;
    return;
  }
reschedule:
  defer(facil_review_timeout, (void *)fd);
}

static void facil_cycle(void *arg) {
  static int idle = 0;
  time(&facil_data->last_cycle);
  int events = evio_review(defer_has_queue() ? 0 : 512);
  if (events < 0)
    goto error;
  if (events > 0) {
    idle = 1;
    goto finish;
  }
  if (idle) {
    ((struct facil_run_args *)arg)->on_idle();
    idle = 0;
  }
finish:
  if (!defer_fork_is_active())
    return;
  if (facil_data->need_review) {
    facil_data->need_review = 0;
    defer(facil_review_timeout, (void *)0);
  }
  defer(facil_cycle, arg);
error:
  (void)1;
}

static void facil_init_run(void *arg) {
  (void)arg;
  evio_create();
  time(&facil_data->last_cycle);
  for (size_t i = 0; i < facil_data->capacity; i++) {
    if (fd_data(i).protocol) {
      if (fd_data(i).protocol->service == listener_protocol_name)
        listener_on_server_start(i);
      else if (fd_data(i).protocol->service == timer_protocol_name)
        timer_on_server_start(i);
      else
        evio_add(i, (void *)sock_fd2uuid(i));
    }
  }
  facil_data->need_review = 1;
  defer(facil_cycle, arg);
}

static void facil_cleanup(void *arg) {
  if (FACIL_PRINT_STATE)
    fprintf(stderr, "\n   ---  starting shutdown  ---\n");
  intptr_t uuid;
  for (size_t i = 0; i < facil_data->capacity; i++) {
    if (fd_data(i).protocol && (uuid = sock_fd2uuid(i)) >= 0) {
      defer(deferred_on_shutdown, (void *)uuid);
    }
  }
  facil_cycle(arg);
  defer_perform();
  facil_cycle(arg);
  ((struct facil_run_args *)arg)->on_finish();
  defer_perform();
}

#undef facil_run
void facil_run(struct facil_run_args args) {
  if (!facil_data)
    facil_lib_init();
  if (!args.on_idle)
    args.on_idle = mock_idle;
  if (!args.on_finish)
    args.on_finish = mock_idle;
#ifdef _SC_NPROCESSORS_ONLN
  if (!args.threads && !args.processes) {
    ssize_t cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count > 0)
      args.threads = args.processes = cpu_count;
  }
#endif
  if (!args.processes)
    args.processes = 1;
  if (!args.threads)
    args.threads = 1;
  if (FACIL_PRINT_STATE) {
    fprintf(stderr, "Server is running, press ^C to stop\n");
    defer(print_pid, NULL);
  }
  defer(facil_init_run, &args);
  int frk = defer_perform_in_fork(args.processes, args.threads);
  facil_cleanup(&args);
  if (frk < 0) {
    perror("ERROR: couldn't spawn workers");
  } else if (frk > 0) {
    exit(0);
  }
}
/* *****************************************************************************
Setting the protocol
***************************************************************************** */

/** Attaches (or updates) a protocol object to a socket UUID.
 * Returns -1 on error and 0 on success.
 */
int facil_attach(intptr_t uuid, protocol_s *protocol) {
  if (!facil_data)
    facil_lib_init();
  if (protocol) {
    if (!protocol->on_close)
      protocol->on_close = mock_on_close;
    if (!protocol->on_data)
      protocol->on_data = mock_on_ev;
    if (!protocol->on_ready)
      protocol->on_ready = mock_on_ev;
    if (!protocol->ping)
      protocol->ping = mock_ping;
    if (!protocol->on_shutdown)
      protocol->on_shutdown = mock_on_ev;
    protocol->rsv = 0;
  }
  if (!sock_isvalid(uuid))
    return -1;
  spn_lock(&uuid_data(uuid).lock);
  protocol_s *old_protocol = uuid_data(uuid).protocol;
  uuid_data(uuid).protocol = protocol;
  uuid_data(uuid).active = facil_data->last_cycle;
  spn_unlock(&uuid_data(uuid).lock);
  if (old_protocol)
    defer(deferred_on_close, old_protocol);
  if (evio_isactive())
    evio_add(sock_uuid2fd(uuid), (void *)uuid);
  return 0;
}

/** Sets a timeout for a specific connection (if active). */
void facil_set_timeout(intptr_t uuid, uint8_t timeout) {
  if (sock_isvalid(uuid))
    uuid_data(uuid).timeout = timeout;
}

/* *****************************************************************************
Misc helpers
***************************************************************************** */

/**
Returns the last time the server reviewed any pending IO events.
*/
time_t facil_last_tick(void) { return facil_data->last_cycle; }
