/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "evio.h"

#ifdef EVIO_ENGINE_KQUEUE

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* *****************************************************************************
Global data and system independant code
***************************************************************************** */

static int evio_fd = -1;

/** Closes the `epoll` / `kqueue` object, releasing it's resources. */
void evio_close() {
  if (evio_fd != -1)
    close(evio_fd);
  evio_fd = -1;
}

/**
returns true if the evio is available for adding or removing file descriptors.
*/
int evio_isactive(void) { return evio_fd >= 0; }

/* *****************************************************************************
BSD `kqueue` implementation
***************************************************************************** */

/**
Creates the `epoll` or `kqueue` object.
*/
intptr_t evio_create() {
  evio_close();
  return evio_fd = kqueue();
}

/**
Removes a file descriptor from the polling object.
*/
void evio_remove(int fd) {
  if (evio_fd < 0)
    return;
  struct kevent chevent[3];
  EV_SET(chevent, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
  EV_SET(chevent + 1, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
  EV_SET(chevent + 2, fd, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
  kevent(evio_fd, chevent, 3, NULL, 0, NULL);
}

/**
Adds a file descriptor to the polling object.
*/
int evio_add(int fd, void *callback_arg) {
  struct kevent chevent[2];
  EV_SET(chevent, fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0, 0, callback_arg);
  EV_SET(chevent + 1, fd, EVFILT_WRITE,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT, 0, 0, callback_arg);
  return kevent(evio_fd, chevent, 2, NULL, 0, NULL);
}

/**
Adds a file descriptor to the polling object (ONE SHOT), to be polled for
incoming data (`evio_on_data` wil be called).
*/
int evio_add_read(int fd, void *callback_arg) {
  struct kevent chevent[1];
  EV_SET(chevent, fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0, 0, callback_arg);
  return kevent(evio_fd, chevent, 1, NULL, 0, NULL);
}

/**
Adds a file descriptor to the polling object (ONE SHOT), to be polled for
outgoing buffer readiness data (`evio_on_ready` wil be called).
*/
int evio_add_write(int fd, void *callback_arg) {
  struct kevent chevent[1];
  EV_SET(chevent, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0, 0, callback_arg);
  return kevent(evio_fd, chevent, 1, NULL, 0, NULL);
}

/**
Creates a timer file descriptor, system dependent.
*/
int evio_open_timer() {
#ifdef P_tmpdir
  if (P_tmpdir[sizeof(P_tmpdir) - 1] == '/') {
    char name_template[] = P_tmpdir "evio_facil_timer_XXXXXX";
    return mkstemp(name_template);
  }
  char name_template[] = P_tmpdir "/evio_facil_timer_XXXXXX";
  return mkstemp(name_template);
#else
  char name_template[] = "/tmp/evio_facil_timer_XXXXXX";
  return mkstemp(name_template);
#endif
}

/**
Adds a timer file descriptor, so that callbacks will be called for it's events.
*/
int evio_set_timer(int fd, void *callback_arg, unsigned long milliseconds) {
  struct kevent chevent;
  EV_SET(&chevent, fd, EVFILT_TIMER, EV_ADD | EV_ENABLE | EV_ONESHOT, 0,
         milliseconds, callback_arg);
  return kevent(evio_fd, &chevent, 1, NULL, 0, NULL);
}

/**
Reviews any pending events (up to EVIO_MAX_EVENTS) and calls any callbacks.
 */
int evio_review(const int timeout_millisec) {
  if (evio_fd < 0)
    return -1;
  struct kevent events[EVIO_MAX_EVENTS];

  const struct timespec timeout = {.tv_sec = (timeout_millisec / 1024),
                                   .tv_nsec =
                                       ((timeout_millisec % 1024) * 1000000)};
  /* wait for events and handle them */
  int active_count =
      kevent(evio_fd, NULL, 0, events, EVIO_MAX_EVENTS, &timeout);

  if (active_count > 0) {
    for (int i = 0; i < active_count; i++) {
      // test for event(s) type
      if (events[i].filter == EVFILT_READ || events[i].filter == EVFILT_TIMER) {
        evio_on_data(events[i].udata);
      }
      // connection errors should be reported after `read` in case there's data
      // left in the buffer.
      if (events[i].flags & (EV_EOF | EV_ERROR)) {
        // errors are hendled as disconnections (on_close)
        // fprintf(stderr, "%p: %s\n", events[i].udata,
        //         (events[i].flags & EV_EOF)
        //             ? "EV_EOF"
        //             : (events[i].flags & EV_ERROR) ? "EV_ERROR" : "WTF?");
        evio_on_error(events[i].udata);
      } else if (events[i].filter == EVFILT_WRITE) {
        // we can only write if there's no error in the socket
        evio_on_ready(events[i].udata);
      }
    }
  } else if (active_count < 0) {
    if (errno == EINTR)
      return 0;
    return -1;
  }
  return active_count;
}

#endif /* system dependent code */
