/*
copyright: Boaz segev, 2015
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef LIB_SERVER_H
#define LIB_SERVER_H

#define LIB_SERVER_VERSION 0.1.0

// lib server is based off and requires the following libraries:
#include "libreact.h"
#include "libasync.h"
#include "libbuffer.h"

/////////////////
// General info

// The following types are defined for the userspace of this library:

typedef struct Server* server_pt;  // used internally. no public data exposed.
struct ServerSettings;
struct Protocol;

// The start_server(...) macro is a shortcut that allows to easily create a
// ServerSettings structure and start the server.
#define start_server(...) Server.listen((struct ServerSettings){__VA_ARGS__})

/////////////////
// The Protocol

// the Protocol struct defines the callbacks used for the connection and sets
// the behaviour for the connection's protocol.
struct Protocol {
  // a string to identify the protocol's service (i.e. "http").
  char* service;
  // called when a connection is opened
  void (*on_open)(struct Server*, int sockfd);
  // called when a data is available
  void (*on_data)(struct Server*, int sockfd);
  // called when the socket is ready to be written to.
  void (*on_ready)(struct Server*, int sockfd);
  // called when the server is shutting down, but before closing the connection.
  void (*on_shutdown)(struct Server*, int sockfd);
  // called when the connection was closed
  void (*on_close)(struct Server*, int sockfd);
  // called when the connection's timeout was reached
  void (*ping)(struct Server*, int sockfd);
};

/////////////////
// The Server Settings

// these settings will be used to setup server behaviour. missing settings will
// be filled in with default values. only the `protocol` setting, which sets the
// default protocol, is required.
struct ServerSettings {
  // the default protocol.
  struct Protocol* protocol;
  // the port to listen to. defaults to 3000.
  char* port;
  // the address to bind to. defaults to NULL (all localhost addresses)
  char* address;
  // called when the server starts, allowing for further initialization, such
  // as timed event scheduling.
  // this will be called seperately for every process.
  void (*on_init)(struct Server* server);
  // called when the server is done, to clean up any leftovers.
  void (*on_finish)(struct Server* server);
  // called whenever an event loop had cycled (a "tick").
  void (*on_tick)(struct Server* server);
  // called if an event loop cycled with no pending events.
  void (*on_idle)(struct Server* server);
  // called each time a new worker thread is spawned (within the new thread).
  void (*on_init_thread)(struct Server* server);
  // sets the amount of threads to be created for the server's thread-pool.
  // Defaults to 1 - all `on_data`/`on_message` callbacks are deffered to a
  // single working thread, protecting the reactor from blocking code.
  // Use a negative value (-1) to disable the creation of any working threads.
  int threads;
  // sets the amount of processes to be used (processes will be forked).
  // Defaults to 1 working processes (no forking).
  int processes;
  // a NULL terminated string for when the server is busy (defaults to NULL - a
  // simple disconnection with no message).
  char* busy_msg;
  // opaque user data.
  void* udata;
  // sets the timeout for new connections. defaults to 5 seconds.
  unsigned char timeout;
};

/////////////////
// The Server API
// and helper functions

// The following allows access to helper functions and defines a namespace
// for
// the API in this library.
extern const struct ServerClass {
  // listens to a server with the following server settings (which MUST include
  // a default protocol).
  int (*listen)(struct ServerSettings);
  // returns the computed capacity for any server instance on the system.
  long (*capacity)(void);
  // stops a specific server, closing any open connections.
  void (*stop)(struct Server* server);
  // stops any and all server instances, closing any open connections.
  void (*stop_all)(void);
  // allows direct access to the reactor object. use with care.
  struct ReactorSettings* (*reactor)(struct Server* server);
  // allows direct access to the reactor object. use with care.
  struct ServerSettings* (*settings)(struct Server* server);
  // retrives the active protocol object for the requested file descriptor.
  struct Protocol* (*get_protocol)(struct Server* server, int sockfd);
  // sets the active protocol object for the requested file descriptor.
  void (*set_protocol)(struct Server* server,
                       int sockfd,
                       struct Protocol* new_protocol);
  // retrives an opaque pointer set by `set_udata` and associated with the
  // connection.
  //
  // since no new connections are expected on fd == 0..2, it's possible to store
  // global data in these locations.
  void* (*get_udata)(struct Server* server, int sockfd);
  // sets the opaque pointer to be associated with the connection. returns the
  // old pointer, if any.
  void* (*set_udata)(struct Server* server, int sockfd, void* udata);
  // counts the number of connections for the specified protocol (NULL = all
  // protocols).
  long (*count)(struct Server* server, char* service);
  // schedules a specific task to run asyncronously for each connection.
  // a NULL service identifier == all connections (all protocols).
  long (*each)(struct Server* server,
               char* service,
               void (*task)(struct Server* server, int fd, void* arg),
               void* arg);
  // schedules a specific task to run asyncronously for a specific connection.
  // a NULL service identifier == all connections (all protocols).
  //
  // returns -1 on failure, 0 on success (success being scheduling or performing
  // the task).
  int (*fd_task)(struct Server* server,
                 int sockfd,
                 void (*task)(struct Server* server, int fd, void* arg),
                 void* arg);
  // Runs an asynchronous task, IF threading is enabled (set the
  // `threads` to 1 (the default) or greater). Returns -1 on error or 0
  // on
  // succeess.
  int (*run_async)(struct Server* self, void (*task)(void*), void* arg);
  // Creates a system timer (at the cost of 1 file descriptor) and pushes the
  // timer to the reactor. The task will NOT repeat. Returns -1 on error or the
  // new file descriptor on succeess.
  int (*run_after)(struct Server* self,
                   long miliseconds,
                   void (*task)(void*),
                   void* arg);
  // Creates a system timer (at the cost of 1 file descriptor) and pushes the
  // timer to the reactor. The task will repeat `repetitions` times. if
  // `repetitions` is set to 0, task will repeat forever. Returns -1 on error
  // or the new file descriptor on succeess.
  int (*run_every)(struct Server* self,
                   long miliseconds,
                   int repetitions,
                   void (*task)(void*),
                   void* arg);

  /// "touches" a socket, reseting it's timeout counter.
  void (*touch)(struct Server* server, int sockfd);
  // returns true if the a specific connection's protected callback is running
  //
  // protected callbacks include only the `on_message` callback and tasks
  // forwarded to the connection using the `each` function.
  unsigned char (*is_busy)(struct Server* server, int sockfd);

  // reads up to `max_len` of data from a socket. the data is stored in the
  // `buffer` and the number of bytes received is returned. Returns -1 if no
  // data was available. Returns 0 if an error was raised and the connection
  // was closed.
  ssize_t (*read)(int sockfd, void* buffer, size_t max_len);

  // Copies & writes data to the socket, managing an asyncronous buffer if
  // needed.
  //
  // Copy is only performed if a buffer is needed.
  //
  // returns 0 on success. success means that the data is in a buffer waiting to
  // be written. If the socket is closed at this point, the buffer will be
  // destroyed (never sent).
  //
  // on error, returns either -1 (closed socket or socket error) or the number
  // of bytes actually sent (unable to initialize a buffer).
  ssize_t (*write)(struct Server* server, int sockfd, void* data, size_t len);
  // Writes data to the socket, managing an asyncronous buffer if needed.
  //
  // The memory is always freed once the data was written.
  //
  // returns 0 on success. success means that the data is in a buffer waiting to
  // be written. If the socket is closed at this point, the buffer will be
  // destroyed (never sent).
  //
  // on error, returns either -1 (closed socket or socket error) or the number
  // of bytes actually sent (unable to initialize a buffer).
  ssize_t (*write_move)(struct Server* server,
                        int sockfd,
                        void* data,
                        size_t len);
  // Writes data to the socket, managing an asyncronous buffer if needed.
  //
  // The memory is always freed once the data was written.
  //
  // returns 0 on success. success means that the data is in a buffer waiting to
  // be written. If the socket is closed at this point, the buffer will be
  // destroyed (never sent).
  //
  // on error, returns either -1 (closed socket or socket error) or the number
  // of bytes actually sent (unable to initialize a buffer).
  void (*close)(struct Server* server, int sockfd);
} Server;

#endif
