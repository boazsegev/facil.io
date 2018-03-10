/*
Copyright: Boaz Segev, 2016-2017
License: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "evio.h"

#ifdef EVIO_ENGINE_EPOLL

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

/** override this if not using facil.io, also change event loop */
#ifndef sock_uuid2fd
#define sock_uuid2fd(uuid) ((intptr_t)((uintptr_t)uuid >> 8))
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

/* *****************************************************************************
Global data and system independant code
***************************************************************************** */

/* epoll tester, in and out */
static int evio_fd[3] = {-1, -1, -1};

/** Closes the `epoll` / `kqueue` object, releasing it's resources. */
void evio_close() {
  for (int i = 0; i < 3; ++i) {
    if (evio_fd[i] != -1) {
      close(evio_fd[i]);
      evio_fd[i] = -1;
    }
  }
}

/**
returns true if the evio is available for adding or removing file descriptors.
*/
int evio_isactive(void) { return evio_fd[0] >= 0; }

/* *****************************************************************************
Linux `epoll` implementation
***************************************************************************** */
#include <sys/epoll.h>
#include <sys/timerfd.h>

/**
Creates the `epoll` or `kqueue` object.
*/
intptr_t evio_create() {
  evio_close();
  for (int i = 0; i < 3; ++i) {
    evio_fd[i] = epoll_create1(EPOLL_CLOEXEC);
    if (evio_fd[i] == -1)
      goto error;
  }
  for (int i = 1; i < 3; ++i) {
    struct epoll_event chevent = {
        .events = (EPOLLOUT | EPOLLIN), .data.fd = evio_fd[i],
    };
    if (epoll_ctl(evio_fd[0], EPOLL_CTL_ADD, evio_fd[i], &chevent) == -1)
      goto error;
  }
  return 0;
error:
#if DEBUB
  perror("ERROR: (evoid) failed to initialize");
#endif
  evio_close();
  return -1;
}

/**
Removes a file descriptor from the polling object.
*/
void evio_remove(int fd) {
  if (evio_fd[0] < 0)
    return;
  struct epoll_event chevent = {.events = (EPOLLOUT | EPOLLIN), .data.fd = fd};
  epoll_ctl(evio_fd[1], EPOLL_CTL_DEL, fd, &chevent);
  epoll_ctl(evio_fd[2], EPOLL_CTL_DEL, fd, &chevent);
}

static inline int evio_add2(int fd, void *callback_arg, uint32_t events,
                            int ep_fd) {
  struct epoll_event chevent;
  errno = 0;
  chevent = (struct epoll_event){
      .events = events, .data.ptr = (void *)callback_arg,
  };
  int ret = epoll_ctl(ep_fd, EPOLL_CTL_MOD, fd, &chevent);
  if (ret == -1 && errno == ENOENT) {
    errno = 0;
    chevent = (struct epoll_event){
        .events = events, .data.ptr = (void *)callback_arg,
    };
    ret = epoll_ctl(ep_fd, EPOLL_CTL_ADD, fd, &chevent);
  }
  return ret;
}

/**
Adds a file descriptor to the polling object.
*/
int evio_add(int fd, void *callback_arg) {
  if (evio_add2(fd, callback_arg,
                (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                evio_fd[1]) == -1)
    return -1;
  if (evio_add2(fd, callback_arg,
                (EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                evio_fd[2]) == -1)
    return -1;
  return 0;
}

/**
Adds a file descriptor to the polling object (ONE SHOT), to be polled for
incoming data (`evio_on_data` wil be called).
*/
int evio_add_read(int fd, void *callback_arg) {
  return evio_add2(fd, callback_arg,
                   (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                   evio_fd[1]);
}

/**
Adds a file descriptor to the polling object (ONE SHOT), to be polled for
outgoing buffer readiness data (`evio_on_ready` wil be called).
*/
int evio_add_write(int fd, void *callback_arg) {
  return evio_add2(fd, callback_arg,
                   (EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                   evio_fd[2]);
}

/**
Creates a timer file descriptor, system dependent.
*/
int evio_open_timer(void) {
#ifndef TFD_NONBLOCK
  int fd = timerfd_create(CLOCK_MONOTONIC, O_NONBLOCK);
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
int evio_set_timer(int fd, void *callback_arg, unsigned long milliseconds) {

  if (evio_fd[0] < 0)
    return -1;
  /* clear out existing timer marker, if exists. */
  char data[8]; // void * is 8 byte long
  if (read(fd, &data, 8) < 0)
    data[0] = 0;
  /* set file's time value */
  struct itimerspec new_t_data;
  new_t_data.it_value.tv_sec = new_t_data.it_interval.tv_sec =
      milliseconds / 1000;
  new_t_data.it_value.tv_nsec = new_t_data.it_interval.tv_nsec =
      (milliseconds % 1000) * 1000000;
  if (timerfd_settime(fd, 0, &new_t_data, NULL) == -1)
    return -1;
  /* add to epoll */
  return evio_add2(fd, callback_arg, (EPOLLIN | EPOLLONESHOT), evio_fd[1]);
}

/**
Reviews any pending events (up to EVIO_MAX_EVENTS) and calls any callbacks.
 */
int evio_review(const int timeout_millisec) {
  if (evio_fd[0] < 0)
    return -1;
  struct epoll_event internal[2];
  struct epoll_event events[EVIO_MAX_EVENTS];
  int total = 0;
  /* wait for events and handle them */
  int internal_count = epoll_wait(evio_fd[0], internal, 2, timeout_millisec);
  if (internal_count == -1)
    return -1;
  if (internal_count == 0)
    return 0;
  for (int i = 0; i < internal_count; ++i) {
    int active_count =
        epoll_wait(internal[i].data.fd, events, EVIO_MAX_EVENTS, 0);
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
      total += active_count;
    }
  }

  return total;
}

#include <poll.h>

/** Waits up to `timeout_millisec` for events. No events are signaled. */
int evio_wait(const int timeout_millisec) {
  if (evio_fd[0] < 0)
    return -1;
  struct pollfd pollfd = {
      .fd = evio_fd[0], .events = POLLIN,
  };
  return poll(&pollfd, 1, timeout_millisec);
}

#endif /* system dependent code */
