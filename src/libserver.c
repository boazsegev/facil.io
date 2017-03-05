/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "libserver.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>

/* *****************************************************************************
Connection Data
*/
typedef struct {
  protocol_s *protocol;
  time_t active;
  uint8_t timeout;
  spn_lock_i lock;
} fd_data_s;

/*
These macros mean we won't need to change code if we change the locking system.
*/

#define lock_fd_init(fd) (fd)->lock = SPN_LOCK_INIT
#define lock_fd_destroy(fd) spn_unlock(&((fd)->lock))
/** returns 0 on success, value on failure */
#define try_lock_fd(fd) spn_trylock(&((fd)->lock))
#define lock_fd(fd) spn_lock(&((fd)->lock))
#define unlock_fd(fd) spn_unlock(&((fd)->lock))
#define clear_fd_data(fd_data)                                                 \
  { *(fd_data) = (fd_data_s){.lock = (fd_data)->lock}; }

/* *****************************************************************************
Server Core Data
*/
static struct {
  fd_data_s *fds;
  time_t last_tick;
  void (*on_idle)(void);
  size_t capacity;
  uint8_t running;
} server_data = {.fds = NULL};

/*
These macros help prevent code changes when changing the data struct.
*/

#define valid_uuid(uuid) sock_isvalid(uuid)

#define fd_data(fd) server_data.fds[(fd)]

#define uuid_data(uuid) fd_data(sock_uuid2fd(uuid))

#define clear_uuid(uuid) clear_fd_data(server_data.fds + sock_uuid2fd(uuid))

#define protocol_fd(fd) (server_data.fds[(fd)].protocol)
#define protocol_uuid(uuid) protocol_fd(sock_uuid2fd(uuid))

#define fduuid_get(ifd) (server_data.fds[(ifd)].uuid)

#define protocol_is_busy(protocol)                                             \
  spn_is_locked(&(((protocol_s *)(protocol))->callback_lock))
#define protocol_unset_busy(protocol)                                          \
  spn_unlock(&(((protocol_s *)(protocol))->callback_lock))
#define protocol_set_busy(protocol)                                            \
  spn_trylock(&(((protocol_s *)(protocol))->callback_lock))

#define try_lock_uuid(uuid) try_lock_fd(server_data.fds + sock_uuid2fd(uuid))
#define lock_uuid(uuid) lock_fd(server_data.fds + sock_uuid2fd(uuid))
#define unlock_uuid(uuid) unlock_fd(server_data.fds + sock_uuid2fd(uuid))

// run through any open sockets and call the shutdown handler
static inline void server_on_shutdown(void) {
  if (server_data.fds && server_data.capacity > 0) {
    intptr_t uuid;
    for (size_t i = 0; i < server_data.capacity; i++) {
      if (server_data.fds[i].protocol == NULL)
        continue;
      uuid = sock_fd2uuid(i);
      if (uuid != -1) {
        if (server_data.fds[i].protocol->on_shutdown != NULL)
          server_data.fds[i].protocol->on_shutdown(uuid,
                                                   server_data.fds[i].protocol);
        sock_close(uuid);
        sock_flush_strong(uuid);
      }
    }
  }
}

static void server_cleanup(void) {
  // run through any open sockets and call the shutdown handler
  server_on_shutdown();
  // free any lock objects (no need to change code if changing locking systems)
  for (size_t i = 0; i < server_data.capacity - 1; i++) {
    lock_fd_destroy(server_data.fds + i);
    server_data.fds[i] = (fd_data_s){.protocol = NULL};
  }
  // free memory
  if (server_data.fds) {
    munmap(server_data.fds, sizeof(fd_data_s) * server_data.capacity);
    server_data.fds = NULL;
  }
}

static void init_server(void) {
  pthread_mutex_t inner_lock = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&inner_lock);
  if (server_data.fds == NULL) {
    server_data.capacity = sock_max_capacity();
    atexit(server_cleanup);
    server_data.fds = mmap(NULL, sizeof(fd_data_s) * server_data.capacity,
                           PROT_READ | PROT_WRITE | PROT_EXEC,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    for (size_t i = 0; i < server_data.capacity - 1; i++) {
      server_data.fds[i] = (fd_data_s){.protocol = NULL};
      lock_fd_init(server_data.fds + i);
    }
  }
  pthread_mutex_unlock(&inner_lock);
}

/** initializes the library if it wasn't already initialized. */
#define validate_mem()                                                         \
  {                                                                            \
    if (server_data.fds == NULL)                                               \
      init_server();                                                           \
  }

/* *****************************************************************************
`libsock` Callback Implementation
*/

void sock_touch(intptr_t uuid) {
  if (server_data.fds != NULL)
    uuid_data(uuid).active = server_data.last_tick;
}

/* *****************************************************************************
The Reactor Callback Implementation
*/

void reactor_on_close_async(void *_pr) {
  if (protocol_set_busy(_pr) == 0) {
    ((protocol_s *)_pr)->on_close(_pr);
    return;
  }
  async_run(reactor_on_close_async, _pr);
}

void reactor_on_close(intptr_t uuid) {
  if (server_data.fds) {
    // get the currect state
    lock_uuid(uuid);
    protocol_s *protocol = protocol_uuid(uuid);
    // clear state
    clear_uuid(uuid);
    unlock_uuid(uuid);
    // call callback
    if (protocol && protocol->on_close)
      reactor_on_close_async(protocol);
  }
}

void reactor_on_data_async(void *_fduuid) {
  intptr_t fduuid = (intptr_t)_fduuid;
  if (!valid_uuid(fduuid) || protocol_uuid(fduuid) == NULL)
    return;
  // try to lock the socket
  if (try_lock_uuid(fduuid))
    goto no_lock;
  // get current state (protocol might have changed during this time)
  protocol_s *protocol = protocol_uuid(fduuid);
  // review protocol and get use privilage
  if (protocol == NULL || protocol_set_busy(protocol)) {
    // fprintf(stderr, "fduuid is busy %p\n", _fduuid);
    unlock_uuid(fduuid);
    goto no_lock;
  }
  // unlock
  unlock_uuid(fduuid);
  // fire event
  if (protocol && protocol->on_data)
    protocol->on_data(fduuid, protocol);
  // clear the original busy flag
  protocol_unset_busy(protocol);
  return;
no_lock:
  // fprintf(stderr, "no lock for %p\n", _fduuid);
  // failed to aquire lock / busy
  async_run(reactor_on_data_async, _fduuid);
}

void reactor_on_data(intptr_t fd) {
  async_run(reactor_on_data_async, (void *)fd);
}

void reactor_on_ready(intptr_t uuid) {
  uuid_data(uuid).active = server_data.last_tick;
  lock_uuid(uuid);
  protocol_s *protocol = protocol_uuid(uuid);
  unlock_uuid(uuid);
  if (protocol && protocol->on_ready && !sock_packets_pending(uuid))
    protocol->on_ready(uuid, protocol);
}

/* *****************************************************************************
Zombie Reaping
With thanks to Dr Graham D Shaw.
http://www.microhowto.info/howto/reap_zombie_processes_using_a_sigchld_handler.html
*/

void reap_child_handler(int sig) {
  (void)(sig);
  int old_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
  errno = old_errno;
}

inline static void reap_children(void) {
  struct sigaction sa;
  sa.sa_handler = reap_child_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  if (sigaction(SIGCHLD, &sa, 0) == -1) {
    perror("Child reaping initialization failed");
    exit(1);
  }
}

/* *****************************************************************************
Exit Signal Handling
*/

static void stop_server_handler(int sig) {
  server_data.running = 0;
  async_signal();
#if defined(SERVER_PRINT_STATE) && SERVER_PRINT_STATE == 1
  fprintf(stderr, "     --- Stop signal received ---\n");
#endif
  signal(sig, SIG_DFL);
}

inline static void listen_for_stop_signal(void) {
  struct sigaction sa;
  sa.sa_handler = stop_server_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  if (sigaction(SIGINT, &sa, 0) || sigaction(SIGTERM, &sa, 0)) {
    perror("Signal registration failed");
    exit(2);
  }
}

/* *****************************************************************************
The Listenning Protocol
*/

static const char *listener_protocol_name = "listening protocol __internal__";

struct ListenerProtocol {
  protocol_s protocol;
  protocol_s *(*on_open)(intptr_t uuid, void *udata);
  void *udata;
  void (*on_start)(void *udata);
  void (*on_finish)(void *udata);
};

static void listener_on_data(intptr_t uuid, protocol_s *_listener) {
  intptr_t new_client;
  struct ListenerProtocol *listener = (void *)_listener;
  while ((new_client = sock_accept(uuid)) != -1) {
    // assume that sock_accept calls  if needed
    // it's a clean slate in reactor_on_close ...
    // lock_uuid(new_client);
    // clear_uuid(new_client);
    // unlock_uuid(new_client);
    protocol_uuid(new_client) = listener->on_open(new_client, listener->udata);
    if (protocol_uuid(new_client)) {
      uuid_data(new_client).active = server_data.last_tick;
      protocol_unset_busy(protocol_uuid(new_client));
      reactor_add(new_client);
      continue;
    } else {
      sock_close(new_client);
    }
  }
}

static void free_listenner(void *_li) { free(_li); }

static void listener_on_close(protocol_s *_listener) {
  if (((struct ListenerProtocol *)_listener)->on_finish)
    ((struct ListenerProtocol *)_listener)
        ->on_finish(((struct ListenerProtocol *)_listener)->udata);
  free_listenner(_listener);
}

static inline struct ListenerProtocol *
listener_alloc(struct ServerServiceSettings settings) {
  struct ListenerProtocol *listener = malloc(sizeof(*listener));
  if (listener) {
    *listener = (struct ListenerProtocol){
        .protocol.service = listener_protocol_name,
        .protocol.on_data = listener_on_data,
        .protocol.on_close = listener_on_close,
        .on_open = settings.on_open,
        .udata = settings.udata,
        .on_start = settings.on_start,
        .on_finish = settings.on_finish,
    };
    return listener;
  }
  return NULL;
}

inline static void listener_on_server_start(void) {
  for (size_t i = 0; i < server_data.capacity; i++) {
    if (protocol_fd(i) && protocol_fd(i)->service == listener_protocol_name) {
      if (reactor_add(sock_fd2uuid(i)))
        perror("Couldn't register listenning socket"), exit(4);
      // call the on_init callback
      if (((struct ListenerProtocol *)protocol_fd(i))->on_start)
        ((struct ListenerProtocol *)protocol_fd(i))
            ->on_start(((struct ListenerProtocol *)protocol_fd(i))->udata);
    }
  }
}
inline static void listener_on_server_shutdown(void) {
  for (size_t i = 0; i < server_data.capacity; i++) {
    if (protocol_fd(i) && protocol_fd(i)->service == listener_protocol_name) {
      sock_close(sock_fd2uuid(i));
    }
  }
}

/* *****************************************************************************
* The timer protocol
*/

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

const char *timer_protocol_name = "timer protocol __internal__";

static void timer_on_data(intptr_t uuid, protocol_s *protocol) {
  prot2timer(protocol).task(prot2timer(protocol).arg);
  if (prot2timer(protocol).repetitions) {
    prot2timer(protocol).repetitions -= 1;
    if (prot2timer(protocol).repetitions == 0) {
      // fprintf(stderr, "closing timer?\n");
      reactor_remove_timer(uuid);
      sock_force_close(uuid);
    }
  }
  reactor_reset_timer(uuid);
}

static void timer_on_close(protocol_s *protocol) {
  // fprintf(stderr, "timer closed\n");
  if (prot2timer(protocol).on_finish)
    prot2timer(protocol).on_finish(prot2timer(protocol).arg);
  free(protocol);
}

static inline timer_protocol_s *timer_alloc(void (*task)(void *), void *arg,
                                            size_t milliseconds,
                                            size_t repetitions,
                                            void (*on_finish)(void *)) {
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

inline static void timer_on_server_start(void) {
  for (size_t i = 0; i < server_data.capacity; i++) {
    if (protocol_fd(i) && protocol_fd(i)->service == timer_protocol_name) {
      if (reactor_add_timer(sock_fd2uuid(i),
                            prot2timer(protocol_fd(i)).milliseconds))
        perror("Couldn't register a required timed event."), exit(4);
    }
  }
}

/* *****************************************************************************
Reactor cycling and timeout handling
*/

static inline void timeout_review(void) {
  static time_t review = 0;
  if (review >= server_data.last_tick)
    return;
  time(&review);
  for (size_t i = 0; i < server_data.capacity; i++) {
    if (protocol_fd(i) == NULL)
      continue; // Protocol objects are required for open connections.
    if (fd_data(i).timeout == 0) {
      if (protocol_fd(i) && protocol_fd(i)->service != listener_protocol_name &&
          protocol_fd(i)->service != timer_protocol_name &&
          review - fd_data(i).active > 300) {
        sock_close(sock_fd2uuid(i));
      }
      continue;
    }
    if (fd_data(i).active + fd_data(i).timeout < review) {
      if (protocol_fd(i)->ping) {
        protocol_fd(i)->ping(sock_fd2uuid(i), protocol_fd(i));
      } else if (!protocol_is_busy(protocol_fd(i)) ||
                 (review - fd_data(i).active > 300)) {
        sock_close(sock_fd2uuid(i));
      }
    }
  }
}

static void server_cycle(void *unused) {
  (void)(unused);
  static int8_t perform_idle = 1;

#if SERVER_DELAY_IO
  if (async_any()) {
    async_run(server_cycle, NULL);
    return;
  }
#endif

  time(&server_data.last_tick);
  if (server_data.running) {
    timeout_review();
    int e_count = reactor_review();
    if (e_count < 0) {
      return;
    }
    if (e_count == 0) {
      if (perform_idle && server_data.on_idle)
        server_data.on_idle();
      perform_idle = 0;
    } else {
      perform_idle = 1;
    }
    async_run(server_cycle, NULL);
  }
}
/* *****************************************************************************
* The Server API
* (and helper functions)
*/

/* *****************************************************************************
* Server actions
*/

#undef server_listen
#undef server_run
/**
Listens to a server with the following server settings (which MUST include
a default protocol).

This method blocks the current thread until the server is stopped (either
though a `srv_stop` function or when a SIGINT/SIGTERM is received).
*/
int server_listen(struct ServerServiceSettings settings) {
  validate_mem();
  if (settings.on_open == NULL || settings.port == NULL)
    return -1;
  intptr_t fduuid = sock_listen(settings.address, settings.port);
  if (fduuid == -1)
    return -1;
  server_data.fds[sock_uuid2fd(fduuid)].protocol =
      (void *)listener_alloc(settings);
  if (server_data.fds[sock_uuid2fd(fduuid)].protocol == NULL)
    goto error;
  if (server_data.running && reactor_add(fduuid))
    goto error;
#if defined(SERVER_PRINT_STATE) && SERVER_PRINT_STATE == 1
  fprintf(stderr, "* Listenning on port %s\n", settings.port);
#endif
  return 0;
error:
  sock_close(fduuid);
  return -1;
}
/** runs the server, hanging the current process and thread. */
ssize_t server_run(struct ServerSettings settings) {
  validate_mem();
  if (server_data.running) {
    return -1;
  }
  reap_children();
  listen_for_stop_signal();
  server_data.running = 1;
  server_data.on_idle = settings.on_idle;
  if (settings.processes == 0)
    settings.processes = 1;

#if defined(SERVER_PRINT_STATE) && SERVER_PRINT_STATE == 1
  if (settings.threads == 0)
    fprintf(stderr, "* Running %u processes"
                    " in single thread mode.\n",
            settings.processes);
  else
    fprintf(stderr, "* Running %u processes"
                    " X %u threads.\n",
            settings.processes, settings.threads);
#endif

  pid_t rootpid = getpid();
  pid_t *children = NULL;
  if (settings.processes > 1) {
    children = malloc(sizeof(*children) * settings.processes);
    for (size_t i = 0; i < (size_t)(settings.processes - 1); i++) {
      if (fork() == 0)
        break;
    }
  }
  if (reactor_init() < 0)
    perror("Reactor initialization failed"), exit(3);
  listener_on_server_start();
  timer_on_server_start();
#if defined(SERVER_PRINT_STATE) && SERVER_PRINT_STATE == 1
  fprintf(stderr, "* [%d] Running.\n", getpid());
#endif
  async_start(settings.threads);
  if (settings.on_init)
    settings.on_init();
  async_run(server_cycle, NULL);
  if (settings.threads > 0)
    async_join();
  else
    async_perform();

  /*
   * start a single worker thread for shutdown tasks and async close operations
   */
  async_start(1);

  listener_on_server_shutdown();
  reactor_review();
  server_on_shutdown();
  /* cycle until no IO events occure. */
  while (reactor_review() > 0)
    ;
  if (settings.on_finish)
    settings.on_finish();

  /*
   * Wait for any unfinished tasks.
   */
  async_finish();

  if (children) {
    if (rootpid == getpid()) {
      while (waitpid(-1, NULL, 0) >= 0)
        ;
    }
    free(children);
  }
#if defined(SERVER_PRINT_STATE) && SERVER_PRINT_STATE == 1
  fprintf(stderr, "* [%d] Shutdown.\n", getpid());
  if (rootpid == getpid())
    fprintf(stderr, "* Shutdown process complete.\n");
#endif
  if (rootpid != getpid())
    exit(0);

  return 0;
}

void server_stop(void) { server_data.running = 0; }
/**
Returns the last time the server reviewed any pending IO events.
*/
time_t server_last_tick(void) { return server_data.last_tick; }

/* *****************************************************************************
* Socket actions
*/

/**
Sets a new active protocol object for the requested file descriptor.

This also schedules the old protocol's `on_close` callback to run, making sure
all resources are released.

Returns -1 on error (i.e. connection closed), otherwise returns 0.
*/
ssize_t server_switch_protocol(intptr_t fd, protocol_s *new_protocol) {
  if (new_protocol == NULL || valid_uuid(fd) == 0)
    return -1;
  protocol_s *old_protocol;
  lock_uuid(fd);
  old_protocol = uuid_data(fd).protocol;
  uuid_data(fd).protocol = new_protocol;
  unlock_uuid(fd);
  if (old_protocol && old_protocol->on_close)
    reactor_on_close_async(old_protocol);
  return 0;
}
/**
Gets the active protocol object for the requested file descriptor.

Returns NULL on error (i.e. connection closed), otherwise returns a `protocol_s`
pointer.
*/
protocol_s *server_get_protocol(intptr_t uuid) {
  if (valid_uuid(uuid) == 0)
    return NULL;
  protocol_s *protocol;
  lock_uuid(uuid);
  protocol = uuid_data(uuid).protocol;
  unlock_uuid(uuid);
  return protocol;
}
/**
Sets a connection's timeout.

Returns -1 on error (i.e. connection closed), otherwise returns 0.
*/
void server_set_timeout(intptr_t fd, uint8_t timeout) {
  if (valid_uuid(fd) == 0) {
    return;
  }
  lock_uuid(fd);
  uuid_data(fd).active = server_data.last_tick;
  uuid_data(fd).timeout = timeout;
  unlock_uuid(fd);
}
/**
Gets a connection's timeout, type of uint8_t.

A value of 0 might mean that no timeout was set OR that the connection inquired
about was invalid.
*/
uint8_t server_get_timeout(intptr_t fd) {
  if (valid_uuid(fd) == 0)
    return 0;
  return uuid_data(fd).timeout;
}

/** Attaches an existing connection (fd) to the server's reactor and protocol
management system, so that the server can be used also to manage connection
based resources asynchronously (i.e. database resources etc').

On failure the fduuid_u.data.fd value will be -1.
*/
intptr_t server_attach(int fd, protocol_s *protocol) {
  intptr_t uuid = sock_open(fd);
  if (uuid == -1)
    return -1;
  protocol_fd(fd) = protocol;
  if (reactor_add(uuid)) {
    sock_close(uuid);
    return -1;
  }
  return uuid;
}
/** Hijack a socket (file descriptor) from the server, clearing up it's
resources. The control of hte socket is totally relinquished.

This method will block until all the data in the buffer is sent before
releasing control of the socket.

The returned value is the fd for the socket, or -1 on error.
*/
int server_hijack(intptr_t uuid) {
  if (sock_isvalid(uuid) == 0)
    return -1;
  reactor_remove(uuid);
  sock_flush_strong(uuid);
  if (sock_isvalid(uuid) == 0)
    return -1;
  protocol_s *old_protocol;
  lock_uuid(uuid);
  old_protocol = uuid_data(uuid).protocol;
  uuid_data(uuid).protocol = NULL;
  unlock_uuid(uuid);
  if (old_protocol && old_protocol->on_close)
    reactor_on_close_async(old_protocol);
  return sock_uuid2fd(uuid);
}
/** Counts the number of connections for the specified protocol (NULL = all
protocols). */
long server_count(char *service) {
  long count = 0;
  if (service == NULL) {
    for (size_t i = 0; i < server_data.capacity; i++) {
      if (protocol_fd(i) && protocol_fd(i)->service != listener_protocol_name &&
          protocol_fd(i)->service != timer_protocol_name)
        count++;
    }
  } else {
    for (size_t i = 0; i < server_data.capacity; i++) {
      if (protocol_fd(i) && protocol_fd(i)->service == service)
        count++;
    }
  }
  return count;
}

/* *****************************************************************************
* Connection Tasks (each and deffer tactics implementations)
*/

/* *******
Task core data
******* */
typedef struct {
  intptr_t origin;
  intptr_t target;
  const char *service;
  void (*task)(intptr_t fd, protocol_s *protocol, void *arg);
  void *on_finish;
  void *arg;
} srv_task_s;

/* Get task from void pointer. */
#define p2task(task) (*((srv_task_s *)(task)))

/* Get fallback callback from the task object. */
#define task2fallback(task)                                                    \
  ((void (*)(intptr_t, void *))(p2task(task).on_finish))

/* Get on_finished callback from the task object. */
#define task2on_done(task)                                                     \
  ((void (*)(intptr_t, protocol_s *, void *))(p2task(task).on_finish))
/* allows for later implementation of a task pool with minimal code updates. */
static inline srv_task_s *task_alloc(void) {
  return malloc(sizeof(srv_task_s));
}

/* allows for later implementation of a task pool with minimal code updates. */
static inline void task_free(srv_task_s *task) { free(task); }

/* performs a single connection task. */
static void perform_single_task(void *task) {
  if (p2task(task).target < 0 || sock_isvalid(p2task(task).target) == 0) {
    if (p2task(task).on_finish) // an invalid connection fallback
      task2fallback(task)(p2task(task).origin, p2task(task).arg);
    task_free(task);
    return;
  }
  if (try_lock_uuid(p2task(task).target) == 0) {
    // get protocol
    protocol_s *protocol = protocol_uuid(p2task(task).target);
    if (protocol && protocol_set_busy(protocol) == 0) {
      // clear the original busy flag
      unlock_uuid(p2task(task).target);
      p2task(task).task(p2task(task).target, protocol, p2task(task).arg);
      protocol_unset_busy(protocol);
      task_free(task);
      return;
    }
    unlock_uuid(p2task(task).target);
  }
  async_run(perform_single_task, task);
}

/* performs a connection group task. */
static void perform_each_task(void *task) {
  intptr_t uuid;
  protocol_s *protocol;
  while (p2task(task).target < (intptr_t)server_data.capacity) {
    uuid = sock_fd2uuid(p2task(task).target);
    if (uuid == -1 || uuid == p2task(task).origin) {
      ++p2task(task).target;
      continue;
    }
    if (try_lock_uuid(uuid) == 0) {
      protocol = protocol_uuid(uuid);
      if (protocol == NULL || protocol->service != p2task(task).service) {
        unlock_uuid(uuid);
        ++p2task(task).target;
        continue;
      } else if (protocol_set_busy(protocol) == 0) {
        // unlock uuid
        unlock_uuid(uuid);
        // perform task
        p2task(task).task(uuid, protocol, p2task(task).arg);
        // clear the busy flag
        protocol_unset_busy(protocol);
        // step forward
        ++p2task(task).target;
        continue;
      }
      // it's the right protocol and service, but we couldn't lock the protocol
      unlock_uuid(uuid);
    }
    async_run(perform_each_task, task);
    return;
  }
  if (p2task(task).on_finish) { // finished group task callback
    task2on_done(task)(p2task(task).origin,
                       (sock_isvalid(p2task(task).origin)
                            ? protocol_uuid(p2task(task).origin)
                            : NULL),
                       p2task(task).arg);
  }
  task_free(task);
  return;
}

/* *******
API
******* */

/**
Performs a task for each connection except the origin connection, unsafely and
synchronously.
*/
void server_each_unsafe(intptr_t origin_uuid,
                        void (*task)(intptr_t origin_uuid, intptr_t target_uuid,
                                     protocol_s *target_protocol, void *arg),
                        void *arg) {
  intptr_t target;
  protocol_s *protocol;
  for (size_t i = 0; i < server_data.capacity; i++) {
    target = sock_fd2uuid(p2task(task).target);
    if (target == -1)
      continue;
    protocol = protocol_uuid(target);
    if (protocol == NULL || protocol->service == listener_protocol_name ||
        protocol->service == timer_protocol_name)
      continue;
    task(origin_uuid, target, protocol, arg);
  }
}

/**
Schedules a specific task to run asyncronously for each connection (except the
origin connection) on a specific protocol.
*/
void server_each(intptr_t origin_fd, const char *service,
                 void (*task)(intptr_t fd, protocol_s *protocol, void *arg),
                 void *arg, void (*on_finish)(intptr_t fd, protocol_s *protocol,
                                              void *arg)) {
  srv_task_s *t = NULL;
  if (service == NULL || task == NULL)
    goto error;
  t = task_alloc();
  if (t == NULL)
    goto error;
  *t = (srv_task_s){.service = service,
                    .origin = origin_fd,
                    .task = task,
                    .on_finish = (void *)on_finish,
                    .arg = arg};
  if (async_run(perform_each_task, t))
    goto error;
  return;
error:
  if (t)
    task_free(t);
  if (on_finish)
    on_finish(origin_fd,
              (sock_isvalid(origin_fd) ? protocol_uuid(origin_fd) : NULL), arg);
}

/** Schedules a specific task to run asyncronously for a specific connection.
 */
void server_task(intptr_t caller_fd,
                 void (*task)(intptr_t fd, protocol_s *protocol, void *arg),
                 void *arg, void (*fallback)(intptr_t fd, void *arg)) {
  srv_task_s *t = NULL;
  if (task == NULL)
    goto error;
  t = task_alloc();
  if (t == NULL)
    goto error;
  *t = (srv_task_s){.target = caller_fd,
                    .task = task,
                    .on_finish = (void *)fallback,
                    .arg = arg};
  if (async_run(perform_single_task, t))
    goto error;
  return;
error:
  if (t)
    task_free(t);
  if (fallback)
    fallback(caller_fd, arg);
}

/* *****************************************************************************
* Timed tasks
*/

/** Creates a system timer (at the cost of 1 file descriptor) and pushes the
timer to the reactor. The task will repeat `repetitions` times. if
`repetitions` is set to 0, task will repeat forever. Returns -1 on error
or the new file descriptor on succeess.
*/
int server_run_every(size_t milliseconds, size_t repetitions,
                     void (*task)(void *), void *arg,
                     void (*on_finish)(void *)) {
  validate_mem();
  if (task == NULL)
    return -1;
  timer_protocol_s *protocol = NULL;
  intptr_t uuid = -1;
  int fd = reactor_make_timer();
  if (fd == -1) {
    // perror("couldn't create a timer fd");
    goto error;
  }
  uuid = sock_open(fd);
  if (uuid == -1)
    goto error;
  clear_uuid(uuid);
  protocol = timer_alloc(task, arg, milliseconds, repetitions, on_finish);
  if (protocol == NULL)
    goto error;
  protocol_fd(fd) = (protocol_s *)protocol;
  if (server_data.running && reactor_add_timer(uuid, milliseconds))
    goto error;
  return 0;
error:
  if (uuid != -1)
    sock_close(uuid);
  else if (fd != -1)
    close(fd);

  if (protocol != NULL) {
    protocol_fd(fd) = NULL;
    timer_on_close((protocol_s *)protocol);
  }
  return -1;
}
