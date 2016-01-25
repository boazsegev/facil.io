/*
copyright: Boaz segev, 2015
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "lib-server.h"

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
// helper predefinitions

// signal management
static void register_server(struct Server* server);
static void on_signal(int sig);
static void stop_one(struct Server* server);

// socket binding and server limits
static int bind_server_socket(struct Server*);
static int set_non_blocking_socket(int fd);
static long calculate_file_limit(void);

// async data sending buffer and helpers
struct Buffer {
  struct Buffer* next;
  int fd;
  void* data;
  int len;
  int sent;
  unsigned notification : 1;
  unsigned moved : 1;
  unsigned final : 1;
  unsigned urgent : 1;
  unsigned destroy : 4;
  unsigned rsv : 3;
};

////////////////////////////////////////////////////////////////////////////////
// The server data object container

struct Server {
  // inherit the reactor (must be first in the struct, for pointer conformity).
  struct ReactorSettings reactor;
  // a pointer to the server's settings object.
  struct ServerSettings* settings;
  // a pointer to the thread pool object (libasync).
  struct Async* async;
  // maps each connection to it's protocol.
  struct Protocol** protocol_map;
  // Used to send the server data + sockfd number to any worker threads.
  // pointer offset calculations are used to calculate the sockfd.
  struct Server** server_map;
  // maps each udata with it's associated connection fd.
  void** udata_map;
  /// maps a connection's "busy" flag, preventing the same connection from
  /// running `on_data` on two threads. use busy[fd] to get the status of the
  /// flag.
  char* busy;
  /// maps all connection's timeout values. use tout[fd] to get/set the
  /// timeout.
  unsigned char* tout;
  /// maps all connection's idle cycle count values. use idle[fd] to get/set
  /// the count.
  unsigned char* idle;
  // a buffer map.
  void** buffer_map;
  // old Signal handlers
  void (*old_sigint)(int);
  void (*old_sigterm)(int);
  // socket capacity
  long capacity;
  /// the last timeout review
  time_t last_to;
  /// the server socket
  int srvfd;
  /// the original process pid
  pid_t root_pid;
};

////////////////////////////////////////////////////////////////////////////////
// timer helpers
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
static void tp_perform_on_data(struct Server* self, int fd) {
  struct TimerProtocol* timer =
      (struct TimerProtocol*)Server.get_protocol(self, fd);
  if (timer) {
    // set local data copy, to avoid race contitions related to `free`.
    void (*task)(void*) = (void (*)(void*))timer->task;
    void* arg = timer->arg;
    // perform the task
    if (task)
      task(arg);
    // on epoll we need to reset the timer
    self->reactor.reset_timer(&self->reactor, fd);
    // close the file descriptor
    if (timer->repeat < 0)
      return;
    if (timer->repeat == 0)
      close(fd);
    timer->repeat--;
  }
}

// the timer's on_close (cleanup)
static void tp_perform_on_close(struct Server* self, int fd) {
  // free the protocol object when it was "aborted" using `close`.
  struct TimerProtocol* timer =
      (struct TimerProtocol*)Server.get_protocol(self, fd);
  if (timer)
    free(timer);
}

// creates a new TimeProtocol object.
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
// The busy flag is used to make sure that a single connection dosn't perform
// two "critical" tasks at the same time. Critical tasks are defined as the
// `on_message` callback and any user task requested by `each` or `fd_task`.

static int set_to_busy(struct Server* server, int sockfd) {
  static pthread_mutex_t locker = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&locker);
  if (server->busy[sockfd] == 1) {
    pthread_mutex_unlock(&locker);
    return 0;
  }
  server->busy[sockfd] = 1;
  pthread_mutex_unlock(&locker);
  return 1;
}

////////////////////////////////////////////////////////////////////////////////
// The Server's callbacks

// use pointer inheritance casting to simplify access to the server object.
#define _server_(reactor) ((struct Server*)(reactor))
// a short-cut for grabbing the connection's protocol object.
#define _protocol_(reactor, fd) (_server_(reactor)->protocol_map[(fd)])

static void manage_timeout(struct ReactorSettings* reactor) {
  static time_t last_to = 0;
  static char working = 0;
  if (working)
    return;
  if (last_to != reactor->last_tick) {
    working = 1;
    // ?? ??
    // // ignore delta, add 1 to timer (even if delta is bigger) and add 1
    // second
    // // to timeout.
    // // timeout isn't exact, but should also avoid killing a new connection
    // // when timeout was set to 0
    // ?? ??
    // // or use delta...
    int delta = reactor->last_tick - last_to;
    for (long i = 3; i < _server_(reactor)->capacity; i++) {
      if (_server_(reactor)->tout[i]) {
        if (_server_(reactor)->tout[i] > _server_(reactor)->idle[i])
          _server_(reactor)->idle[i] += _server_(reactor)->idle[i] ? delta : 1;
        else {
          if (_server_(reactor)->protocol_map[i] &&
              _server_(reactor)->protocol_map[i]->ping)
            _server_(reactor)->protocol_map[i]->ping(_server_(reactor), i);
          else if (!_server_(reactor)->busy[i] ||
                   _server_(reactor)->idle[i] == 255)
            close(i);
        }
      }
    }
    // ready for next call.
    last_to = reactor->last_tick;
    working = 0;
    // double en = (float)clock()/CLOCKS_PER_SEC;
    // printf("timeout review took %f milliseconds\n", (en-st)*1000);
  }
}

/// called for any open file descriptors when the reactor is shutting down.
static void on_shutdown(struct ReactorSettings* reactor, int fd) {
  // call all callbacks for active connections.
  for (long i = 0; i <= reactor->last; i++) {
    if (_protocol_(reactor, i) && _protocol_(reactor, i)->on_shutdown)
      _protocol_(reactor, i)->on_shutdown(_server_(reactor), i);
  }
}

// called when a file descriptor was closed (either locally or by the other
// party, when it's a socket or a pipe).
static void on_close(struct ReactorSettings* reactor, int fd) {
  if (!_protocol_(reactor, fd))
    return;
  if (_protocol_(reactor, fd)->on_close)
    _protocol_(reactor, fd)->on_close(_server_(reactor), fd);
  _protocol_(reactor, fd) = 0;
  _server_(reactor)->tout[fd] = 0;
  // we can keep the buffer on standby for the connection... but we won't
  if (_server_(reactor)->buffer_map[fd]) {
    Buffer.destroy(_server_(reactor)->buffer_map[fd]);
    _server_(reactor)->buffer_map[fd] = 0;
  }
}

static void async_on_data(struct Server** p_server) {
  // compute sockfd by comparing the distance between the original pointer and
  // the passed pointer's address.
  if (!(*p_server))
    return;
  int sockfd = p_server - (*p_server)->server_map;
  // printf("threaded on data for %d\n", sockfd);
  struct Protocol* protocol = (*p_server)->protocol_map[sockfd];
  if (!protocol || !protocol->on_data)
    return;
  // if we get the handle, perform the task
  if (set_to_busy(*p_server, sockfd)) {
    protocol->on_data((*p_server), sockfd);
    // release the handle
    (*p_server)->busy[sockfd] = 0;
    return;
  }
  // we didn't get the handle, reschedule.
  Async.run((*p_server)->async, (void (*)(void*))async_on_data, p_server);
}

// The heavy(!) on_data, handles accept and forwards events
static void on_data(struct ReactorSettings* reactor, int fd) {
  static socklen_t cl_addrlen = 0;
  if (fd == reactor->first) {  // listening socket. accept connections.
    int client = 1;
    while (1) {
#ifdef SOCK_NONBLOCK
      client = accept4(fd, NULL, &cl_addrlen, SOCK_NONBLOCK);
      if (client <= 0)
        return;
// perror("accept 4 called");
#else
      client = accept(fd, NULL, &cl_addrlen);
      if (client <= 0)
        return;
      set_non_blocking_socket(client);
// perror("accept reg called");
#endif
      // close the prior protocol stream, if needed
      on_close(reactor, client);
      // setup protocol
      _protocol_(reactor, client) = _server_(reactor)->settings->protocol;
      // setup timeouts
      _server_(reactor)->tout[client] = _server_(reactor)->settings->timeout;
      _server_(reactor)->idle[client] = 0;
      // call on_open
      // register the client - start it off as busy, protocol still initializing
      // we don't need the mutex, because it's all fresh
      _server_(reactor)->busy[client] = 1;
      if (_protocol_(reactor, client)->on_open)
        _protocol_(reactor, client)->on_open(_server_(reactor), client);
      reactor->add(reactor, client);
      _server_(reactor)->busy[client] = 0;
    }
  } else if (_protocol_(reactor, fd) && _protocol_(reactor, fd)->on_data) {
    _server_(reactor)->idle[fd] = 0;
    // clients, forward on.
    if (_server_(reactor)->async) {  // perform multi-thread
      Async.run(_server_(reactor)->async, (void (*)(void*))async_on_data,
                &(_server_(reactor)->server_map[fd]));
    } else {  // perform single thread
      _protocol_(reactor, fd)->on_data(_server_(reactor), fd);
    }
  }
}
// on_ready, handles async buffer sends
static void on_ready(struct ReactorSettings* reactor, int fd) {
  if (_server_(reactor)->buffer_map[fd]) {
    if (Buffer.flush(_server_(reactor)->buffer_map[fd], fd) > 0)
      _server_(reactor)->idle[fd] = 0;
  }
  if (_protocol_(reactor, fd) && _protocol_(reactor, fd)->on_ready)
    _protocol_(reactor, fd)->on_ready(_server_(reactor), fd);
}

// global callbacks

// called after the event loop was created but before any cycling takes
// place, allowing for further initialization (such as adding a server
// socket, setting up timers, etc').
static void on_init(struct ReactorSettings* reactor) {
  if (_server_(reactor)->settings->on_init)
    _server_(reactor)->settings->on_init(_server_(reactor));
}

// called whenever an event loop had cycled (a "tick").
static void on_tick(struct ReactorSettings* reactor) {
  if (_server_(reactor)->settings->on_tick)
    _server_(reactor)->settings->on_tick(_server_(reactor));
  manage_timeout(reactor);
}

// called if an event loop cycled with no pending events.
static void on_idle(struct ReactorSettings* reactor) {
  // // Shrink buffers? - No... if they don't auto-shrink, wtf?
  // for (size_t i = 0; i <= reactor->last; i++) {
  //   if (_server_(reactor)->buffer_map[i])
  //     Buffer.shrink(_server_(reactor)->buffer_map[i]);
  // }
  if (_server_(reactor)->settings->on_idle)
    _server_(reactor)->settings->on_idle(_server_(reactor));
}

// called when a new thread is spawned
void on_init_thread(async_p async, void* server) {
  if (((struct Server*)server)->settings->on_init_thread)
    ((struct Server*)server)->settings->on_init_thread((struct Server*)server);
}

////////////////////////////////////////////////////////////////////////////////
// The Server's main function

// this is the server's main action...
static int server_listen(struct ServerSettings settings) {
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
  if (!settings.threads)
    settings.threads = 1;
  if (!settings.processes)
    settings.processes = 1;
  // We'll use the stack for the server's core memory.
  void* udata_map[calculate_file_limit()];
  struct Protocol* protocol_map[calculate_file_limit()];
  struct Server* server_map[calculate_file_limit()];
  void* buffer_map[calculate_file_limit()];
  char busy[calculate_file_limit()];
  unsigned char tout[calculate_file_limit()];
  unsigned char idle[calculate_file_limit()];
  // populate the Server structure with the data
  struct Server server = {
      .settings = &settings,
      .protocol_map = protocol_map,
      .udata_map = udata_map,
      .server_map = server_map,
      .buffer_map = buffer_map,
      .busy = busy,
      .tout = tout,
      .idle = idle,
      .capacity = calculate_file_limit(),
      .reactor.last = calculate_file_limit() - 1,
      .reactor.on_tick = on_tick,
      .reactor.on_idle = on_idle,
      .reactor.on_init = on_init,
      .reactor.on_data = on_data,
      .reactor.on_ready = on_ready,
      .reactor.on_shutdown = on_shutdown,
      .reactor.on_close = on_close,
  };
  server.reactor.udata = &server;
  // bind the server's socket
  int srvfd = bind_server_socket(&server);
  server.srvfd = srvfd;
  // tell the reactor about the "first" socket it should react to.
  server.reactor.first = srvfd;
  // if we didn't get a socket, quit now.
  if (srvfd < 0)
    return -1;

  // zero out data...
  for (int i = 0; i < calculate_file_limit(); i++) {
    protocol_map[i] = 0;
    server_map[i] = &server;
    busy[i] = 0;
    tout[i] = 0;
    idle[i] = 0;
  }

  // setup concurrency
  server.root_pid = getpid();
  pid_t pids[settings.processes > 0 ? settings.processes : 0];
  if (settings.processes > 1) {
    pids[0] = 0;
    for (int i = 1; i < settings.processes; i++) {
      if (getpid() == server.root_pid)
        pids[i] = fork();
    }
  }
  if (settings.threads > 0) {
    server.async = Async.new(settings.threads, on_init_thread, &server);
    if (server.async < 0) {
      close(srvfd);
      return -1;
    }
  }

  // register signals
  server.old_sigint = signal(SIGINT, on_signal);
  server.old_sigterm = signal(SIGTERM, on_signal);

  // register server for signals
  register_server(&server);

  // let'm know...
  printf("(%d) Starting to listen on port %s (max sockets: %lu, ~%d used)\n",
         getpid(), settings.port, server.capacity,
         (server.async ? server.srvfd + 2 : server.srvfd));

  // bind server data to reactor loop
  reactor_start(&server.reactor);

  // cleanup

  if (settings.processes > 1 && getpid() == server.root_pid) {
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
  close(srvfd);  // already performed by the reactor...
  if (server.async)
    Async.finish(server.async);
  if (settings.on_finish)
    settings.on_finish(&server);
  printf("\n(%d) Finished listening on port %s\n", getpid(), settings.port);
  if (getpid() != server.root_pid) {
    fflush(NULL);
    exit(0);
  }
  signal(SIGINT, server.old_sigint);
  signal(SIGTERM, server.old_sigterm);

  // remove server from registry, it it's still there...
  stop_one(&server);

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// The Server API (helper methods and API container)

////////////////////////////////////////////////////////////////////////////////
// Connection data and protocol management

// get protocol for connection
static struct Protocol* get_protocol(struct Server* server, int sockfd) {
  return server->protocol_map[sockfd];
}

// set protocol for connection
static void set_protocol(struct Server* server,
                         int sockfd,
                         struct Protocol* new_protocol) {
  server->protocol_map[sockfd] = new_protocol;
}

// get protocol for connection
static void* get_udata(struct Server* server, int sockfd) {
  return server->udata_map[sockfd];
}

// set protocol for connection
static void* set_udata(struct Server* server, int sockfd, void* udata) {
  void* old = server->udata_map[sockfd];
  server->udata_map[sockfd] = udata;
  return old;
}

// count existing connections for a specific protocol (NULL is all)
static long count(struct Server* server, char* service) {
  int c = 0;
  for (long i = 0; i < server->capacity; i++) {
    if (server->protocol_map[i]  // there is a protocol
        && (                     // one of the following must apply
               !service          // there's no service name required
               ||                // or
               (server->protocol_map[i]->service &&  // there is a name that
                !strcmp(service, server->protocol_map[i]->service))  // matches
               ))
      c++;
  }
  return c;
}

// reset idle counter (timeout monitor) for a specific connection
// This is unprotected, as it seems that data curruption isn't super important
// for this one
static void touch(struct Server* self, int sockfd) {
  self->idle[sockfd] = 0;
}
// sets the timeout limit for the specified connectionl, in seconds.
static void set_timeout(struct Server* server,
                        int sockfd,
                        unsigned char timeout) {
  server->tout[sockfd] = timeout;
}
// returns true if the connection is performing a critical task.
static unsigned char is_busy(struct Server* self, int sockfd) {
  return self->busy[sockfd];
}

// return a server's reactor
static struct ReactorSettings* reactor(struct Server* server) {
  return &server->reactor;
}

// return the settings used to initiate the server
static struct ServerSettings* server_settings(struct Server* server) {
  return server->settings;
}

// connects an existing connection (fd) to the Server's callback system.
static int server_connect(struct Server* self,
                          int fd,
                          struct Protocol* protocol) {
  // if the connection is already owned by the server - ignore.
  if (self->protocol_map[fd] == protocol)
    return 0;
  // make sure the fd recycled is clean
  on_close(&self->reactor, fd);
  // set protocol for new fd
  self->protocol_map[fd] = protocol;
  // add the fd to the reactor
  if (self->reactor.add(&self->reactor, fd) < 0)
    return -1;
  // remember to call on_open
  if (protocol->on_open)
    protocol->on_open(self, fd);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Connection tasks (implementing `each`)

// perform a task on each existing connection for a specific protocol
// the data-type for async messages
struct ConnTask {
  struct Server* server;
  int fd;
  void (*task)(struct Server* server, int fd, void* arg);
  void* arg;
};
// the async handler
static void perform_each_task(struct ConnTask* task) {
  // is it okay to perform the task?
  if (set_to_busy(task->server, task->fd)) {
    // perform the task
    task->task(task->server, task->fd, task->arg);
    // release the busy flag
    task->server->busy[task->fd] = 0;
    // free the memory
    free(task);
    return;
  }
  // reschedule
  Async.run(task->server->async, (void (*)(void*))perform_each_task, task);
  return;
}
// the message scheduler (async) / performer (single-thread)
static long each(struct Server* server,
                 char* service,
                 void (*task)(struct Server* server, int fd, void* arg),
                 void* arg) {
  int c = 0;
  struct ConnTask* msg;
  for (long i = 0; i < server->capacity; i++) {
    if (server->protocol_map[i]  // there is a protocol
        && (                     // one of the following must apply
               !service          // there's no service name required
               ||                // or
               (server->protocol_map[i]->service &&  // there is a name that
                !strcmp(service, server->protocol_map[i]->service))  // matches
               )) {
      c++;
      if (server->async) {
        if (!(msg = malloc(sizeof(struct ConnTask))))
          return -1;
        *msg = (struct ConnTask){
            .server = server, .task = task, .arg = arg, .fd = i};
        Async.run(server->async, (void (*)(void*))perform_each_task, task);
      } else {
        task(server, i, arg);
      }
    }
  }
  return c;
}

static int fd_task(struct Server* server,
                   int sockfd,
                   void (*task)(struct Server* server, int fd, void* arg),
                   void* arg) {
  struct ConnTask* msg;
  if (server->protocol_map[sockfd]) {
    if (server->async) {
      if (!(msg = malloc(sizeof(struct ConnTask))))
        return -1;
      *msg = (struct ConnTask){
          .server = server, .task = task, .arg = arg, .fd = sockfd};
      Async.run(server->async, (void (*)(void*))perform_each_task, msg);
    } else {
      task(server, sockfd, arg);
    }
    return 0;
  }
  return -1;
}

////////////////////////////////////////////////////////////////////////////////
// Async tasks and timers helpers

// run a task asynchronously
static int run_async(struct Server* self, void (*task)(void*), void* arg) {
  return Async.run(self->async, task, arg);
}

// run a timer, one single time
static int run_every(struct Server* self,
                     long milliseconds,
                     int repetitions,
                     void (*task)(void*),
                     void* arg) {
  int tfd = self->reactor.open_timer_fd();
  if (tfd <= 0)
    return -1;
  // make sure the fd recycled is clean
  on_close(&self->reactor, tfd);
  // set protocol for new fd (timer protocol)
  self->protocol_map[tfd] =
      (struct Protocol*)TimerProtocol(task, arg, repetitions);
  if (self->reactor.add_as_timer(&self->reactor, tfd, milliseconds) < 0) {
    close(tfd);
    self->protocol_map[tfd]->on_close(self, tfd);
    self->protocol_map[tfd] = 0;
    return -1;
  };
  return 0;
}

// run a timer, one single time
static int run_after(struct Server* self,
                     long milliseconds,
                     void (*task)(void*),
                     void* arg) {
  return run_every(self, milliseconds, 1, task, arg);
}

////////////////////////////////////////////////////////////////////////////////
// socket read/write helpers

// reads data and closes socket on error
static ssize_t read_data(int fd, void* buffer, size_t max_len) {
  // reads up to `max_len` of data from a socket. the data is storen in the
  // `buffer` and the number of bytes received is returned.
  // Returns 0 if no data was available. Returns -1 if an error was raised and
  // the connection should be closed.
  ssize_t read = 0;
  if ((read = recv(fd, buffer, max_len, 0)) > 0) {
    return read;
  } else {
    if (read && (errno & (EWOULDBLOCK | EAGAIN)))
      return -1;
  }
  close(fd);
  return 0;
}

// sends data to the socket, managing an async-write buffer if needed
static ssize_t buffer_send(struct Server* server,
                           int sockfd,
                           void* data,
                           size_t len,
                           char move,
                           char urgent) {
  ssize_t snt = -1;
  // reset timeout
  server->idle[sockfd] = 0;
  // try to avoid the buffer if we can.
  if (!server->buffer_map[sockfd]) {
    ssize_t snt = send(sockfd, data, len, 0);
    if (snt < 0 && !(errno & (EWOULDBLOCK | EAGAIN))) {
      if (move && data)
        free(data);
      close(sockfd);
      return -1;
    }
    // no need for a buffer.
    if (snt == len) {
      if (move && data)
        free(data);
      return 0;
    }
    server->buffer_map[sockfd] = Buffer.new(snt > 0 ? snt : 0);
    if (!server->buffer_map[sockfd]) {
      fprintf(stderr,
              "Couldn't initiate a buffer object for conection no. %d\n",
              sockfd);
      if (move && data)
        free(data);
      return snt;
    }
  }

  if (move ? (urgent ? Buffer.write_move_next : Buffer.write_move)(
                 server->buffer_map[sockfd], data, len)
           : (urgent ? Buffer.write_move : Buffer.write)(
                 server->buffer_map[sockfd], data, len)) {
    Buffer.flush(server->buffer_map[sockfd], sockfd);
    return 0;
  }
  fprintf(stderr, "couldn't write to the buffer on address %p...\n",
          server->buffer_map[sockfd]);
  return snt;
}

static ssize_t buffer_write(struct Server* server,
                            int sockfd,
                            void* data,
                            size_t len) {
  return buffer_send(server, sockfd, data, len, 0, 0);
}
static ssize_t buffer_write_urgent(struct Server* server,
                                   int sockfd,
                                   void* data,
                                   size_t len) {
  return buffer_send(server, sockfd, data, len, 0, 1);
}
static ssize_t buffer_move(struct Server* server,
                           int sockfd,
                           void* data,
                           size_t len) {
  return buffer_send(server, sockfd, data, len, 1, 0);
}
static ssize_t buffer_write_urgent_move(struct Server* server,
                                        int sockfd,
                                        void* data,
                                        size_t len) {
  return buffer_send(server, sockfd, data, len, 1, 1);
}

static void buffer_close(struct Server* server, int sockfd) {
  server->buffer_map[sockfd]
      ? Buffer.close_when_done(server->buffer_map[sockfd], sockfd)
      : close(sockfd);
}

////////////////////////////////////////////////////////////////////////////////
// stopping and signal managment

// types and global data
struct ServerSet {
  struct ServerSet* next;
  struct Server* server;
};
static struct ServerSet* global_servers_set = NULL;
static pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

// registers servers
static void register_server(struct Server* server) {
  pthread_mutex_lock(&global_lock);
  struct ServerSet** set = &global_servers_set;
  while (*set)
    set = &((*set)->next);
  *set = malloc(sizeof(struct ServerSet));
  if (!set) {
    pthread_mutex_unlock(&global_lock);
    raise(SIGSEGV);
  }
  (*set)->next = 0;
  (*set)->server = server;
  pthread_mutex_unlock(&global_lock);
}

// handles signals
static void on_signal(int sig) {
  struct ServerSet* set = global_servers_set;
  if (!set) {
    signal(sig, SIG_DFL);
    pthread_mutex_unlock(&global_lock);
    raise(sig);
    return;
  }
  struct ServerSet* prev = NULL;
  while (set) {
    set->server->reactor.stop(&(set->server->reactor));
    prev = set;
    set = set->next;
    free(prev);
  }
  global_servers_set = NULL;
}

// stops a specific server
static void stop_one(struct Server* server) {
  server->reactor.stop((struct ReactorSettings*)server);
  pthread_mutex_lock(&global_lock);
  struct ServerSet* set = global_servers_set;
  struct ServerSet* prev = NULL;
  while (set && set->server != server) {
    prev = set;
    set = set->next;
  }
  if (set) {
    if (prev)
      prev->next = set->next;
    if (set == global_servers_set)
      global_servers_set = set->next;
    free(set);
  }
  pthread_mutex_unlock(&global_lock);
}
// stops all the servers.
static void stop_all(void) {
  pthread_mutex_lock(&global_lock);
  struct ServerSet* set = global_servers_set;
  if (!set) {
    pthread_mutex_unlock(&global_lock);
    return;
  }
  while (set) {
    set->server->reactor.stop(&(set->server->reactor));
    global_servers_set = set;
    set = set->next;
    free(global_servers_set);
  }
  global_servers_set = NULL;
  pthread_mutex_unlock(&global_lock);
}

////////////////////////////////////////////////////////////////////////////////
// The actual Server API access setup
// The following allows access to helper functions and defines a namespace
// for the API in this library.
const struct ServerClass Server = {
    .capacity = calculate_file_limit,
    .listen = server_listen,
    .stop = stop_one,
    .stop_all = stop_all,
    .touch = touch,
    .set_timeout = set_timeout,
    .connect = server_connect,
    .is_busy = is_busy,
    .reactor = reactor,
    .settings = server_settings,
    .get_protocol = get_protocol,
    .set_protocol = set_protocol,
    .get_udata = get_udata,
    .set_udata = set_udata,
    .run_async = run_async,
    .run_after = run_after,
    .run_every = run_every,
    .read = read_data,
    .write = buffer_write,
    .write_move = buffer_move,
    .write_urgent = buffer_write_urgent,
    .write_move_urgent = buffer_write_urgent_move,
    .close = buffer_close,
    .count = count,
    .each = each,
    .fd_task = fd_task,
};

////////////////////////////////////////////////////////////////////////////////
// socket helpers

// sets a socket to non blocking state
static inline int set_non_blocking_socket(int fd)  // Thanks to Bjorn Reese
{
/* If they have O_NONBLOCK, use the Posix way to do it */
#if defined(O_NONBLOCK)
  /* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
  static int flags;
  if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
    flags = 0;
  // printf("flags initial value was %d\n", flags);
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
  /* Otherwise, use the old way of doing it */
  static int flags = 1;
  return ioctl(fd, FIOBIO, &flags);
#endif
}

// helper functions used in the context of this file
static int bind_server_socket(struct Server* self) {
  int srvfd;
  // setup the address
  struct addrinfo hints;
  struct addrinfo* servinfo;        // will point to the results
  memset(&hints, 0, sizeof hints);  // make sure the struct is empty
  hints.ai_family = AF_UNSPEC;      // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;      // fill in my IP for me
  if (getaddrinfo(self->settings->address, self->settings->port, &hints,
                  &servinfo)) {
    perror("addr err");
    return -1;
  }
  // get the file descriptor
  srvfd =
      socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  if (srvfd <= 0) {
    perror("socket err");
    freeaddrinfo(servinfo);
    return -1;
  }
  // make sure the socket is non-blocking
  if (set_non_blocking_socket(srvfd) < 0) {
    perror("couldn't set socket as non blocking! ");
    freeaddrinfo(servinfo);
    close(srvfd);
    return -1;
  }
  // avoid the "address taken"
  {
    int optval = 1;
    setsockopt(srvfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  }
  // bind the address to the socket
  {
    int bound = 0;
    for (struct addrinfo* p = servinfo; p != NULL; p = p->ai_next) {
      if (!bind(srvfd, p->ai_addr, p->ai_addrlen))
        bound = 1;
    }

    if (!bound) {
      perror("bind err");
      freeaddrinfo(servinfo);
      close(srvfd);
      return -1;
    }
  }
  freeaddrinfo(servinfo);
  // listen in
  if (listen(srvfd, SOMAXCONN) < 0) {
    perror("couldn't start listening");
    close(srvfd);
    return -1;
  }
  return srvfd;
}

////////////////
// file limit helper
static long calculate_file_limit(void) {
  // get current limits
  static long flim = 0;
  if (flim)
    return flim;
#ifdef _SC_OPEN_MAX
  flim = sysconf(_SC_OPEN_MAX) - 1;
#elif defined(OPEN_MAX)
  flim = OPEN_MAX - 1;
#endif
  // try to maximize limits - collect max and set to max
  struct rlimit rlim;
  getrlimit(RLIMIT_NOFILE, &rlim);
  // printf("Meximum open files are %llu out of %llu\n", rlim.rlim_cur,
  //        rlim.rlim_max);
  rlim.rlim_cur = rlim.rlim_max;
  setrlimit(RLIMIT_NOFILE, &rlim);
  getrlimit(RLIMIT_NOFILE, &rlim);
  // printf("Meximum open files are %llu out of %llu\n", rlim.rlim_cur,
  //        rlim.rlim_max);
  // if the current limit is higher than it was, update
  if (flim < rlim.rlim_cur)
    flim = rlim.rlim_cur;
  // review stack memory limits - don't use more than half...?
  // use 1 byte (char) for busy_map;
  // use 1 byte (char) for idle_map;
  // use 1 byte (char) for tout_map;
  // use 8 byte (void *) for protocol_map;
  // use 8 byte (void *) for server_map;
  // ------
  // total of 19 (assume 20) bytes per connection.
  //
  // assume a 2Kb stack allocation (per connection) for
  // network IO buffer and request management...?
  // (this would be almost wrong to assume, as buffer might be larger)
  // -------
  // 2068 Byte
  // round up to page size?
  // 4096
  //
  getrlimit(RLIMIT_STACK, &rlim);
  if (flim * 4096 > rlim.rlim_cur && flim * 4096 < rlim.rlim_max) {
    rlim.rlim_cur = flim * 4096;
    setrlimit(RLIMIT_STACK, &rlim);
    getrlimit(RLIMIT_STACK, &rlim);
  }
  if (flim > rlim.rlim_cur / 4096) {
    // printf("The current maximum file limit (%d) if reduced due to stack
    // memory "
    //        "limits(%d)\n",
    //        flim, (int)(rlim.rlim_cur / 2068));
    flim = rlim.rlim_cur / 4096;
  } else {
    // printf(
    //     "The current maximum file limit (%d) is supported by the stack
    //     memory
    //     "
    //     "limits(%d)\n",
    //     flim, (int)(rlim.rlim_cur / 2068));
  }

  // how many Kb per connection? assume 8Kb for kernel? x2 (16Kb).
  // (http://www.metabrew.com/article/a-million-user-comet-application-with-mochiweb-part-3)
  // 10,000 connections == 16*1024*10000 == +- 160Mb? seems a tight fit...
  // i.e. the Http request buffer is 8Kb... maybe 24Kb is a better minimum?
  getrlimit(RLIMIT_DATA, &rlim);
  if (flim > rlim.rlim_cur / (24 * 1024)) {
    printf(
        "The current maximum file limit (%ld) if reduced due to system "
        "memory "
        "limits(%ld)\n",
        flim, (long)(rlim.rlim_cur / (24 * 1024)));
    flim = rlim.rlim_cur / (24 * 1024);
  } else {
    // printf(
    //     "The current maximum file limit (%d) is supported by the stack
    //     memory
    //     "
    //     "limits(%d)\n",
    //     flim, (int)(rlim.rlim_cur / 2068));
  }
  // return what we have
  return flim;
}
