/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef H_FACIL_EVIO_H
#include <stdint.h>
#include <stdlib.h>

/**
This is an `epoll` / `kqueue` edge triggered wrapper, allowing for portability
between BSD and Linux polling machanisms as well as routing events to hard-coded
callbacks (weak function symbols) instead of the polling return values.

The callbacks supported by thils library:

* `evio_on_data(intptr_t)` called when data is available or on timer.
* `evio_on_ready(intptr_t)` called when writing is possible.
* `evio_on_error(intptr_t)` called when an error occured.
* `evio_on_close(intptr_t)` called when the connection was remotely closed.

*/
#define H_FACIL_EVIO_H
#define LIB_EVIO_VERSION_MAJOR 0
#define LIB_EVIO_VERSION_MINOR 1
#define LIB_EVIO_VERSION_PATCH 2

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EVIO_MAX_EVENTS
#define EVIO_MAX_EVENTS 64
#endif
#ifndef EVIO_TICK
#define EVIO_TICK 256 /** in milliseconsd */
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

/**
Closes the `epoll` / `kqueue` object, releasing it's resources.
*/
void evio_close(void);

/**
returns true if the evio is available for adding or removing file descriptors.
*/
int evio_isactive(void);

/* *****************************************************************************
Adding and removing normal file descriptors.
*/

/**
Adds a file descriptor to the polling object.

Returns -1 on error, otherwise return value is system dependent and can be
safely ignored.
*/
int evio_add(int fd, void *callback_arg);

/**
Removes a file descriptor from the polling object.

Returns -1 on error, otherwise return value is system dependent. If the file
descriptor did exist in the polling object, it isn't an error.
*/
int evio_remove(int fd);

/* *****************************************************************************
Timers.
*/

/**
Creates a timer file descriptor, system dependent.

Returns -1 on error, or a valid fd on success.

NOTE: Systems have a limit on the number of timers that can be opened.
*/
intptr_t evio_open_timer(void);

/**
Adds a timer file descriptor, so that callbacks will be called for it's events.

Returns -1 on error, otherwise return value is system dependent.
*/
intptr_t evio_add_timer(int fd, void *callback_arg, unsigned long milliseconds);

/**
Re-arms the timer.

Required only by `epoll`. `kqueue` timers will continue cycling regardless.
*/
void evio_reset_timer(int timer_fd);

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
