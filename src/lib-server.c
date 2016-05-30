/*
copyright: Boaz segev, 2016
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/

// lib server is based off and requires the following libraries:
#include "lib-server.h"
#include "libsock.h"

// #include <ssl.h>
// sys includes
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

////////////////////////////////////////////////////////////////////////////////
// The data associated with each connection
struct ConnectionData {
  server_pt srv;
  struct Protocol* protocol;
  void* udata;
  int fd;
  volatile uint32_t counter;
  unsigned tout : 8;
  unsigned idle : 8;
  volatile unsigned busy : 1;
};

////////////////////////////////////////////////////////////////////////////////
// Task object containers

// the data-type for async messages
struct FDTask {
  union fd_id conn_id;
  struct FDTask* next;
  server_pt server;
  void (*task)(server_pt server, uint64_t conn_id, void* arg);
  void* arg;
  void (*fallback)(server_pt server, uint64_t origin, void* arg);
};

// A self handling task structure
struct GroupTask {
  uint64_t conn_origin;
  struct GroupTask* next;
  server_pt server;
  char* service;
  void (*task)(server_pt server, uint64_t conn, void* arg);
  void* arg;
  void (*on_finished)(server_pt server, uint64_t origin, void* arg);
  uint32_t pos;
};

////////////////////////////////////////////////////////////////////////////////
// The server data object container
struct Server {
  // inherit the reactor (must be first in the struct, for pointer conformity).
  struct Reactor reactor;
  // a pointer to the server's settings object.
  struct ServerSettings* settings;
  // a pointer to the thread pool object (libasync).
  struct Async* async;
  // a mutex for server data integrity
  pthread_mutex_t lock;
  // maps each connection to it's protocol.
  struct ConnectionData* connections;
  /// a mutex for server data integrity
  pthread_mutex_t task_lock;
  /// fd task pool.
  struct FDTask* fd_task_pool;
  /// group task pool.
  struct GroupTask* group_task_pool;
  /// task(s) pool size
  size_t fd_task_pool_size;
  size_t group_task_pool_size;
  // socket capacity
  long capacity;
  /// the last timeout review
  time_t last_to;
  /// the server socket
  int srvfd;
  /// the original process pid
  pid_t root_pid;
  /// the flag that tells the server to stop
  volatile char run;
};

////////////////////////////////////////////////////////////////////////////////
// Macros for checking a connection ID is valid or retriving it's data

/** returns the connection's data */
#define connection_data(srv, conn) ((srv)->connections[(conn).data.fd])

/** valuates as FALSE (0) if the connection is valid and true if outdated */
#define validate_connection(srv, conn)                                \
  ((connection_data((srv), (conn)).counter != (conn).data.counter) || \
   (connection_data((srv), (conn)).counter &&                         \
    (connection_data((srv), (conn)).protocol == NULL)))

/** returns a value if the connection isn't valid */
#define is_open_connection_or_return(srv, conn, ret) \
  if (validate_connection((srv), (conn)))            \
    return (ret);

// object accessing helper macros

/** casts the server to the reactor pointer. */
#define _reactor_(server) ((struct Reactor*)(server))
/** casts the reactor pointer to a server pointer. */
#define _server_(reactor) ((server_pt)(reactor))
/** gets a specific connection data object from the server (reactor). */
#define _connection_(reactor, fd) (_server_(reactor)->connections[(fd)])
/** gets a specific protocol from a server's connection. */
#define _protocol_(reactor, fd) (_connection_((reactor), (fd))).protocol
/** gets a server connection's UUID. */
#define _fd2uuid_(reactor, sfd)                                            \
  (((union fd_id){.data.fd = (sfd),                                        \
                  .data.counter = _connection_((reactor), (sfd)).counter}) \
       .uuid)
/** creates a UUID from an fd and a counter object. */
#define _fd_counter_uuid_(sfd, count) \
  (((union fd_id){.data.fd = (sfd), .data.counter = count}).uuid)

////////////////////////////////////////////////////////////////////////////////
// Server API gateway

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Server API declerations

////////////////////////////////////////////////////////////////////////////////
// Server settings and objects

/** returns the originating process pid */
static pid_t root_pid(server_pt server);
/// allows direct access to the reactor object. use with care.
static struct Reactor* srv_reactor(server_pt server);
/// allows direct access to the server's original settings. use with care.
static struct ServerSettings* srv_settings(server_pt server);

/**
Returns the file descriptor belonging to the connection's UUID, if
available. Returns -1 if the connection is closed (we cannot use 0 since 0
is potentially a valid file descriptor). */
int to_fd(server_pt server, uint64_t connection_id);

////////////////////////////////////////////////////////////////////////////////
// Server actions

/** listens to a server with the following server settings (which MUST include
a default protocol). */
static int srv_listen(struct ServerSettings);
/// stops a specific server, closing any open connections.
static void srv_stop(server_pt server);
/// stops any and all server instances, closing any open connections.
static void srv_stop_all(void);

// helpers
static void srv_cycle_core(server_pt server);
static int set_to_busy(server_pt server, int fd);
static void async_on_data(struct ConnectionData* conn);
static void on_ready(struct Reactor* reactor, int fd);
static void on_shutdown(struct Reactor* reactor, int fd);
static void on_close(struct Reactor* reactor, int fd);
static void clear_conn_data(server_pt server, int fd);
static void accept_async(server_pt server);

// signal management
static void register_server(server_pt server);
static void on_signal(int sig);

////////////////////////////////////////////////////////////////////////////////
// Socket settings and data

/** Returns true if a specific connection's protected callback is running.

Protected callbacks include only the `on_message` callback and tasks forwarded
to the connection using the `td_task` or `each` functions.
*/
static uint8_t is_busy(server_pt server, union fd_id cuuid);
/** Returns true if the connection's UUID points to a valid connection (valid
 * meanning `on_close` wasn't called and processed just yet).
*/
static uint8_t is_open(server_pt server, union fd_id cuuid);
/// retrives the active protocol object for the requested file descriptor.
static struct Protocol* get_protocol(server_pt server, union fd_id cuuid);
/// sets the active protocol object for the requested file descriptor.
static int set_protocol(server_pt server,
                        union fd_id cuuid,
                        struct Protocol* new_protocol);
/** retrives an opaque pointer set by `set_udata` and associated with the
connection.

since no new connections are expected on fd == 0..2, it's possible to store
global data in these locations. */
static void* get_udata(server_pt server, union fd_id cuuid);
/** Sets the opaque pointer to be associated with the connection. returns the
old pointer, if any. */
static void* set_udata(server_pt server, union fd_id cuuid, void* udata);
/** Sets the timeout limit for the specified connectionl, in seconds, up to
255 seconds (the maximum allowed timeout count). */
static void set_timeout(server_pt server, union fd_id cuuid, uint8_t timeout);

////////////////////////////////////////////////////////////////////////////////
// Socket actions

/** Attaches an existing connection (fd) to the server's reactor and protocol
management system, so that the server can be used also to manage connection
based resources asynchronously (i.e. database resources etc').
*/
static int srv_attach(server_pt server, int sockfd, struct Protocol* protocol);
/** Closes the connection.

If any data is waiting to be written, close will
return immediately and the connection will only be closed once all the data
was sent. */
static void srv_close(server_pt server, union fd_id cuuid);
/** Hijack a socket (file descriptor) from the server, clearing up it's
resources. The control of hte socket is totally relinquished.

This method will block until all the data in the buffer is sent before
releasing control of the socket. */
static int srv_hijack(server_pt server, union fd_id cuuid);
/** Counts the number of connections for the specified protocol (NULL = all
protocols). */
static long srv_count(server_pt server, char* service);
/// "touches" a socket, reseting it's timeout counter.
static void srv_touch(server_pt server, union fd_id cuuid);

////////////////////////////////////////////////////////////////////////////////
// Read and Write

/**
Sets up the read/write hooks, allowing for transport layer extensions (i.e.
SSL/TLS) or monitoring extensions.
*/
void rw_hooks(
    server_pt srv,
    union fd_id cuuid,
    ssize_t (*reading_hook)(server_pt srv, int fd, void* buffer, size_t size),
    ssize_t (*writing_hook)(server_pt srv, int fd, void* data, size_t len));

/** Reads up to `max_len` of data from a socket. the data is stored in the
`buffer` and the number of bytes received is returned.

Returns -1 if an error was raised and the connection was closed.

Returns the number of bytes written to the buffer. Returns 0 if no data was
available.
*/
static ssize_t srv_read(server_pt srv,
                        union fd_id cuuid,
                        void* buffer,
                        size_t max_len);
/** Copies & writes data to the socket, managing an asyncronous buffer.

returns 0 on success. success means that the data is in a buffer waiting to
be written. If the socket is forced to close at this point, the buffer will be
destroyed (never sent).

On error, returns -1 otherwise returns the number of bytes in `len`.
*/
static ssize_t srv_write(server_pt server,
                         union fd_id cuuid,
                         void* data,
                         size_t len);
/** Writes data to the socket, moving the data's pointer directly to the buffer.

Once the data was written, `free` will be called to free the data's memory.

returns 0 on success. success means that the data is in a buffer waiting to
be written. If the socket is forced to close at this point, the buffer will be
destroyed (never sent).

On error, returns -1 otherwise returns the number of bytes in `len`.
*/
static ssize_t srv_write_move(server_pt server,
                              union fd_id cuuid,
                              void* data,
                              size_t len);
/** Copies & writes data to the socket, managing an asyncronous buffer.

Each call to a `write` function considers it's data atomic (a single package).

The `urgent` varient will send the data as soon as possible, without distrupting
any data packages (data written using `write` will not be interrupted in the
middle).

returns 0 on success. success means that the data is in a buffer waiting to
be written. If the socket is forced to close at this point, the buffer will be
destroyed (never sent).

On error, returns -1 otherwise returns the number of bytes in `len`.
*/
static ssize_t srv_write_urgent(server_pt server,
                                union fd_id cuuid,
                                void* data,
                                size_t len);
/** Writes data to the socket, moving the data's pointer directly to the buffer.

Once the data was written, `free` will be called to free the data's memory.

Each call to a `write` function considers it's data atomic (a single package).

The `urgent` varient will send the data as soon as possible, without distrupting
any data packages (data written using `write` will not be interrupted in the
middle).

returns 0 on success. success means that the data is in a buffer waiting to
be written. If the socket is forced to close at this point, the buffer will be
destroyed (never sent).

On error, returns -1 otherwise returns the number of bytes in `len`.
*/
static ssize_t srv_write_move_urgent(server_pt server,
                                     union fd_id cuuid,
                                     void* data,
                                     size_t len);
/**
Sends a whole file as if it were a single atomic packet.

Once the file was sent, the `FILE *` will be closed using `fclose`.

The file will be buffered to the socket chunk by chunk, so that memory
consumption is capped at ~ 64Kb.
*/
static ssize_t srv_sendfile(server_pt server, union fd_id cuuid, FILE* file);

////////////////////////////////////////////////////////////////////////////////
// Tasks + Async

/** Schedules a specific task to run asyncronously for each connection.
a NULL service identifier == all connections (all protocols). */
static int each(server_pt server,
                union fd_id origin,
                char* service,
                void (*task)(server_pt server, uint64_t target_fd, void* arg),
                void* arg,
                void (*on_finish)(server_pt server,
                                  uint64_t origin,
                                  void* arg));
/** Schedules a specific task to run for each connection. The tasks will be
 * performed sequencially, in a blocking manner. The method will only return
 * once all the tasks were completed. A NULL service identifier == all
 * connections (all protocols).
*/
static int each_block(server_pt server,
                      union fd_id origin,
                      char* service,
                      void (*task)(server_pt server,
                                   uint64_t target_fd,
                                   void* arg),
                      void* arg);
/** Schedules a specific task to run asyncronously for a specific connection.

returns -1 on failure, 0 on success (success being scheduling or performing
the task).
*/
static int fd_task(server_pt server,
                   union fd_id target,
                   void (*task)(server_pt server, uint64_t fd, void* arg),
                   void* arg,
                   void (*fallback)(server_pt server, uint64_t fd, void* arg));
/** Runs an asynchronous task, IF threading is enabled (set the
`threads` to 1 (the default) or greater).

If threading is disabled, the current thread will perform the task and return.

Returns -1 on error or 0
on succeess.
*/
static int run_async(server_pt self, void task(void*), void* arg);
/** Creates a system timer (at the cost of 1 file descriptor) and pushes the
timer to the reactor. The task will NOT repeat. Returns -1 on error or the
new file descriptor on succeess.

NOTICE: Do NOT create timers from within the on_close callback, as this might
block resources from being properly freed (if the timer and the on_close
object share the same fd number).
*/
static int run_after(server_pt self,
                     long milliseconds,
                     void task(void*),
                     void* arg);
/** Creates a system timer (at the cost of 1 file descriptor) and pushes the
timer to the reactor. The task will repeat `repetitions` times. if
`repetitions` is set to 0, task will repeat forever. Returns -1 on error
or the new file descriptor on succeess.

NOTICE: Do NOT create timers from within the on_close callback, as this might
block resources from being properly freed (if the timer and the on_close
object share the same fd number).
*/
static int run_every(server_pt self,
                     long milliseconds,
                     int repetitions,
                     void task(void*),
                     void* arg);

// Global Task helpers
static inline int perform_single_task(server_pt srv,
                                      uint64_t connection_id,
                                      void (*task)(server_pt server,
                                                   uint64_t connection_id,
                                                   void* arg),
                                      void* arg);
// FDTask helpers
struct FDTask* new_fd_task(server_pt srv);
void destroy_fd_task(server_pt srv, struct FDTask* task);
static void perform_fd_task(struct FDTask* task);

// GroupTask helpers
struct GroupTask* new_group_task(server_pt srv);
void destroy_group_task(server_pt srv, struct GroupTask* task);
static void perform_group_task(struct GroupTask* task);
void add_to_group_task(server_pt server, int fd, void* arg);

////////////////////////////////////////////////////////////////////////////////
// The actual Server API access setup
// The following allows access to helper functions and defines a namespace
// for the API in this library.
const struct Server__API___ Server = {
    /* accessor and server functions */
    .reactor = srv_reactor,
    .settings = srv_settings,
    .capacity = sock_max_capacity,
    .listen = srv_listen,
    .stop = srv_stop,
    .stop_all = srv_stop_all,
    .root_pid = root_pid,
    /* connection data functions */
    .is_busy = (uint8_t (*)(server_pt, uint64_t))is_busy,
    .is_open = (uint8_t (*)(server_pt, uint64_t))is_open,
    .get_protocol = (struct Protocol * (*)(server_pt, uint64_t))get_protocol,
    .set_protocol =
        (int (*)(server_pt, uint64_t, struct Protocol*))set_protocol,
    .get_udata = (void* (*)(server_pt, uint64_t))get_udata,
    .set_udata = (void* (*)(server_pt, uint64_t, void*))set_udata,
    .set_timeout = (void (*)(server_pt, uint64_t, uint8_t))set_timeout,
    /* connection managment functions */
    .attach = srv_attach,
    .close = (void (*)(server_pt, uint64_t))srv_close,
    .hijack = (int (*)(server_pt, uint64_t))srv_hijack,
    .count = srv_count,
    /* connection activity and read/write functions */
    .touch = (void (*)(server_pt, uint64_t))srv_touch,
    .read = (ssize_t (*)(server_pt, uint64_t, void*, size_t))srv_read,
    .write = (ssize_t (*)(server_pt, uint64_t, void*, size_t))srv_write,
    .write_move =
        (ssize_t (*)(server_pt, uint64_t, void*, size_t))srv_write_move,
    .write_urgent =
        (ssize_t (*)(server_pt, uint64_t, void*, size_t))srv_write_urgent,
    .write_move_urgent = (ssize_t (*)(server_pt,
                                      uint64_t,
                                      void*,
                                      size_t))srv_write_move_urgent,  //
    .sendfile = (ssize_t (*)(server_pt, uint64_t, FILE*))srv_sendfile,
    /* connection tasks functions */
    .each = (int (*)(server_pt,
                     uint64_t,
                     char*,
                     void (*)(server_pt, uint64_t, void*),
                     void*,
                     void (*)(server_pt, uint64_t, void*)))each,
    .each_block = (int (*)(server_pt,
                           uint64_t,
                           char*,
                           void (*)(server_pt, uint64_t, void*),
                           void*))each_block,
    .fd_task = (int (*)(server_pt,
                        uint64_t,
                        void (*)(server_pt, uint64_t, void*),
                        void*,
                        void (*)(server_pt, uint64_t, void*)))fd_task,
    /* global task functions */
    .run_async = run_async,
    .run_after = run_after,
    .run_every = run_every,
};

////////////////////////////////////////////////////////////////////////////////
// timer protocol

// service name
static char* timer_protocol_name = "timer";
// a timer protocol, will be allocated for each timer.
struct TimerProtocol {
  // must be first for pointer compatability
  struct Protocol parent;
  void (*task)(void*);
  void* arg;
  // how many repeats?
  int repeat;
};

// the timer's on_data callback
static void tp_perform_on_data(server_pt self, uint64_t conn) {
  struct TimerProtocol* timer =
      (struct TimerProtocol*)Server.get_protocol(self, conn);
  if (timer) {
    // set local data copy, to avoid race contitions related to `free`.
    void (*task)(void*) = (void (*)(void*))timer->task;
    void* arg = timer->arg;
    // perform the task
    if (task)
      task(arg);
    // reset the timer
    reactor_reset_timer(server_uuid_to_fd(conn));
    // close the file descriptor
    if (timer->repeat < 0)
      return;
    if (timer->repeat == 0) {
      reactor_close((struct Reactor*)self, server_uuid_to_fd(conn));
      return;
    }
    timer->repeat--;
  }
}

// the timer's on_close (cleanup)
static void tp_perform_on_close(server_pt self, uint64_t conn) {
  // free the protocol object when it was "aborted" using `close`.
  struct TimerProtocol* timer =
      (struct TimerProtocol*)Server.get_protocol(self, conn);
  if (timer)
    free(timer);
}

// creates a new TimeProtocol object.
// use: TimerProtocol(task, arg, rep)
static struct TimerProtocol* TimerProtocol(void* task,
                                           void* arg,
                                           int repetitions) {
  struct TimerProtocol* tp = malloc(sizeof(struct TimerProtocol));

  *tp = (struct TimerProtocol){.parent.on_data = tp_perform_on_data,
                               .parent.on_close = tp_perform_on_close,
                               .parent.service = timer_protocol_name,
                               .task = task,
                               .arg = arg,
                               .repeat = repetitions - 1};
  return tp;
}

////////////////////////////////////////////////////////////////////////////////
// Server settings and objects

/** returns the originating process pid */
static pid_t root_pid(server_pt server) {
  return server->root_pid;
}
/// allows direct access to the reactor object. use with care.
static struct Reactor* srv_reactor(server_pt server) {
  return (struct Reactor*)server;
}
/// allows direct access to the server's original settings. use with care.
static struct ServerSettings* srv_settings(server_pt server) {
  return server->settings;
}

////////////////////////////////////////////////////////////////////////////////
// Socket settings and data

/** Returns true if a specific connection's protected callback is running AND
the connection is still active

Protected callbacks include only the `on_message` callback and tasks forwarded
to the connection using the `td_task` or `each` functions.
*/
static uint8_t is_busy(server_pt server, union fd_id cuuid) {
  return server->connections[cuuid.data.fd].counter == cuuid.data.counter &&
         server->connections[cuuid.data.fd].busy;
}
/** Returns true if a specific connection is still valid (on_close hadn't
completed) */
static uint8_t is_open(server_pt server, union fd_id cuuid) {
  return !validate_connection(server, cuuid);
}

/// retrives the active protocol object for the requested file descriptor.
static struct Protocol* get_protocol(server_pt server, union fd_id conn) {
  is_open_connection_or_return(server, conn, NULL);
  return server->connections[conn.data.fd].protocol;
}
/// sets the active protocol object for the requested file descriptor.
static int set_protocol(server_pt server,
                        union fd_id conn,
                        struct Protocol* new_protocol) {
  is_open_connection_or_return(server, conn, -1);
  // on_close and set_protocol should never conflict.
  // we should also prevent the same thread from deadlocking
  // (i.e., when on_close tries to update the protocol)
  if (pthread_mutex_trylock(&(server->lock)))
    return -1;
  // review the connection's validity again (in proteceted state)
  if (validate_connection(server, conn)) {
    pthread_mutex_unlock(&(server->lock));
    // fprintf(stderr,
    //         "ERROR: Cannot set a protocol for a disconnected socket.\n");
    return -1;
  }
  // set the new protocol
  server->connections[conn.data.fd].protocol = new_protocol;
  // release the lock
  pthread_mutex_unlock(&(server->lock));
  // return 0 (no error)
  return 0;
}
/** retrives an opaque pointer set by `set_udata` and associated with the
connection.

since no new connections are expected on fd == 0..2, it's possible to store
global data in these locations. */
static void* get_udata(server_pt server, union fd_id conn) {
  if (validate_connection(server, conn))
    return NULL;
  return server->connections[conn.data.fd].udata;
}
/** Sets the opaque pointer to be associated with the connection. returns the
old pointer, if any.

Returns NULL both on error (i.e. old connection ID) and
sucess (no previous value). Check that the value was set using `get_udata`.
*/
static void* set_udata(server_pt server, union fd_id conn, void* udata) {
  if (validate_connection(server, conn))
    return NULL;
  pthread_mutex_lock(&(server->lock));
  if (validate_connection(server, conn)) {
    pthread_mutex_unlock(&(server->lock));
    return NULL;
  }
  void* old = connection_data(server, conn).udata;
  connection_data(server, conn).udata = udata;
  pthread_mutex_unlock(&(server->lock));
  return old;
}
/** Sets the timeout limit for the specified connectionl, in seconds, up to
255 seconds (the maximum allowed timeout count). */
static void set_timeout(server_pt server, union fd_id conn, uint8_t timeout) {
  connection_data(server, conn).tout = timeout;
}

////////////////////////////////////////////////////////////////////////////////
// Server actions & Core

// clears a connection's data
static void clear_conn_data(server_pt server, int fd) {
  server->connections[fd].counter++;
  server->connections[fd].idle = 0;
  server->connections[fd].protocol = NULL;
  server->connections[fd].tout = 0;
  server->connections[fd].udata = NULL;
  server->connections[fd].busy = 0;
}
// on_ready, handles async buffer sends
static void on_ready(struct Reactor* reactor, int fd) {
  _server_(reactor)->connections[fd].idle = 0;
  if (_protocol_(reactor, fd) && _protocol_(reactor, fd)->on_ready)
    _protocol_(reactor, fd)
        ->on_ready(_server_(reactor), _fd2uuid_(reactor, fd));
}

/// called for any open file descriptors when the reactor is shutting down.
static void on_shutdown(struct Reactor* reactor, int fd) {
  // call the callback for the mentioned active(?) connection.
  if (_protocol_(reactor, fd) && _protocol_(reactor, fd)->on_shutdown)
    _protocol_(reactor, fd)
        ->on_shutdown(_server_(reactor), _fd2uuid_(reactor, fd));
}

// called when a file descriptor was closed (either locally or by the other
// party, when it's a socket or a pipe).
static void on_close(struct Reactor* reactor, int fd) {
  if (!_protocol_(reactor, fd))
    return;
  // file descriptors can be added from external threads (i.e. creating timers),
  // this could cause race conditions and data corruption,
  // (i.e., when on_close is releasing memory).
  int lck = 0;
  // this should preven the same thread from calling on_close recursively
  // (i.e., when using the Server.attach)
  if ((lck = pthread_mutex_trylock(&(_server_(reactor)->lock)))) {
    if (lck != EAGAIN)
      return;
    pthread_mutex_lock(&(_server_(reactor)->lock));
  }
  if (_protocol_(reactor, fd)) {
    if (_protocol_(reactor, fd)->on_close)
      _protocol_(reactor, fd)
          ->on_close(_server_(reactor), _fd2uuid_(reactor, fd));
    clear_conn_data(_server_(reactor), fd);
  }
  pthread_mutex_unlock(&(_server_(reactor)->lock));
}

// The busy flag is used to make sure that a single connection doesn't perform
// two "critical" tasks at the same time. Critical tasks are defined as the
// `on_message` callback and any user task requested by `each` or `fd_task`.
static int set_to_busy(server_pt server, int sockfd) {
  static pthread_mutex_t locker = PTHREAD_MUTEX_INITIALIZER;

  if (!_protocol_(server, sockfd))
    return 0;
  pthread_mutex_lock(&locker);
  if (_connection_(server, sockfd).busy == 1) {
    pthread_mutex_unlock(&locker);
    return 0;
  }
  _connection_(server, sockfd).busy = 1;
  pthread_mutex_unlock(&locker);
  return 1;
}

// accepts new connections
static void accept_async(server_pt server) {
  int client = 1;
  while (1) {
    client = sock_accept(NULL, server->srvfd);
    if (client <= 0)
      return;
    // handle server overload
    if (client >= _reactor_(server)->maxfd - 8) {
      if (server->settings->busy_msg)
        if (write(client, server->settings->busy_msg,
                  strlen(server->settings->busy_msg)) < 0)
          ;
      close(client);
      continue;
    }
    // attach the new client (performs on_close if needed)
    srv_attach(server, client, server->settings->protocol);
  }
}
// makes sure that the on_data callback isn't overlapping a previous on_data
static void async_on_data(struct ConnectionData* conn) {
  // compute sockfd by comparing the distance between the original pointer and
  // the passed pointer's address.
  if (!conn || !conn->protocol || !conn->protocol->on_data)
    return;
  // if we get the handle, perform the task
  if (set_to_busy(conn->srv, conn->fd)) {
    conn->idle = 0;
    conn->protocol->on_data(conn->srv,
                            _fd_counter_uuid_(conn->fd, conn->counter));
    // release the handle
    conn->busy = 0;
    return;
  }
  // we didn't get the handle, reschedule - but only if the connection is still
  // open.
  if (conn->protocol)
    Async.run(conn->srv->async, (void (*)(void*))async_on_data, conn);
}

static void on_data(struct Reactor* reactor, int fd) {
  // static socklen_t cl_addrlen = 0;
  if (fd == _server_(reactor)->srvfd) {
    // listening socket. accept connections.
    Async.run(_server_(reactor)->async, (void (*)(void*))accept_async, reactor);
  } else if (_protocol_(reactor, fd) && _protocol_(reactor, fd)->on_data) {
    _connection_(reactor, fd).idle = 0;
    // clients, forward on.
    Async.run(_server_(reactor)->async, (void (*)(void*))async_on_data,
              &_connection_(reactor, fd));
  }
}

// calls the reactor's core and checks for timeouts.
// schedules it's own execution when done.
// shouldn't be called by more then a single thread at a time (NOT thread safe)
static void srv_cycle_core(server_pt server) {
  static size_t idle_performed = 0;
  int delta;  // we also use this for other things, but it's the TOut delta
  // review reactor events
  delta = reactor_review(_reactor_(server));
  if (delta < 0) {
    srv_stop(server);
    return;
  } else if (delta == 0) {
    if (server->settings->on_idle && idle_performed == 0)
      server->settings->on_idle(server);
    idle_performed = 1;
  } else
    idle_performed = 0;
  // timeout + local close management
  if (server->last_to != _reactor_(server)->last_tick) {
    // We use the delta with fuzzy logic (only after the first second)
    int delta = _reactor_(server)->last_tick - server->last_to;
    for (long i = 3; i <= _reactor_(server)->maxfd; i++) {
      if (_protocol_(server, i) && fcntl(i, F_GETFL) < 0 && errno == EBADF) {
        reactor_close(_reactor_(server), i);
      }
      if (_connection_(server, i).tout) {
        if (_connection_(server, i).tout > _connection_(server, i).idle)
          _connection_(server, i).idle +=
              _connection_(server, i).idle ? delta : 1;
        else {
          if (_protocol_(server, i) && _protocol_(server, i)->ping)
            _protocol_(server, i)->ping(server, i);
          else if (!_connection_(server, i).busy ||
                   _connection_(server, i).idle == 255)
            reactor_close(_reactor_(server), i);
        }
      }
    }
    // ready for next call.
    server->last_to = _reactor_(server)->last_tick;
    // double en = (float)clock()/CLOCKS_PER_SEC;
    // printf("timeout review took %f milliseconds\n", (en-st)*1000);
  }
  if (server->run &&
      Async.run(server->async, (void (*)(void*))srv_cycle_core, server)) {
    perror(
        "FATAL ERROR:"
        "couldn't schedule the server's reactor in the task queue");
    exit(1);
  }
}

/** listens to a server with the following server settings (which MUST include
a default protocol). */
static int srv_listen(struct ServerSettings settings) {
  // review the settings, setup defaults when something's missing
  if (!settings.protocol) {
    fprintf(stderr,
            "Server err: (protocol == NULL) Running a server requires "
            "a protocol for new connections.\n");
    return -1;
  }
  if (!settings.port)
    settings.port = "3000";
  if (!settings.timeout)
    settings.timeout = 5;
  if (!settings.threads || settings.threads <= 0)
    settings.threads = 1;
  if (!settings.processes || settings.processes <= 0)
    settings.processes = 1;

  // V.3 Avoids using the Stack, allowing userspace to use the memory
  long capacity = sock_max_capacity();
  struct ConnectionData* connections = calloc(sizeof(*connections), capacity);
  if (!connections)
    return -1;

  // populate the Server structure with the data
  struct Server srv = {
      // initialize the server object
      .settings = &settings,  // store a pointer to the settings
      .last_to = 0,           // last timeout review
      .capacity = capacity,   // the server's capacity
      .connections = connections,
      .fd_task_pool = NULL,
      .fd_task_pool_size = 0,
      .group_task_pool = NULL,
      .group_task_pool_size = 0,
      .reactor.maxfd = capacity - 1,
      .reactor.on_data = on_data,
      .reactor.on_ready = on_ready,
      .reactor.on_shutdown = on_shutdown,
      .reactor.on_close = on_close,
  };
  // initialize the server data mutex
  if (pthread_mutex_init(&srv.lock, NULL)) {
    free(connections);
    return -1;
  }
  // initialize the server task pool mutex
  if (pthread_mutex_init(&srv.task_lock, NULL)) {
    pthread_mutex_destroy(&srv.lock);
    free(connections);
    return -1;
  }

  // bind the server's socket - if relevent
  int srvfd = 0;
  if (settings.port != NULL) {
    srvfd = sock_listen(settings.address, settings.port);
    // if we didn't get a socket, quit now.
    if (srvfd < 0) {
      pthread_mutex_destroy(&srv.task_lock);
      pthread_mutex_destroy(&srv.lock);
      free(connections);
      return -1;
    }
    srv.srvfd = srvfd;
  }

  // initialize connection data...
  init_socklib(capacity);
  for (int i = 0; i < capacity; i++) {
    connections[i].srv = &srv;
    connections[i].fd = i;
  }

  // register signals - do this before concurrency, so that they are inherited.
  struct sigaction old_term, old_int, old_pipe, new_int, new_pipe;
  sigemptyset(&new_int.sa_mask);
  sigemptyset(&new_pipe.sa_mask);
  new_pipe.sa_flags = new_int.sa_flags = 0;
  new_pipe.sa_handler = SIG_IGN;
  new_int.sa_handler = on_signal;
  sigaction(SIGINT, &new_int, &old_int);
  sigaction(SIGTERM, &new_int, &old_term);
  sigaction(SIGPIPE, &new_pipe, &old_pipe);

  // setup concurrency
  srv.root_pid = getpid();
  pid_t pids[settings.processes > 0 ? settings.processes : 0];
  if (settings.processes > 1) {
    pids[0] = 0;
    for (int i = 1; i < settings.processes; i++) {
      if (getpid() == srv.root_pid)
        pids[i] = fork();
    }
  }
  // once we forked, we can initiate a thread pool for each process
  srv.async = Async.create(settings.threads);
  if (srv.async <= 0) {
    if (srvfd)
      close(srvfd);
    pthread_mutex_destroy(&srv.task_lock);
    pthread_mutex_destroy(&srv.lock);
    free(connections);
    return -1;
  }

  // register server for signals
  register_server(&srv);

  // let'm know...
  if (srvfd)
    printf(
        "(pid %d x %d threads) Listening on port %s (max sockets: %lu, ~%d "
        "used)\n",
        getpid(), srv.settings->threads, srv.settings->port, srv.capacity,
        srv.srvfd + 2);
  else
    printf(
        "(pid %d x %d threads) Started a non-listening network service"
        "(max sockets: %lu ~ at least 6 are in system use)\n",
        getpid(), srv.settings->threads, srv.capacity);

  // initialize reactor
  reactor_init(&srv.reactor);
  // bind server data to reactor loop
  if (srvfd)
    reactor_add(&srv.reactor, srv.srvfd);

  // call the on_init callback
  if (settings.on_init) {
    settings.on_init(&srv);
  }

  // initiate the core's cycle
  srv.run = 1;
  Async.run(srv.async, (void (*)(void*))srv_cycle_core, &srv);
  Async.wait(srv.async);
  // cleanup
  reactor_stop(&srv.reactor);

  if (settings.processes > 1 && getpid() == srv.root_pid) {
    int sts;
    for (int i = 1; i < settings.processes; i++) {
      // printf("sending signal to pid %d\n", pids[i]);
      kill(pids[i], SIGINT);
    }
    for (int i = 1; i < settings.processes; i++) {
      sts = 0;
      // printf("waiting for pid %d\n", pids[i]);
      if (waitpid(pids[i], &sts, 0) != pids[i])
        perror("waiting for child process had failed");
    }
  }

  if (settings.on_finish)
    settings.on_finish(&srv);
  printf("(pid %d) Stopped listening on port %s\n", getpid(), settings.port);
  if (getpid() != srv.root_pid) {
    fflush(NULL);
    exit(0);
  }
  // restore signal state
  sigaction(SIGINT, &old_int, NULL);
  sigaction(SIGTERM, &old_term, NULL);
  sigaction(SIGPIPE, &old_pipe, NULL);

  // destroy the task pools
  destroy_fd_task(&srv, NULL);
  destroy_group_task(&srv, NULL);

  // destroy the mutexes
  pthread_mutex_destroy(&srv.lock);
  pthread_mutex_destroy(&srv.task_lock);

  // free the connection data array
  free(connections);

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Socket actions

/** Attaches an existing connection (fd) to the server's reactor and protocol
management system, so that the server can be used also to manage connection
based resources asynchronously (i.e. database resources etc').
*/
static int srv_attach_core(server_pt server,
                           int sockfd,
                           struct Protocol* protocol,
                           long milliseconds) {
  if (sockfd >= server->capacity)
    return -1;
  if (_protocol_(server, sockfd)) {
    on_close((struct Reactor*)server, sockfd);
  }
  // udata can be set and received for closed connections
  clear_conn_data(server, sockfd);
  // setup protocol
  _protocol_(server, sockfd) = protocol;  // set_protocol() would cost more
  // setup timeouts
  _connection_(server, sockfd).tout = server->settings->timeout;
  _connection_(server, sockfd).idle = 0;
  // register the client - start it off as busy, protocol still initializing
  // we don't need the mutex, because it's all fresh
  _connection_(server, sockfd).busy = 1;
  // attach the socket to the reactor
  if (((milliseconds > 0) && (reactor_add_timer((struct Reactor*)server, sockfd,
                                                milliseconds) < 0)) ||
      ((milliseconds <= 0) &&
       (sock_attach((struct Reactor*)server, sockfd) < 0))) {
    clear_conn_data(server, sockfd);
    return -1;
  }
  // call on_open
  if (protocol->on_open)
    protocol->on_open(server, _fd2uuid_(server, sockfd));
  _connection_(server, sockfd).busy = 0;
  return 0;
}
static int srv_attach(server_pt server, int sockfd, struct Protocol* protocol) {
  return srv_attach_core(server, sockfd, protocol, 0);
}

/** Closes the connection.

If any data is waiting to be written, close will
return immediately and the connection will only be closed once all the data
was sent. */
static void srv_close(server_pt server, union fd_id conn) {
  if (validate_connection(server, conn) || !_protocol_(server, conn.data.fd))
    return;
  sock_close((struct Reactor*)server, conn.data.fd);
}
/** Hijack a socket (file descriptor) from the server, clearing up it's
resources. The control of the socket is totally relinquished.

This method will block until all the data in the buffer is sent before
releasing control of the socket. */
static int srv_hijack(server_pt server, union fd_id conn) {
  is_open_connection_or_return(server, conn, -1);
  reactor_remove((struct Reactor*)server, conn.data.fd);
  while (sock_flush(conn.data.fd) > 0)
    ;
  clear_conn_data(server, conn.data.fd);
  return 0;
}
/** Counts the number of connections for the specified protocol (NULL = all
protocols). */
static long srv_count(server_pt server, char* service) {
  int c = 0;
  if (service) {
    for (int i = 0; i < server->capacity; i++) {
      if (_protocol_(server, i) &&
          (_protocol_(server, i)->service == service ||
           !strcmp((_protocol_(server, i)->service), service)))
        c++;
    }
  } else {
    for (int i = 0; i < server->capacity; i++) {
      if (_protocol_(server, i) &&
          _protocol_(server, i)->service != timer_protocol_name)
        c++;
    }
  }
  return c;
}
/// "touches" a socket, reseting it's timeout counter.
static void srv_touch(server_pt server, union fd_id conn) {
  if (validate_connection(server, conn))
    return;
  _connection_(server, conn.data.fd).idle = 0;
}

////////////////////////////////////////////////////////////////////////////////
// Read and Write

/** Reads up to `max_len` of data from a socket. the data is stored in the
`buffer` and the number of bytes received is returned.

Returns -1 if an error was raised and the connection was closed.

Returns the number of bytes written to the buffer. Returns 0 if no data was
available.
*/
static ssize_t srv_read(server_pt srv,
                        union fd_id conn,
                        void* buffer,
                        size_t max_len) {
  ssize_t read = 0;
  if ((read = recv(conn.data.fd, buffer, max_len, 0)) > 0) {
    // reset timeout
    _connection_(srv, conn.data.fd).idle = 0;
    // return read count
    return read;
  } else {
    if (read && (errno & (EWOULDBLOCK | EAGAIN)))
      return 0;
  }
  return read;
}
/** Copies & writes data to the socket, managing an asyncronous buffer.

On error, returns -1 otherwise returns the number of bytes already sent.
*/
static ssize_t srv_write(server_pt server,
                         union fd_id conn,
                         void* data,
                         size_t len) {
  is_open_connection_or_return(server, conn, -1);
  // reset timeout
  _connection_(server, conn.data.fd).idle = 0;
  // send data
  if (sock_write2(.fd = conn.data.fd, .buffer = data, .length = len))
    return -1;
  return 0;
}
/** Writes data to the socket, moving the data's pointer directly to the buffer.

Once the data was written, `free` will be called to free the data's memory.

On error, returns -1 otherwise returns the number of bytes already sent.
*/
static ssize_t srv_write_move(server_pt server,
                              union fd_id conn,
                              void* data,
                              size_t len) {
  is_open_connection_or_return(server, conn, -1);
  // reset timeout
  _connection_(server, conn.data.fd).idle = 0;
  // send data
  if (sock_write2(.fd = conn.data.fd, .buffer = data, .length = len, .move = 1))
    return -1;
  return 0;
}
/** Copies & writes data to the socket, managing an asyncronous buffer.

Each call to a `write` function considers it's data atomic (a single package).

The `urgent` varient will send the data as soon as possible, without distrupting
any data packages (data written using `write` will not be interrupted in the
middle).

On error, returns -1 otherwise returns the number of bytes already sent.
*/
static ssize_t srv_write_urgent(server_pt server,
                                union fd_id conn,
                                void* data,
                                size_t len) {
  is_open_connection_or_return(server, conn, -1);
  // reset timeout
  _connection_(server, conn.data.fd).idle = 0;
  // send data
  // send data
  if (sock_write2(.fd = conn.data.fd, .buffer = data, .length = len,
                  .urgent = 1))
    return -1;
  return 0;
}
/** Writes data to the socket, moving the data's pointer directly to the buffer.

Once the data was written, `free` will be called to free the data's memory.

Each call to a `write` function considers it's data atomic (a single package).

The `urgent` varient will send the data as soon as possible, without distrupting
any data packages (data written using `write` will not be interrupted in the
middle).

On error, returns -1 otherwise returns the number of bytes already sent.
*/
static ssize_t srv_write_move_urgent(server_pt server,
                                     union fd_id conn,
                                     void* data,
                                     size_t len) {
  is_open_connection_or_return(server, conn, -1);
  // reset timeout
  _connection_(server, conn.data.fd).idle = 0;
  // send data
  // send data
  if (sock_write2(.fd = conn.data.fd, .buffer = data, .length = len, .move = 1,
                  .urgent = 1))
    return -1;
  return 0;
}
/**
Sends a whole file as if it were a single atomic packet.

Once the file was sent, the `FILE *` will be closed using `fclose`.

The file will be buffered to the socket chunk by chunk, so that memory
consumption is capped at ~ 64Kb.
*/
static ssize_t srv_sendfile(server_pt server, union fd_id conn, FILE* file) {
  is_open_connection_or_return(server, conn, -1);
  // reset timeout
  _connection_(server, conn.data.fd).idle = 0;
  // send data
  if (sock_write2(.fd = conn.data.fd, .buffer = file, .file = 1))
    return -1;
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Tasks + Async

// macro for each task review

#define check_if_connection_fits_service(server, i, service_name)         \
  ((service_name && server->connections[i].protocol &&                    \
    server->connections[i].protocol->service &&                           \
    (server->connections[i].protocol->service == service_name ||          \
     !strcmp(server->connections[i].protocol->service, service_name))) || \
   (service_name == NULL && server->connections[i].protocol &&            \
    server->connections[i].protocol->service != timer_protocol_name))

// Global Task helpers
static inline int perform_single_task(server_pt srv,
                                      uint64_t connection_id,
                                      void (*task)(server_pt server,
                                                   uint64_t connection_id,
                                                   void* arg),
                                      void* arg) {
  is_open_connection_or_return(srv, ((union fd_id)connection_id), 0);
  if (set_to_busy(srv, server_uuid_to_fd(connection_id))) {
    // perform the task
    task(srv, connection_id, arg);
    // release the busy flag
    _connection_(srv, server_uuid_to_fd(connection_id)).busy = 0;
    // return completion flag;
    return 1;
  }
  return 0;
}

// FDTask helpers
struct FDTask* new_fd_task(server_pt srv) {
  struct FDTask* ret = NULL;
  pthread_mutex_lock(&srv->task_lock);
  if (srv->fd_task_pool) {
    ret = srv->fd_task_pool;
    srv->fd_task_pool = srv->fd_task_pool->next;
    --srv->fd_task_pool_size;
    pthread_mutex_unlock(&srv->task_lock);
    return ret;
  }
  pthread_mutex_unlock(&srv->task_lock);
  ret = malloc(sizeof(*ret));
  return ret;
}
void destroy_fd_task(server_pt srv, struct FDTask* task) {
  if (task == NULL) {
    pthread_mutex_lock(&srv->task_lock);
    struct FDTask* tmp;
    srv->fd_task_pool_size = 0;
    while ((tmp = srv->fd_task_pool)) {
      srv->fd_task_pool = srv->fd_task_pool->next;
      free(tmp);
    }
    pthread_mutex_unlock(&srv->task_lock);
    return;
  }
  pthread_mutex_lock(&srv->task_lock);
  if (srv->fd_task_pool_size >= 128) {
    pthread_mutex_unlock(&srv->task_lock);
    free(task);
    return;
  }
  task->next = srv->fd_task_pool;
  srv->fd_task_pool = task;
  ++srv->fd_task_pool_size;
  pthread_mutex_unlock(&srv->task_lock);
}
static void perform_fd_task(struct FDTask* task) {
  // is it okay to perform the task?
  if (validate_connection(task->server, task->conn_id)) {
    if (task->fallback)  // check for fallback, call if requested
      task->fallback(task->server, task->conn_id.uuid, task->arg);
    destroy_fd_task(task->server, task);
    return;
  }

  if (perform_single_task(task->server, task->conn_id.uuid, task->task,
                          task->arg)) {
    // free the memory
    destroy_fd_task(task->server, task);
  } else
    Async.run(task->server->async, (void (*)(void*))perform_fd_task, task);
}

// GroupTask helpers
struct GroupTask* new_group_task(server_pt srv) {
  struct GroupTask* ret = NULL;
  pthread_mutex_lock(&srv->task_lock);
  if (srv->group_task_pool) {
    ret = srv->group_task_pool;
    srv->group_task_pool = srv->group_task_pool->next;
    --(srv->group_task_pool_size);
    pthread_mutex_unlock(&srv->task_lock);
    memset(ret, 0, sizeof(*ret));
    return ret;
  }
  pthread_mutex_unlock(&srv->task_lock);
  ret = calloc(sizeof(*ret), 1);
  return ret;
}
void destroy_group_task(server_pt srv, struct GroupTask* task) {
  if (task == NULL) {
    pthread_mutex_lock(&srv->task_lock);
    struct GroupTask* tmp;
    srv->group_task_pool_size = 0;
    while ((tmp = srv->group_task_pool)) {
      srv->group_task_pool = srv->group_task_pool->next;
      free(tmp);
    }
    pthread_mutex_unlock(&srv->task_lock);
    return;
  }
  pthread_mutex_lock(&srv->task_lock);
  if (srv->group_task_pool_size >= 64) {
    pthread_mutex_unlock(&srv->task_lock);
    free(task);
    return;
  }
  task->next = srv->group_task_pool;
  srv->group_task_pool = task;
  ++(srv->group_task_pool_size);
  pthread_mutex_unlock(&srv->task_lock);
}

static void perform_group_task(struct GroupTask* task) {
  // the maximum number of connections to review
  long fd_max = task->server->capacity;
  // continue cycle from the point where it was left off
  while (task->pos < fd_max) {
    // the byte contains at least one bit marker for a task related fd

    // is it okay to perform the task?
    if (task->pos != server_uuid_to_fd(task->conn_origin) &&
        check_if_connection_fits_service(task->server, task->pos,
                                         task->service)) {
      if (perform_single_task(task->server, _fd2uuid_(task->server, task->pos),
                              task->task, task->arg))
        task->pos++;
      goto rescedule;
    } else  // closed/same connection, ignore it.
      task->pos++;
  }
  // clear the task away...
  if (task->on_finished) {
    fd_task(task->server, (union fd_id)task->conn_origin, task->on_finished,
            task->arg, task->on_finished);
  }
  destroy_group_task(task->server, task);
  return;
rescedule:
  Async.run(task->server->async, (void (*)(void*)) & perform_group_task, task);
  return;
}

/** Schedules a specific task to run asyncronously for each connection.
a NULL service identifier == all connections (all protocols). */
static int each(server_pt server,
                union fd_id origin_fd,
                char* service,
                void (*task)(server_pt server, uint64_t target_fd, void* arg),
                void* arg,
                void (*on_finish)(server_pt server,
                                  uint64_t origin_fd,
                                  void* arg)) {
  struct GroupTask* gtask = new_group_task(server);
  if (!gtask || !task)
    return -1;
  gtask->arg = arg;
  gtask->task = task;
  gtask->server = server;
  gtask->on_finished = on_finish;
  gtask->conn_origin = origin_fd.uuid;
  gtask->pos = 0;
  gtask->service = service;
  Async.run(server->async, (void (*)(void*)) & perform_group_task, gtask);
  return 0;
}
/** Schedules a specific task to run for each connection. The tasks will be
 * performed sequencially, in a blocking manner. The method will only return
 * once all the tasks were completed. A NULL service identifier == all
 * connections (all protocols).
*/
static int each_block(server_pt server,
                      union fd_id origin_fd,
                      char* service,
                      void (*task)(server_pt server, uint64_t fd, void* arg),
                      void* arg) {
  int c = 0;
  for (int i = 0; i < server->capacity; i++) {
    if (i != origin_fd.data.fd &&
        check_if_connection_fits_service(server, i, service)) {
      task(server, i, arg);
      ++c;
    }
  }
  return c;
}
/** Schedules a specific task to run asyncronously for a specific connection.

returns -1 on failure, 0 on success (success being scheduling or performing
the task).
*/
static int fd_task(server_pt server,
                   union fd_id conn,
                   void (*task)(server_pt server, uint64_t conn, void* arg),
                   void* arg,
                   void (*fallback)(server_pt server,
                                    uint64_t conn,
                                    void* arg)) {
  is_open_connection_or_return(server, conn, -1);
  struct FDTask* msg = new_fd_task(server);
  if (!msg)
    return -1;
  *msg = (struct FDTask){.conn_id = conn,
                         .server = server,
                         .task = task,
                         .arg = arg,
                         .fallback = fallback};
  Async.run(server->async, (void (*)(void*)) & perform_fd_task, msg);
  return 0;
}
/** Runs an asynchronous task, IF threading is enabled (set the
`threads` to 1 (the default) or greater).

If threading is disabled, the current thread will perform the task and return.

Returns -1 on error or 0
on succeess.
*/
static int run_async(server_pt self, void task(void*), void* arg) {
  return Async.run(self->async, task, arg);
}
/** Creates a system timer (at the cost of 1 file descriptor) and pushes the
timer to the reactor. The task will NOT repeat. Returns -1 on error or the
new file descriptor on succeess.

NOTICE: Do NOT create timers from within the on_close callback, as this might
block resources from being properly freed (if the timer and the on_close
object share the same fd number).
*/
static int run_after(server_pt self,
                     long milliseconds,
                     void task(void*),
                     void* arg) {
  return run_every(self, milliseconds, 1, task, arg);
}
/** Creates a system timer (at the cost of 1 file descriptor) and pushes the
timer to the reactor. The task will repeat `repetitions` times. if
`repetitions` is set to 0, task will repeat forever. Returns -1 on error
or the new file descriptor on succeess.

NOTICE: Do NOT create timers from within the on_close callback, as this might
block resources from being properly freed (if the timer and the on_close
object share the same fd number).
*/
static int run_every(server_pt self,
                     long milliseconds,
                     int repetitions,
                     void task(void*),
                     void* arg) {
  int tfd = reactor_make_timer();
  if (tfd <= 0)
    return -1;
  struct Protocol* timer_protocol =
      (struct Protocol*)TimerProtocol(task, arg, repetitions);
  if (srv_attach_core(self, tfd, timer_protocol, milliseconds)) {
    free(timer_protocol);
    return -1;
  }
  // remove the default timeout (timers shouldn't timeout)
  _connection_(self, tfd).tout = 0;
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Server Registry
// (stopping and signal managment)

// types and global data
struct ServerSet {
  struct ServerSet* next;
  server_pt server;
};
static struct ServerSet* global_servers_set = NULL;
static pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

// register a server
static void register_server(server_pt srv) {
  pthread_mutex_lock(&global_lock);
  struct ServerSet* n_reg = malloc(sizeof(struct ServerSet));
  n_reg->server = srv;
  n_reg->next = global_servers_set;
  global_servers_set = n_reg;
  pthread_mutex_unlock(&global_lock);
}
// stop a server (+unregister)
static void srv_stop(server_pt srv) {
  pthread_mutex_lock(&global_lock);
  // remove from regisrty
  struct ServerSet* set = global_servers_set;
  struct ServerSet* tmp = global_servers_set;
  if (global_servers_set && global_servers_set->server == srv) {
    global_servers_set = global_servers_set->next;
    free(tmp);
    goto sig_srv;
  } else
    while (set) {
      if (set->next && set->next->server == srv) {
        tmp = set->next;
        set->next = set->next->next;
        free(tmp);
        goto sig_srv;
      }
      set = set->next;
    }
  // the server wasn't in the registry - we shouldn't stop it again...
  srv = NULL;
// send a signal to the server, if it was in the registry
sig_srv:
  if (srv) {
    // close the listening socket, preventing new connections from coming in.
    reactor_add((struct Reactor*)srv, srv->srvfd);
    // set the stop server flag.
    srv->run = 0;
    // signal the async object to finish
    Async.signal(srv->async);
  }
  pthread_mutex_unlock(&global_lock);
}
// deregisters and stops all servers
static void srv_stop_all(void) {
  while (global_servers_set)
    srv_stop(global_servers_set->server);
}

// handles signals
static void on_signal(int sig) {
  if (!global_servers_set) {
    signal(sig, SIG_DFL);
    raise(sig);
    return;
  }
  fprintf(stderr, "(pid %d) Exit signal received.\n", getpid());
  srv_stop_all();
}
