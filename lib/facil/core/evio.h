/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_FACIL_EVIO_H
#include <stdint.h>
#include <stdlib.h>

/**
This is an `epoll` / `kqueue` ONE-SHOT polling wrapper, allowing for portability
between BSD and Linux polling machanisms and routing events to hard-coded
callbacks (weak function symbols).

The callbacks supported by thils library:

* `evio_on_data(intptr_t)` called when data is available or on timer.
* `evio_on_ready(intptr_t)` called when writing is possible.
* `evio_on_error(intptr_t)` called when an error occured.
* `evio_on_close(intptr_t)` called when the connection was remotely closed.

*/
#define H_FACIL_EVIO_H
#define LIB_EVIO_VERSION_MAJOR 0
#define LIB_EVIO_VERSION_MINOR 2
#define LIB_EVIO_VERSION_PATCH 0

#if defined(__linux__)
#define EVIO_ENGINE_EPOLL 1
#elif defined(__APPLE__) || defined(__unix__)
#define EVIO_ENGINE_KQUEUE 1
#else
#error This library currently supports only Unix based systems with kqueue or epoll (i.e. Linux and BSD)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EVIO_MAX_EVENTS
#define EVIO_MAX_EVENTS 64
#endif
#ifndef EVIO_TICK
#define EVIO_TICK 512 /** in milliseconsd */
#endif

#if (EVIO_MAX_EVENTS & 1)
#error EVIO_MAX_EVENTS must be an EVEN number.
#endif
#if EVIO_MAX_EVENTS <= 3
#error EVIO_MAX_EVENTS must be an GREATER than 4.
#endif

/**
Creates the `epoll` or `kqueue` object.

It's impossible to add or remove file descriptors from the polling system before
calling this method.

Returns -1 on error, otherwise returns a unique value representing the `epoll`
or `kqueue` object. The returned value can be safely ignored.

NOTE: Once an `epoll` / `kqueue` object was opened, `fork` should be avoided,
since ALL the events will be shared among the forked processes (while not ALL
the file descriptors are expected to be shared).
*/
intptr_t evio_create(void);

/**
Reviews any pending events (up to EVIO_MAX_EVENTS) and calls any callbacks.

Waits up to `timeout_millisec` for events to occur.

Returns -1 on error, otherwise returns the number of events handled.
*/
int evio_review(const int timeout_millisec);

/** Waits up to `timeout_millisec` for events. Events aren't reviewed. */
int evio_wait(const int timeout_millisec);

/**
Closes the `epoll` / `kqueue` object, releasing it's resources (important if
forking!).
*/
void evio_close(void);

/**
returns true if the evio is available for adding or removing file descriptors.
*/
int evio_isactive(void);

/* *****************************************************************************
Adding and removing normal file descriptors (ONE SHOT).
*/

/**
Adds a file descriptor to the polling object (ONE SHOT), both for read / write
readiness.

Returns -1 on error, otherwise return value is system dependent and can be
safely ignored.
*/
int evio_add(int fd, void *callback_arg);

/**
Adds a file descriptor to the polling object (ONE SHOT), to be polled for
incoming data (`evio_on_data` wil be called).

Returns -1 on error, otherwise return value is system dependent and can be
safely ignored.
*/
int evio_add_read(int fd, void *callback_arg);

/**
Adds a file descriptor to the polling object (ONE SHOT), to be polled for
outgoing buffer readiness data (`evio_on_ready` wil be called).

Returns -1 on error, otherwise return value is system dependent and can be
safely ignored.
*/
int evio_add_write(int fd, void *callback_arg);

/**
Removes a file descriptor from the polling object.
*/
void evio_remove(int fd);

/* *****************************************************************************
Timers.
*/

/**
Creates a timer file descriptor, system dependent.

Returns -1 on error, or a valid fd on success.

NOTE: Systems have a limit on the number of timers that can be opened.
*/
int evio_open_timer(void);

/**
Adds a timer file descriptor, so that callbacks will be called for it's events.

Returns -1 on error, otherwise return value is system dependent.
*/
int evio_set_timer(int fd, void *callback_arg, unsigned long milliseconds);

/* *****************************************************************************
Callbacks - override these.
*/
void evio_on_data(void *);
void evio_on_ready(void *);
void evio_on_error(void *);
void evio_on_close(void *);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* H_FACIL_EVIO_H */
