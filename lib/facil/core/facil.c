/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "spnlock.inc"

#include "evio.h"
#include "facil.h"
#include "fio_list.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

/* *****************************************************************************
Data Structures
***************************************************************************** */
typedef struct ProtocolMetadata {
  spn_lock_i locks[3];
  unsigned rsv : 8;
} protocol_metadata_s;

union protocol_metadata_union_u {
  size_t opaque;
  protocol_metadata_s meta;
};
#define prt_meta(prt) (((union protocol_metadata_union_u *)(&(prt)->rsv))->meta)

struct connection_data_s {
  protocol_s *protocol;
  time_t active;
  uint8_t timeout;
  spn_lock_i lock;
};

static struct facil_data_s {
  spn_lock_i global_lock;
  uint8_t need_review;
  ssize_t capacity;
  time_t last_cycle;
  pid_t parent;
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
/** locks a connection's protocol returns a pointer that need to be unlocked. */
inline static protocol_s *protocol_try_lock(intptr_t fd,
                                            enum facil_protocol_lock_e type) {
  if (spn_trylock(&fd_data(fd).lock))
    goto would_block;
  protocol_s *pr = fd_data(fd).protocol;
  if (!pr) {
    spn_unlock(&fd_data(fd).lock);
    errno = EBADF;
    return NULL;
  }
  if (spn_trylock(&prt_meta(pr).locks[type])) {
    spn_unlock(&fd_data(fd).lock);
    goto would_block;
  }
  spn_unlock(&fd_data(fd).lock);
  return pr;
would_block:
  errno = EWOULDBLOCK;
  return NULL;
}
/** See `facil_protocol_try_lock` for details. */
inline static void protocol_unlock(protocol_s *pr,
                                   enum facil_protocol_lock_e type) {
  spn_unlock(&prt_meta(pr).locks[type]);
}

/* *****************************************************************************
Deferred event handlers
***************************************************************************** */
static void deferred_on_close(void *uuid_, void *pr_) {
  protocol_s *pr = pr_;
  if (pr->rsv)
    goto postpone;
  pr->on_close((intptr_t)uuid_, pr);
  return;
postpone:
  defer(deferred_on_close, uuid_, pr_);
}

static void deferred_on_ready(void *arg, void *arg2) {
  if (!uuid_data(arg).protocol)
    return;
  protocol_s *pr = protocol_try_lock(sock_uuid2fd(arg), FIO_PR_LOCK_WRITE);
  if (!pr)
    goto postpone;
  pr->on_ready((intptr_t)arg, pr);
  protocol_unlock(pr, FIO_PR_LOCK_WRITE);
  return;
postpone:
  defer(deferred_on_ready, arg, NULL);
  (void)arg2;
}

static void deferred_on_shutdown(void *arg, void *arg2) {
  if (!uuid_data(arg).protocol)
    return;
  protocol_s *pr = protocol_try_lock(sock_uuid2fd(arg), FIO_PR_LOCK_WRITE);
  if (!pr)
    goto postpone;
  pr->on_shutdown((intptr_t)arg, pr);
  protocol_unlock(pr, FIO_PR_LOCK_WRITE);
  sock_close((intptr_t)arg);
  return;
postpone:
  defer(deferred_on_shutdown, arg, NULL);
  (void)arg2;
}

static void deferred_on_data(void *arg, void *arg2) {
  if (!uuid_data(arg).protocol)
    return;
  protocol_s *pr = protocol_try_lock(sock_uuid2fd(arg), FIO_PR_LOCK_TASK);
  if (!pr)
    goto postpone;
  pr->on_data((intptr_t)arg, pr);
  protocol_unlock(pr, FIO_PR_LOCK_TASK);
  return;
postpone:
  defer(deferred_on_data, arg, NULL);
  (void)arg2;
}

static void deferred_ping(void *arg, void *arg2) {
  if (!uuid_data(arg).protocol ||
      (uuid_data(arg).timeout &&
       (uuid_data(arg).timeout >
        (facil_data->last_cycle - uuid_data(arg).active)))) {
    return;
  }
  protocol_s *pr = protocol_try_lock(sock_uuid2fd(arg), FIO_PR_LOCK_WRITE);
  if (!pr)
    goto postpone;
  pr->ping((intptr_t)arg, pr);
  protocol_unlock(pr, FIO_PR_LOCK_WRITE);
  return;
postpone:
  defer(deferred_ping, arg, NULL);
  (void)arg2;
}

/* *****************************************************************************
Event Handlers (evio)
***************************************************************************** */
static void sock_flush_defer(void *arg, void *ignored) {
  (void)ignored;
  sock_flush((intptr_t)arg);
}

void evio_on_ready(void *arg) {
  defer(sock_flush_defer, arg, NULL);
  defer(deferred_on_ready, arg, NULL);
}
void evio_on_close(void *arg) { sock_force_close((intptr_t)arg); }
void evio_on_error(void *arg) { sock_force_close((intptr_t)arg); }
void evio_on_data(void *arg) { defer(deferred_on_data, arg, NULL); }

/* *****************************************************************************
Forcing IO events
***************************************************************************** */

void facil_force_event(intptr_t uuid, enum facil_io_event ev) {
  switch (ev) {
  case FIO_EVENT_ON_DATA:
    evio_on_data((void *)uuid);
    break;
  case FIO_EVENT_ON_TIMEOUT:
    defer(deferred_ping, (void *)uuid, NULL);
    break;
  case FIO_EVENT_ON_READY:
    evio_on_ready((void *)uuid);
    break;
  }
}

/* *****************************************************************************
Socket callbacks
***************************************************************************** */

void sock_on_close(intptr_t uuid) {
  spn_lock(&uuid_data(uuid).lock);
  protocol_s *old_protocol = uuid_data(uuid).protocol;
  clear_connection_data_unsafe(uuid, NULL);
  spn_unlock(&uuid_data(uuid).lock);
  if (old_protocol)
    defer(deferred_on_close, (void *)uuid, old_protocol);
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
  ssize_t capa = sock_max_capacity();
  if (capa < 0)
    perror("ERROR: socket capacity unknown / failure"), exit(ENOMEM);
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
  *facil_data = (struct facil_data_s){.capacity = capa, .parent = getpid()};
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
Mock Protocol Callbacks and Service Funcions
***************************************************************************** */
static void mock_on_ev(intptr_t uuid, protocol_s *protocol) {
  (void)uuid;
  (void)protocol;
}

static void mock_on_close(intptr_t uuid, protocol_s *protocol) {
  (void)(protocol);
  (void)(uuid);
}

static void mock_on_finish(intptr_t uuid, void *udata) {
  (void)(udata);
  (void)(uuid);
}

static void mock_ping(intptr_t uuid, protocol_s *protocol) {
  (void)(protocol);
  sock_force_close(uuid);
}
static void mock_idle(void) {}

/* Support for the default pub/sub cluster engine */
#pragma weak pubsub_cluster_init
void pubsub_cluster_init(void) {}

/*
 * Support for the (removed) file caching service.
 *
 * It's possible to re-add the service from the `unused` folder.
 */
#pragma weak fio_cfd_clear
void fio_cfd_clear(void) {}

/* perform initialization for external services. */
static void facil_external_init(void) { pubsub_cluster_init(); }

/* perform cleanup for external services. */
static void facil_external_cleanup(void) { fio_cfd_clear(); }

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
  void *rw_udata;
  sock_rw_hook_s *(*set_rw_hooks)(intptr_t uuid, void *udata);
  void (*on_finish_rw)(intptr_t uuid, void *rw_udata);
  void (*on_start)(intptr_t uuid, void *udata);
  void (*on_finish)(intptr_t uuid, void *udata);
  char port[16];
};

static sock_rw_hook_s *listener_set_rw_hooks(intptr_t uuid, void *udata) {
  (void)udata;
  (void)uuid;
  return NULL; /* (sock_rw_hook_s *)&SOCK_DEFAULT_HOOKS; */
}

static void listener_ping(intptr_t uuid, protocol_s *plistener) {
  // fprintf(stderr, "*** Listener Ping Called for %ld\n", sock_uuid2fd(uuid));
  uuid_data(uuid).active = facil_data->last_cycle;
  return;
  (void)plistener;
}

/* called asynchronously after a new connection was opened */
static void listener_deferred_on_open(void *uuid_, void *srv_uuid_) {
  intptr_t uuid = (intptr_t)uuid_;
  intptr_t srv_uuid = (intptr_t)srv_uuid_;
  struct ListenerProtocol *listener =
      (struct ListenerProtocol *)protocol_try_lock(sock_uuid2fd(srv_uuid),
                                                   FIO_PR_LOCK_WRITE);
  if (!listener) {
    if (sock_isvalid(uuid) && sock_isvalid(srv_uuid))
      defer(listener_deferred_on_open, uuid_, srv_uuid_);
    else
      sock_close(uuid);
    return;
  }
  {
    sock_rw_hook_s *hooks = listener->set_rw_hooks(uuid, listener->rw_udata);
    if (hooks)
      sock_rw_hook_set(uuid, hooks);
  }
  protocol_s *pr = listener->on_open(uuid, listener->udata);
  protocol_unlock((protocol_s *)listener, FIO_PR_LOCK_WRITE);
  facil_attach(uuid, pr);
  if (!pr)
    sock_close(uuid);
}

static void listener_on_data(intptr_t uuid, protocol_s *plistener) {
  intptr_t new_client;
  if ((new_client = sock_accept(uuid)) == -1) {
    if (errno == ECONNABORTED || errno == ECONNRESET)
      defer(deferred_on_data, (void *)uuid, NULL);
    else if (errno != EWOULDBLOCK)
      perror("ERROR: socket accept error");
    return;
  }
  defer(listener_deferred_on_open, (void *)new_client, (void *)uuid);
  defer(deferred_on_data, (void *)uuid, NULL);
  return;
  (void)plistener;
}

static void free_listenner(void *li) { free(li); }

static void listener_on_close(intptr_t uuid, protocol_s *plistener) {
  struct ListenerProtocol *listener = (void *)plistener;
  listener->on_finish(uuid, listener->udata);
  listener->on_finish_rw(uuid, listener->rw_udata);
  if (FACIL_PRINT_STATE)
    fprintf(stderr, "* (%d) Stopped listening on port %s\n", getpid(),
            listener->port);
  free_listenner(listener);
}

static inline struct ListenerProtocol *
listener_alloc(struct facil_listen_args settings) {
  if (!settings.on_start)
    settings.on_start = mock_on_finish;
  if (!settings.on_finish)
    settings.on_finish = (void (*)(intptr_t, void *))mock_on_close;
  if (!settings.on_finish_rw)
    settings.on_finish_rw = mock_on_finish;
  if (!settings.set_rw_hooks)
    settings.set_rw_hooks = listener_set_rw_hooks;
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
        .on_finish_rw = settings.on_finish_rw,
        .set_rw_hooks = settings.set_rw_hooks,
        .rw_udata = settings.rw_udata,
    };
    size_t tmp = strlen(settings.port);
    memcpy(listener->port, settings.port, tmp + 1);
    return listener;
  }
  return NULL;
}

inline static void listener_on_start(size_t fd) {
  intptr_t uuid = sock_fd2uuid(fd);
  if (uuid < 0)
    fprintf(stderr, "ERROR: listening socket dropped?\n"), kill(0, SIGINT),
        exit(4);
  if (evio_add(fd, (void *)uuid) < 0)
    perror("Couldn't register listening socket"), kill(0, SIGINT), exit(4);
  fd_data(fd).active = facil_data->last_cycle;
  // call the on_init callback
  struct ListenerProtocol *listener =
      (struct ListenerProtocol *)uuid_data(uuid).protocol;
  listener->on_start(uuid, listener->udata);
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
Connect (as client)
***************************************************************************** */

static const char *connector_protocol_name = "connect protocol __internal__";

struct ConnectProtocol {
  protocol_s protocol;
  protocol_s *(*on_connect)(intptr_t uuid, void *udata);
  void (*on_fail)(intptr_t uuid, void *udata);
  sock_rw_hook_s *(*set_rw_hooks)(intptr_t uuid, void *udata);
  void *udata;
  void *rw_udata;
  intptr_t uuid;
  int opened;
};

/* The first `ready` signal is fired when a connection was established */
static void connector_on_ready(intptr_t uuid, protocol_s *_connector) {
  struct ConnectProtocol *connector = (void *)_connector;
  // fprintf(stderr, "connector_on_ready called\n");
  if (!connector->opened) {
    sock_rw_hook_s *hooks = connector->set_rw_hooks(uuid, connector->rw_udata);
    if (hooks)
      sock_rw_hook_set(uuid, hooks);
  }
  connector->opened = 1;
  sock_touch(uuid);
  if (facil_attach(uuid, connector->on_connect(uuid, connector->udata)) == -1)
    goto error;
  uuid_data(uuid).protocol->on_ready(uuid, uuid_data(uuid).protocol);
  return;
error:
  sock_close(uuid);
}

/* If data events reach this protocol, delay their execution. */
static void connector_on_data(intptr_t uuid, protocol_s *connector) {
  (void)connector;
  facil_force_event(uuid, FIO_EVENT_ON_DATA);
}

/* Failed to connect */
static void connector_on_close(intptr_t uuid, protocol_s *pconnector) {
  struct ConnectProtocol *connector = (void *)pconnector;
  if (connector->opened == 0 && connector->on_fail)
    connector->on_fail(connector->uuid, connector->udata);
  free(connector);
  (void)uuid;
}

#undef facil_connect
intptr_t facil_connect(struct facil_connect_args opt) {
  intptr_t uuid = -1;
  if (!opt.address || !opt.port || !opt.on_connect)
    goto error;
  if (!opt.set_rw_hooks)
    opt.set_rw_hooks = listener_set_rw_hooks;
  struct ConnectProtocol *connector = malloc(sizeof(*connector));
  if (!connector)
    goto error;
  *connector = (struct ConnectProtocol){
      .on_connect = opt.on_connect,
      .on_fail = opt.on_fail,
      .udata = opt.udata,
      .protocol.service = connector_protocol_name,
      .protocol.on_data = connector_on_data,
      .protocol.on_ready = connector_on_ready,
      .protocol.on_close = connector_on_close,
      .set_rw_hooks = opt.set_rw_hooks,
      .rw_udata = opt.rw_udata,
      .opened = 0,
  };
  uuid = connector->uuid = sock_connect(opt.address, opt.port);
  /* check for errors, always invoke the on_fail if required */
  if (uuid == -1)
    goto error;
  if (facil_attach(uuid, &connector->protocol) == -1) {
    sock_close(uuid);
    goto error;
  }
  return uuid;
error:
  if (opt.on_fail)
    opt.on_fail(uuid, opt.udata);
  return -1;
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

static const char *timer_protocol_name = "timer protocol __facil_internal__";

static void timer_on_data(intptr_t uuid, protocol_s *protocol) {
  prot2timer(protocol).task(prot2timer(protocol).arg);
  evio_reset_timer(sock_uuid2fd(uuid));
  if (prot2timer(protocol).repetitions == 0)
    return;
  prot2timer(protocol).repetitions -= 1;
  if (prot2timer(protocol).repetitions)
    return;
  evio_remove(sock_uuid2fd(uuid));
  sock_force_close(uuid);
}

static void timer_on_close(intptr_t uuid, protocol_s *protocol) {
  prot2timer(protocol).on_finish(prot2timer(protocol).arg);
  free(protocol);
  (void)uuid;
}

static void timer_ping(intptr_t uuid, protocol_s *protocol) {
  sock_touch(uuid);
  (void)protocol;
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
        .protocol.ping = timer_ping,
        .arg = arg,
        .task = task,
        .on_finish = on_finish,
        .milliseconds = milliseconds,
        .repetitions = repetitions,
    };
  return t;
}

inline static void timer_on_server_start(int fd) {
  if (evio_add_timer(fd, (void *)sock_fd2uuid(fd),
                     prot2timer(fd_data(fd).protocol).milliseconds))
    perror("Couldn't register a required timed event."), kill(0, SIGINT),
        exit(4);
}

/**
 * Creates a system timer (at the cost of 1 file descriptor).
 *
 * The task will repeat `repetitions` times. If `repetitions` is set to 0, task
 * will repeat forever.
 *
 * Returns -1 on error or the new file descriptor on succeess.
 *
 * The `on_finish` handler is always called (even on error).
 */
int facil_run_every(size_t milliseconds, size_t repetitions,
                    void (*task)(void *), void *arg,
                    void (*on_finish)(void *)) {
  if (task == NULL)
    goto error_fin;
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
  if (evio_isactive() && evio_add_timer(fd, (void *)uuid, milliseconds) == -1)
    goto error;
  return 0;
error:
  if (uuid != -1) {
    const int old = errno;
    sock_close(uuid);
    errno = old;
  } else if (fd != -1) {
    const int old = errno;
    close(fd);
    errno = old;
  }
error_fin:
  if (on_finish) {
    const int old = errno;
    on_finish(arg);
    errno = old;
  }
  return -1;
}

/* *****************************************************************************
Cluster Messaging
***************************************************************************** */
/* cluster state management */
static struct {
  uintptr_t count;
  fio_list_s handlers;
  spn_lock_i lock;
  struct facil_cluster_data_pipe {
    intptr_t in;
    intptr_t out;
  } * pipes;
} facil_cluster_data = {.handlers.next = &facil_cluster_data.handlers,
                        .handlers.prev = &facil_cluster_data.handlers,
                        .lock = SPN_LOCK_INIT};

/* message handler */
typedef struct {
  fio_list_s list;
  void (*on_message)(void *data, uint32_t len);
  int32_t msg_type;
} fio_cluster_handler;

/* message sending and protocol. */
struct facil_cluster_msg_packet {
  uint16_t count;
  spn_lock_i lock;
  struct facil_cluster_msg {
    uint32_t len;
    int32_t msg_type;
  } payload;
  uint8_t data[];
};

/* pipe communication protocol object. */
struct facil_cluster_protocol {
  protocol_s protocol;
  size_t mem_size;
  size_t read;
  struct facil_cluster_msg *msg;
};

/* cluster handling of message */
static void facil_cluster_handle_msg(struct facil_cluster_msg *msg) {
  fio_cluster_handler *hnd;
  spn_lock(&facil_cluster_data.lock);
  fio_list_for_each(fio_cluster_handler, list, hnd,
                    facil_cluster_data.handlers) {
    if (hnd->msg_type == msg->msg_type) {
      hnd->on_message(msg + 1, msg->len - sizeof(*msg));
    }
  }
  spn_unlock(&facil_cluster_data.lock);
}

/* cluster service ID */
static char *facil_cluster_protocol_id = "facil_io_internal_cluster_protocol";

/* cluster protocol on_close callback */
static void facil_cluster_on_close(intptr_t uuid, protocol_s *pr_) {
  struct facil_cluster_protocol *pr = (struct facil_cluster_protocol *)pr_;
  if (pr->msg)
    free(pr->msg);
  free(pr);
#if FACIL_PRINT_STATE == 1
  if (defer_fork_is_active())
    fprintf(stderr, "ERROR: facil cluster member (%d) closed prematurely.\n",
            defer_fork_pid());
#endif
  (void)uuid;
}

/* React when a sibling process closes / crashes */
static void facil_cluster_on_sibling_close(intptr_t uuid, protocol_s *pr_) {
  if (facil_cluster_data.pipes[defer_fork_pid()].in == uuid)
    return;
  if (defer_fork_is_active()) {
    kill(getpid(), SIGINT);
#if DEBUG
    fprintf(stderr,
            "* Sibling death detected, signaling self exit process %d\n",
            getpid());
#endif
  }
  (void)uuid;
  (void)pr_;
}

/* cluster protocol on_data callback */
static void facil_cluster_on_data(intptr_t uuid, protocol_s *pr_) {
  struct facil_cluster_protocol *pr = (struct facil_cluster_protocol *)pr_;
  while (1) {
    errno = 0;
    if (pr->read < 8 || pr->read < pr->msg->len) {
      ssize_t read = sock_read(uuid, (void *)((uintptr_t)pr->msg + pr->read),
                               pr->mem_size - pr->read);
      if (read <= 0) {
        if (read < 0)
          perror("ERROR: cluster pipe read error");
        return;
      }
      pr->read += read;
      if (pr->read < 8)
        continue;
      if (pr->mem_size < pr->msg->len) {
        void *tmp = realloc(pr->msg, pr->msg->len);
        if (!tmp)
          perror("ERROR: (critical) cannot allocate memory for a clustered "
                 "message."),
              kill(0, SIGINT), exit(errno);
        pr->msg = tmp;
#if defined(FACIL_PRINT_STATE) && FACIL_PRINT_STATE == 1 && defined(DEBUG)
        fprintf(stderr, "* Cluster (%d) Reallocated msg size to %u\n",
                defer_fork_pid(), pr->msg->len);
#endif
        pr->mem_size = pr->msg->len;
      }
    }
    if ((size_t)(pr->msg->len) <= pr->read) {
      facil_cluster_handle_msg(pr->msg);
      pr->read = pr->read - pr->msg->len;
      if (pr->read)
        memcpy(pr->msg, (void *)((uintptr_t)pr->msg + pr->msg->len), pr->read);
    }
  }
}

/* Registers process specific information */
static void facil_cluster_register(void *arg1, void *arg2) {
  (void)arg1;
  (void)arg2;
  if (facil_cluster_data.count < 2)
    return;

  /* used to watch for sibling crashes and prevent other events from firing */
  static protocol_s sentinel_protocol = {
      .service = NULL,
      .ping = listener_ping,
      .on_close = facil_cluster_on_sibling_close,
  };
  /* close what we don't need and watch the connections we do need */
  for (size_t i = 0; i < facil_cluster_data.count; i++) {
    if (i == (size_t)defer_fork_pid())
      continue;
    sock_close(facil_cluster_data.pipes[i].in);
    facil_attach(facil_cluster_data.pipes[i].out, &sentinel_protocol);
  }
  sock_close(facil_cluster_data.pipes[defer_fork_pid()].out);

  struct facil_cluster_protocol *pr = malloc(sizeof(*pr));
  if (!pr)
    perror("ERROR: (critical) cannot allocate memory for cluster."),
        kill(0, SIGINT), exit(errno);
  *pr = (struct facil_cluster_protocol){
      .protocol.service = facil_cluster_protocol_id,
      .protocol.on_data = facil_cluster_on_data,
      .protocol.on_close = facil_cluster_on_close,
      .protocol.ping = listener_ping,
  };
  pr->msg = calloc(1, 1024);
  pr->mem_size = 1024;
  if (!pr->msg)
    perror("ERROR: (critical) cannot allocate memory for cluster."),
        kill(0, SIGINT), exit(errno);
  facil_attach(facil_cluster_data.pipes[defer_fork_pid()].in, &pr->protocol);
}

static void facil_cluster_init(uint16_t count) {
  if (facil_cluster_data.pipes || count < 2)
    return;
  facil_cluster_data.pipes =
      malloc((sizeof(struct facil_cluster_data_pipe) * count));
  if (!facil_cluster_data.pipes)
    goto error;

  /* used to prevent any events from firing  */
  static protocol_s stub_protocol = {
      .service = NULL, .ping = listener_ping,
  };
  stub_protocol.service = facil_cluster_protocol_id;

  facil_cluster_data.count = count;
  int p_tmp[2];

  for (size_t i = 0; i < count; i++) {
    if (pipe(p_tmp))
      goto error;
    sock_set_non_block(p_tmp[0]);
    sock_set_non_block(p_tmp[1]);
    facil_cluster_data.pipes[i].in = sock_open(p_tmp[0]);
    facil_attach(facil_cluster_data.pipes[i].in, &stub_protocol);
    facil_cluster_data.pipes[i].out = sock_open(p_tmp[1]);
    facil_attach(facil_cluster_data.pipes[i].out, &stub_protocol);
  }
  defer(facil_cluster_register, NULL, NULL);
  return;
error:
  perror("ERROR: (facil.io) Couldn't initialize cluster pipes");
  kill(0, SIGINT), exit(errno);
}

static void facil_cluster_destroy(void) {
  if (!facil_cluster_data.pipes)
    return;
  for (size_t i = 0; i < facil_cluster_data.count; i++) {
    sock_close(facil_cluster_data.pipes[i].out);
    sock_close(facil_cluster_data.pipes[i].in);
  }

  fio_cluster_handler *hnd;
  while ((hnd = fio_list_pop(fio_cluster_handler, list,
                             facil_cluster_data.handlers)))
    free(hnd);
  free(facil_cluster_data.pipes);
  facil_cluster_data.pipes = NULL;
}

void facil_cluster_set_handler(int32_t msg_type,
                               void (*on_message)(void *data, uint32_t len)) {
  fio_cluster_handler *hnd;
  spn_lock(&facil_cluster_data.lock);
  fio_list_for_each(fio_cluster_handler, list, hnd,
                    facil_cluster_data.handlers) {
    if (hnd->msg_type == msg_type) {
      hnd->on_message = on_message;
      spn_unlock(&facil_cluster_data.lock);
      return;
    }
  }
  hnd = malloc(sizeof(*hnd));
  *hnd = (fio_cluster_handler){.msg_type = msg_type, .on_message = on_message};
  fio_list_push(fio_cluster_handler, list, facil_cluster_data.handlers, hnd);
  spn_unlock(&facil_cluster_data.lock);
}

static void facil_msg_free(void *msg_) {
  struct facil_cluster_msg_packet *msg = msg_;
  spn_lock(&msg->lock);
  msg->count--;
  if (msg->count) {
    spn_unlock(&msg->lock);
    return;
  }
  free(msg);
}
int facil_cluster_send(int32_t msg_type, void *data, uint32_t len) {
  if (!defer_fork_is_active()) {
#ifdef DEBUG
    fprintf(stderr,
            "WARNING: (debug, ignored) `facil_cluster_send` can only be "
            "called while facil.io is running.\n");
#endif
    return -1;
  }
  if (facil_cluster_data.count < 2)
    return 0;
  if (!data || !len)
    return -1;
  struct facil_cluster_msg_packet *msg = malloc(len + sizeof(*msg));
  if (!msg)
    return -1;
  msg->count = facil_cluster_data.count - 1;
  msg->lock = SPN_LOCK_INIT;
  msg->payload.len = len + sizeof(msg->payload);
  msg->payload.msg_type = msg_type;
  memcpy(msg->data, data, len);
  for (int i = 0; i < (int)facil_cluster_data.count; i++) {
    if (defer_fork_pid() == i)
      continue;
    if (sock_write2(.uuid = facil_cluster_data.pipes[i].out, .buffer = msg,
                    .offset =
                        ((uintptr_t) &
                         (((struct facil_cluster_msg_packet *)(0))->payload)),
                    .length = msg->payload.len, .move = 1,
                    .dealloc = facil_msg_free))
      perror("ERROR: Cluster `write` failed");
  }
  return 0;
}

/* *****************************************************************************
Running the server
***************************************************************************** */

static void print_pid(void *arg, void *ignr) {
  (void)arg;
  (void)ignr;
  fprintf(stderr, "* %d is running.\n", getpid());
}

static void facil_review_timeout(void *arg, void *ignr) {
  (void)ignr;
  protocol_s *tmp;
  time_t review = facil_data->last_cycle;
  intptr_t fd = (intptr_t)arg;

  uint16_t timeout = fd_data(fd).timeout;
  if (!timeout)
    timeout = 300; /* enforced timout settings */

  if (!fd_data(fd).protocol || (fd_data(fd).active + timeout >= review))
    goto finish;
  tmp = protocol_try_lock(fd, FIO_PR_LOCK_STATE);
  if (!tmp)
    goto reschedule;
  if (prt_meta(tmp).locks[FIO_PR_LOCK_TASK] ||
      prt_meta(tmp).locks[FIO_PR_LOCK_WRITE])
    goto unlock;
  defer(deferred_ping, (void *)sock_fd2uuid(fd), NULL);
unlock:
  protocol_unlock(tmp, FIO_PR_LOCK_STATE);
finish:
  do {
    fd++;
  } while (!fd_data(fd).protocol && (fd < facil_data->capacity));

  if (facil_data->capacity <= fd) {
    facil_data->need_review = 1;
    return;
  }
reschedule:
  defer(facil_review_timeout, (void *)fd, NULL);
}

static void facil_cycle(void *arg, void *ignr) {
  (void)ignr;
  static int idle = 0;
  time(&facil_data->last_cycle);
  int events;
  if (defer_has_queue()) {
    events = evio_review(0);
    if (events < 0)
      goto error;
    if (events > 0)
      idle = 1;
  } else {
    events = evio_review(512);
    if (events < 0)
      goto error;
    if (events > 0) {
      idle = 1;
    } else if (idle) {
      ((struct facil_run_args *)arg)->on_idle();
      idle = 0;
    }
  }
  if (!defer_fork_is_active())
    return;
  if (facil_data->need_review) {
    facil_data->need_review = 0;
    defer(facil_review_timeout, (void *)0, NULL);
  }
  defer(facil_cycle, arg, NULL);
error:
  (void)1;
}

static void facil_init_run(void *arg, void *arg2) {
  (void)arg;
  (void)arg2;
  evio_create();
  time(&facil_data->last_cycle);
  for (intptr_t i = 0; i < facil_data->capacity; i++) {
    if (fd_data(i).protocol) {
      if (fd_data(i).protocol->service == listener_protocol_name)
        listener_on_start(i);
      else if (fd_data(i).protocol->service == timer_protocol_name)
        timer_on_server_start(i);
      else
        evio_add(i, (void *)sock_fd2uuid(i));
    }
  }
  facil_data->need_review = 1;
  facil_external_init();
  defer(facil_cycle, arg, NULL);
}

static void facil_cleanup(void *arg) {
  fprintf(stderr, "* %d cleanning up.\n", getpid());
  intptr_t uuid;
  for (intptr_t i = 0; i < facil_data->capacity; i++) {
    if (fd_data(i).protocol && (uuid = sock_fd2uuid(i)) >= 0) {
      defer(deferred_on_shutdown, (void *)uuid, NULL);
    }
  }
  facil_cycle(arg, NULL);
  defer_perform();
  facil_cycle(arg, NULL);
  ((struct facil_run_args *)arg)->on_finish();
  defer_perform();
  evio_close();
  facil_cluster_destroy();
  facil_external_cleanup();
}

#undef facil_run
void facil_run(struct facil_run_args args) {
  signal(SIGPIPE, SIG_IGN);
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
  facil_cluster_init(args.processes);
  if (FACIL_PRINT_STATE) {
    fprintf(stderr, "Server is running %u %s X %u %s, press ^C to stop\n",
            args.processes, args.processes > 1 ? "workers" : "worker",
            args.threads, args.threads > 1 ? "threads" : "thread");
    defer(print_pid, NULL, NULL);
  }
  defer(facil_init_run, &args, NULL);
  int frk = defer_perform_in_fork(args.processes, args.threads);
  facil_cleanup(&args);
  if (frk < 0) {
    perror("ERROR: couldn't spawn workers");
  } else if (frk > 0) {
    exit(0);
  }
  if (FACIL_PRINT_STATE)
    fprintf(stderr, "\n   ---  Completed Shutdown  ---\n");
}
/* *****************************************************************************
Setting the protocol
***************************************************************************** */

static int facil_attach_state(intptr_t uuid, protocol_s *protocol,
                              protocol_metadata_s state) {
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
    prt_meta(protocol) = state;
  }
  spn_lock(&uuid_data(uuid).lock);
  if (!sock_isvalid(uuid)) {
    spn_unlock(&uuid_data(uuid).lock);
    if (protocol)
      defer(deferred_on_close, (void *)uuid, protocol);
    return -1;
  }
  protocol_s *old_protocol = uuid_data(uuid).protocol;
  uuid_data(uuid).protocol = protocol;
  uuid_data(uuid).active = facil_data->last_cycle;
  spn_unlock(&uuid_data(uuid).lock);
  if (old_protocol)
    defer(deferred_on_close, (void *)uuid, old_protocol);
  if (evio_isactive())
    evio_add(sock_uuid2fd(uuid), (void *)uuid);
  return 0;
}

/** Attaches (or updates) a protocol object to a socket UUID.
 * Returns -1 on error and 0 on success.
 */
int facil_attach(intptr_t uuid, protocol_s *protocol) {
  return facil_attach_state(uuid, protocol, (protocol_metadata_s){.rsv = 0});
}

/**
 * Attaches (or updates) a LOCKED protocol object to a socket UUID.
 */
int facil_attach_locked(intptr_t uuid, protocol_s *protocol) {
  {
    protocol_metadata_s state = {.rsv = 0};
    spn_lock(state.locks + FIO_PR_LOCK_TASK);
    return facil_attach_state(uuid, protocol, state);
  }
}

/** Sets a timeout for a specific connection (if active). */
void facil_set_timeout(intptr_t uuid, uint8_t timeout) {
  if (sock_isvalid(uuid)) {
    uuid_data(uuid).active = facil_data->last_cycle;
    uuid_data(uuid).timeout = timeout;
  }
}
/** Gets a timeout for a specific connection. Returns 0 if there's no set
 * timeout or the connection is inactive. */
uint8_t facil_get_timeout(intptr_t uuid) { return uuid_data(uuid).timeout; }

/* *****************************************************************************
Misc helpers
***************************************************************************** */

/**
Returns the last time the server reviewed any pending IO events.
*/
time_t facil_last_tick(void) {
  return facil_data ? facil_data->last_cycle : time(NULL);
}

/**
 * This function allows out-of-task access to a connection's `protocol_s`
 * object by attempting to lock it.
 */
protocol_s *facil_protocol_try_lock(intptr_t uuid,
                                    enum facil_protocol_lock_e type) {
  if (!sock_isvalid(uuid) || !uuid_data(uuid).protocol) {
    errno = EBADF;
    return NULL;
  }
  return protocol_try_lock(sock_uuid2fd(uuid), type);
}
/** See `facil_protocol_try_lock` for details. */
void facil_protocol_unlock(protocol_s *pr, enum facil_protocol_lock_e type) {
  if (!pr)
    return;
  protocol_unlock(pr, type);
}
/** Counts all the connections of a specific type. */
size_t facil_count(void *service) {
  long count = 0;
  void *tmp;
  for (intptr_t i = 0; i < facil_data->capacity; i++) {
    tmp = NULL;
    spn_lock(&fd_data(i).lock);
    if (fd_data(i).protocol && fd_data(i).protocol->service)
      tmp = (void *)fd_data(i).protocol->service;
    spn_unlock(&fd_data(i).lock);
    if (tmp != listener_protocol_name && tmp != timer_protocol_name &&
        (!service || (tmp == service)))
      count++;
  }
  return count;
}

/* *****************************************************************************
Task Management - `facil_defer`, `facil_each`
***************************************************************************** */

struct task {
  intptr_t origin;
  void (*func)(intptr_t uuid, protocol_s *, void *arg);
  void *arg;
  void (*on_done)(intptr_t uuid, void *arg);
  const void *service;
  uint32_t count;
  enum facil_protocol_lock_e task_type;
  spn_lock_i lock;
};

static inline struct task *alloc_facil_task(void) {
  return malloc(sizeof(struct task));
}

static inline void free_facil_task(struct task *task) { free(task); }

static void mock_on_task_done(intptr_t uuid, void *arg) {
  (void)uuid;
  (void)arg;
}

static void perform_single_task(void *v_uuid, void *v_task) {
  struct task *task = v_task;
  if (!uuid_data(v_uuid).protocol)
    goto fallback;
  protocol_s *pr = protocol_try_lock(sock_uuid2fd(v_uuid), task->task_type);
  if (!pr)
    goto defer;
  if (pr->service == connector_protocol_name) {
    protocol_unlock(pr, task->task_type);
    goto defer;
  }
  task->func((intptr_t)v_uuid, pr, task->arg);
  protocol_unlock(pr, task->task_type);
  free_facil_task(task);
  return;
fallback:
  task->on_done((intptr_t)v_uuid, task->arg);
  free_facil_task(task);
  return;
defer:
  defer(perform_single_task, v_uuid, v_task);
  return;
}

static void finish_multi_task(void *v_fd, void *v_task) {
  struct task *task = v_task;
  if (spn_trylock(&task->lock))
    goto reschedule;
  task->count--;
  if (task->count) {
    spn_unlock(&task->lock);
    return;
  }
  task->on_done(task->origin, task->arg);
  free_facil_task(task);
  return;
reschedule:
  defer(finish_multi_task, v_fd, v_task);
}

static void perform_multi_task(void *v_fd, void *v_task) {
  if (!fd_data((intptr_t)v_fd).protocol) {
    finish_multi_task(v_fd, v_task);
    return;
  }
  struct task *task = v_task;
  protocol_s *pr = protocol_try_lock((intptr_t)v_fd, task->task_type);
  if (!pr)
    goto reschedule;
  if (pr->service == task->service)
    task->func(sock_fd2uuid((intptr_t)v_fd), pr, task->arg);
  protocol_unlock(pr, task->task_type);
  defer(finish_multi_task, v_fd, v_task);
  return;
reschedule:
  // fprintf(stderr, "rescheduling multi for %p\n", v_fd);
  defer(perform_multi_task, v_fd, v_task);
}

static void schedule_multi_task(void *v_fd, void *v_task) {
  struct task *task = v_task;
  intptr_t fd = (intptr_t)v_fd;
  for (size_t i = 0; i < 64; i++) {
    if (!fd_data(fd).protocol)
      goto finish;
    if (spn_trylock(&fd_data(fd).lock))
      goto reschedule;
    if (!fd_data(fd).protocol ||
        fd_data(fd).protocol->service != task->service || fd == task->origin) {
      spn_unlock(&fd_data(fd).lock);
      goto finish;
    }
    spn_unlock(&fd_data(fd).lock);
    spn_lock(&task->lock);
    task->count++;
    spn_unlock(&task->lock);
    defer(perform_multi_task, (void *)fd, task);
  finish:
    do {
      fd++;
    } while (!fd_data(fd).protocol && (fd < facil_data->capacity));
    if (fd >= (intptr_t)facil_data->capacity)
      goto complete;
  }
reschedule:
  schedule_multi_task((void *)fd, v_task);
  return;
complete:
  defer(finish_multi_task, NULL, v_task);
}
/**
 * Schedules a protected connection task. The task will run within the
 * connection's lock.
 *
 * If the connection is closed before the task can run, the
 * `fallback` task wil be called instead, allowing for resource cleanup.
 */
#undef facil_defer
void facil_defer(struct facil_defer_args_s args) {
  if (!args.fallback)
    args.fallback = mock_on_task_done;
  if (!args.task_type)
    args.task_type = FIO_PR_LOCK_TASK;
  if (!args.task || !uuid_data(args.uuid).protocol || args.uuid < 0 ||
      !sock_isvalid(args.uuid))
    goto error;
  struct task *task = alloc_facil_task();
  if (!task)
    goto error;
  *task = (struct task){
      .func = args.task, .arg = args.arg, .on_done = args.fallback};
  defer(perform_single_task, (void *)args.uuid, task);
  return;
error:
  defer((void (*)(void *, void *))args.fallback, (void *)args.uuid, args.arg);
}

/**
 * Schedules a protected connection task for each `service` connection.
 * The tasks will run within each of the connection's locks.
 *
 * Once all the tasks were performed, the `on_complete` callback will be called.
 */
#undef facil_each
int facil_each(struct facil_each_args_s args) {
  if (!args.on_complete)
    args.on_complete = mock_on_task_done;
  if (!args.task_type)
    args.task_type = FIO_PR_LOCK_TASK;
  if (!args.task)
    goto error;
  struct task *task = alloc_facil_task();
  if (!task)
    goto error;
  *task = (struct task){.origin = args.origin,
                        .func = args.task,
                        .arg = args.arg,
                        .on_done = args.on_complete,
                        .service = args.service,
                        .task_type = args.task_type,
                        .count = 1};
  defer(schedule_multi_task, (void *)0, task);
  return 0;
error:
  defer((void (*)(void *, void *))args.on_complete, (void *)args.origin,
        args.arg);
  return -1;
}
