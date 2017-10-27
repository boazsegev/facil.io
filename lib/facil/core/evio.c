/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "evio.h"

#if !defined(__unix__) && !defined(__linux__) && !defined(__APPLE__) &&        \
    !defined(__CYGWIN__)
#error This library currently supports only Unix based systems (i.e. Linux and BSD)
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/*
#define EVIO_MAX_EVENTS 64
#define EVIO_TICK 256
*/

#if (EVIO_MAX_EVENTS & 1) || EVIO_MAX_EVENTS < 3
#error EVIO_MAX_EVENTS must be an even number higher than 4
#endif

/* *****************************************************************************
Callbacks - weak versions to be overridden.
***************************************************************************** */
#pragma weak evio_on_data
void __attribute__((weak)) evio_on_data(void *arg) { (void)arg; }
#pragma weak evio_on_ready
void __attribute__((weak)) evio_on_ready(void *arg) { (void)arg; }
#pragma weak evio_on_error
void __attribute__((weak)) evio_on_error(void *arg) { (void)arg; }
#pragma weak evio_on_close
void __attribute__((weak)) evio_on_close(void *arg) { (void)arg; }

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

#if defined(__linux__) || defined(__CYGWIN__)
/* *****************************************************************************
Linux `epoll` implementation
***************************************************************************** */
#include <sys/epoll.h>
#include <sys/timerfd.h>

/**
Creates the `epoll` or `kqueue` object.
*/
intptr_t evio_create() { return evio_fd = epoll_create1(EPOLL_CLOEXEC); }

/**
Removes a file descriptor from the polling object.
*/
int evio_remove(int fd) {
  struct epoll_event chevent = {0};
  return epoll_ctl(evio_fd, EPOLL_CTL_DEL, fd, &chevent);
}

/**
Adds a file descriptor to the polling object.
*/
int evio_add(int fd, void *callback_arg) {
  struct epoll_event chevent = {0};
  chevent.data.ptr = (void *)callback_arg;
  chevent.events = EPOLLOUT | EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP;
  return epoll_ctl(evio_fd, EPOLL_CTL_ADD, fd, &chevent);
}

/**
Creates a timer file descriptor, system dependent.
*/
intptr_t evio_open_timer(void) {
#ifndef TFD_NONBLOCK
  intptr_t fd = timerfd_create(CLOCK_MONOTONIC, O_NONBLOCK);
  if (fd != -1) { /* make sure it's a non-blocking timer. */
#if defined(O_NONBLOCK)
    /* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
    int flags;
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
      flags = 0;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
      goto error;
#else
    /* no O_NONBLOCK, use the old way of doing it */
    static int flags = 1;
    if (ioctl(fd, FIOBIO, &flags) == -1)
      goto error;
#endif
  }
  return fd;
error:
  close(fd);
  return -1;
#else
  return timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
#endif
}

/**
Adds a timer file descriptor, so that callbacks will be called for it's events.
*/
intptr_t evio_add_timer(int fd, void *callback_arg,
                        unsigned long milliseconds) {
  struct epoll_event chevent = {.data.ptr = (void *)callback_arg,
                                .events = (EPOLLIN | EPOLLET)};
  struct itimerspec new_t_data;
  new_t_data.it_value.tv_sec = new_t_data.it_interval.tv_sec =
      milliseconds / 1000;
  new_t_data.it_value.tv_nsec = new_t_data.it_interval.tv_nsec =
      (milliseconds % 1000) * 1000000;
  if (timerfd_settime(fd, 0, &new_t_data, NULL) == -1)
    return -1;
  int ret = epoll_ctl(evio_fd, EPOLL_CTL_ADD, fd, &chevent);
  if (ret == -1 && errno == EEXIST)
    return 0;
  return ret;
}

/** Rearms the timer. Required only by `epoll`.*/
void evio_reset_timer(int timer_fd) {
  char data[8]; // void * is 8 byte long
  if (read(timer_fd, &data, 8) < 0)
    data[0] = 0;
}

/**
Reviews any pending events (up to EVIO_MAX_EVENTS) and calls any callbacks.
 */
int evio_review(const int timeout_millisec) {
  if (evio_fd < 0)
    return -1;
  struct epoll_event events[EVIO_MAX_EVENTS];
  /* wait for events and handle them */
  int active_count =
      epoll_wait(evio_fd, events, EVIO_MAX_EVENTS, timeout_millisec);

  if (active_count > 0) {
    for (int i = 0; i < active_count; i++) {
      if (events[i].events & (~(EPOLLIN | EPOLLOUT))) {
        // errors are hendled as disconnections (on_close)
        evio_on_error(events[i].data.ptr);
      } else {
        // no error, then it's an active event(s)
        if (events[i].events & EPOLLOUT) {
          evio_on_ready(events[i].data.ptr);
        }
        if (events[i].events & EPOLLIN)
          evio_on_data(events[i].data.ptr);
      }
    } // end for loop
  } else if (active_count < 0) {
    return -1;
  }
  return active_count;
}

#else
/* *****************************************************************************
BSD `kqueue` implementation
***************************************************************************** */
#include <sys/event.h>

/**
Creates the `epoll` or `kqueue` object.
*/
intptr_t evio_create() { return evio_fd = kqueue(); }

/**
Removes a file descriptor from the polling object.
*/
int evio_remove(int fd) {
  struct kevent chevent[3];
  EV_SET(chevent, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
  EV_SET(chevent + 1, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
  EV_SET(chevent + 2, fd, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
  return kevent(evio_fd, chevent, 3, NULL, 0, NULL);
}

/**
Adds a file descriptor to the polling object.
*/
int evio_add(int fd, void *callback_arg) {
  struct kevent chevent[2];
  EV_SET(chevent, fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0,
         callback_arg);
  EV_SET(chevent + 1, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0,
         callback_arg);
  return kevent(evio_fd, chevent, 2, NULL, 0, NULL);
}

/**
Creates a timer file descriptor, system dependent.
*/
intptr_t evio_open_timer() {
#ifdef P_tmpdir
  char template[] = P_tmpdir "evio_facil_timer_XXXXXX";
#else
  char template[] = "/tmp/evio_facil_timer_XXXXXX";
#endif
  return mkstemp(template);
}

/**
Adds a timer file descriptor, so that callbacks will be called for it's events.
*/
intptr_t evio_add_timer(int fd, void *callback_arg,
                        unsigned long milliseconds) {
  struct kevent chevent;
  EV_SET(&chevent, fd, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, milliseconds,
         callback_arg);
  return kevent(evio_fd, &chevent, 1, NULL, 0, NULL);
}

/** Rearms the timer. Required only by `epoll`.*/
void evio_reset_timer(int timer_fd) { (void)timer_fd; }

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
      if (events[i].flags & (EV_EOF | EV_ERROR)) {
        // errors are hendled as disconnections (on_close)
        evio_on_error(events[i].udata);
      } else {
        // no error, then it's an active event(s)
        if (events[i].filter == EVFILT_WRITE) {
          evio_on_ready(events[i].udata);
        }
        if (events[i].filter == EVFILT_READ || events[i].filter == EVFILT_TIMER)
          evio_on_data(events[i].udata);
      }
    } // end for loop
  } else if (active_count < 0) {
    if (errno == EINTR)
      return 0;
    return -1;
  }
  return active_count;
}

#endif /* system dependent code */
