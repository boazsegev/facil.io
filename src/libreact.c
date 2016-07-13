/*
copyright: Boaz segev, 2016
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "libreact.h"

#if !defined(__linux__) && !defined(__CYGWIN__)
#include <sys/event.h>
#else
#include <sys/timerfd.h>
#include <sys/epoll.h>
#endif
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <sys/resource.h>

/* The (sadly, global) reactor fd */
static int reactor_fd = -1;

/* an inner helper function that removes and adds events */

/* *****************************************************************************
Callbacks
*/

#pragma weak reactor_on_close
void reactor_on_close(intptr_t uuid) {}

#pragma weak reactor_on_data
void reactor_on_data(intptr_t uuid) {
  char data[1024];
  while (read(sock_uuid2fd(uuid), data, 1024) > 0)
    ;
}

#pragma weak reactor_on_ready
void reactor_on_ready(intptr_t uuid) {}

/* *****************************************************************************
Integrate the `libsock` library if exists.
*/

#pragma weak sock_flush
ssize_t sock_flush(intptr_t uuid) {
  return 0;
}

#pragma weak sock_close
void sock_close(intptr_t uuid) {
  shutdown(sock_uuid2fd(uuid), SHUT_RDWR);
  close(sock_uuid2fd(uuid));
  /* this is automatic on epoll... what about kqueue? */
  reactor_remove(uuid);
  /* call callback */
  reactor_on_close(uuid);
}

/* *****************************************************************************
The main libreact API.
*/

/**
Closes a file descriptor, calling it's callback if it was registered with the
reactor.
*/
void reactor_close(intptr_t uuid) {
  sock_close(uuid);
  return;
}

/* *****************************************************************************
KQueue implementation.
*/
#ifdef EV_SET

int reactor_add(intptr_t uuid) {
  struct kevent chevent[2];
  EV_SET(chevent, sock_uuid2fd(uuid), EVFILT_READ,
         EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, (void*)uuid);
  EV_SET(chevent + 1, sock_uuid2fd(uuid), EVFILT_WRITE,
         EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, (void*)uuid);
  return kevent(reactor_fd, chevent, 2, NULL, 0, NULL);
}
int reactor_add_timer(intptr_t uuid, long milliseconds) {
  struct kevent chevent;
  EV_SET(&chevent, sock_uuid2fd(uuid), EVFILT_TIMER, EV_ADD | EV_ENABLE, 0,
         milliseconds, (void*)uuid);
  return kevent(reactor_fd, &chevent, 1, NULL, 0, NULL);
}

int reactor_remove(intptr_t uuid) {
  struct kevent chevent[2];
  EV_SET(chevent, sock_uuid2fd(uuid), EVFILT_READ, EV_DELETE, 0, 0,
         (void*)uuid);
  EV_SET(chevent + 1, sock_uuid2fd(uuid), EVFILT_WRITE, EV_DELETE, 0, 0,
         (void*)uuid);
  return kevent(reactor_fd, chevent, 2, NULL, 0, NULL);
}
int reactor_remove_timer(intptr_t uuid) {
  struct kevent chevent;
  EV_SET(&chevent, sock_uuid2fd(uuid), EVFILT_TIMER, EV_DELETE, 0, 0,
         (void*)uuid);
  return kevent(reactor_fd, &chevent, 1, NULL, 0, NULL);
}

/**
epoll requires the timer to be "reset" before repeating. Kqueue requires no such
thing.

This method promises that the timer will be repeated when running on epoll. This
method is redundent on kqueue.
*/
void reactor_reset_timer(intptr_t uuid) {
/* EPoll only */
#ifdef EPOLLIN
  char data[8];  // void * is 8 byte long
  if (read(sock_uuid2fd(uuid), &data, 8) < 0)
    data[0] = 0;
#endif
}
/**
Creates a timer file descriptor, system dependent.

Opens a new file decriptor for creating timer events. On BSD this will revert to
an `fileno(tmpfile())` (with error handling) and on Linux it will call
`timerfd_create(CLOCK_MONOTONIC, ...)`.

Returns -1 on error, otherwise returns the file descriptor.
*/
intptr_t reactor_make_timer() {
#ifdef P_tmpdir
  char template[] = P_tmpdir "libreact_timer_XXXXXX";
#else
  char template[] = "/tmp/libreact_timer_XXXXXX";
#endif
  return mkstemp(template);
}

/* *****************************************************************************
EPoll implementation.
*/
#elif defined(EPOLLIN)

int reactor_add(intptr_t uuid) {
  struct epoll_event chevent;
  chevent.data.ptr = (void*)uuid;
  chevent.events =
      EPOLLOUT | EPOLLIN | EPOLLET | EPOLLERR | EPOLLRDHUP | EPOLLHUP;
  return epoll_ctl(reactor_fd, EPOLL_CTL_ADD, sock_uuid2fd(uuid), &chevent);
}
int reactor_add_timer(intptr_t uuid, long milliseconds) {
  struct epoll_event chevent;
  chevent.data.ptr = (void*)uuid;
  chevent.events =
      EPOLLOUT | EPOLLIN | EPOLLET | EPOLLERR | EPOLLRDHUP | EPOLLHUP;
  struct itimerspec new_t_data;
  new_t_data.it_value.tv_sec = new_t_data.it_interval.tv_sec =
      milliseconds / 1000;
  new_t_data.it_value.tv_nsec = new_t_data.it_interval.tv_nsec =
      (milliseconds % 1000) * 1000000;
  timerfd_settime(sock_uuid2fd(uuid), 0, &new_t_data, NULL);
  return epoll_ctl(reactor_fd, EPOLL_CTL_ADD, sock_uuid2fd(uuid), &chevent);
}

int reactor_remove(intptr_t uuid) {
  struct epoll_event chevent;
  chevent.data.ptr = (void*)uuid;
  chevent.events =
      EPOLLOUT | EPOLLIN | EPOLLET | EPOLLERR | EPOLLRDHUP | EPOLLHUP;
  return epoll_ctl(reactor_fd, EPOLL_CTL_DEL, sock_uuid2fd(uuid), &chevent);
}
int reactor_remove_timer(intptr_t uuid) {
  struct epoll_event chevent;
  chevent.data.ptr = (void*)uuid;
  chevent.events =
      EPOLLOUT | EPOLLIN | EPOLLET | EPOLLERR | EPOLLRDHUP | EPOLLHUP;
  return epoll_ctl(reactor_fd, EPOLL_CTL_DEL, sock_uuid2fd(uuid), &chevent);
}

/**
epoll requires the timer to be "reset" before repeating. Kqueue requires no such
thing.

This method promises that the timer will be repeated when running on epoll. This
method is redundent on kqueue.
*/
void reactor_reset_timer(intptr_t uuid) {
  char data[8];  // void * is 8 byte long
  if (read(sock_uuid2fd(uuid), &data, 8) < 0)
    data[0] = 0;
}
/**
Creates a timer file descriptor, system dependent.

Opens a new file decriptor for creating timer events. On BSD this will revert to
an `fileno(tmpfile())` (with error handling) and on Linux it will call
`timerfd_create(CLOCK_MONOTONIC, ...)`.

Returns -1 on error, otherwise returns the file descriptor.
*/
intptr_t reactor_make_timer() {
  return timerfd_create(CLOCK_MONOTONIC, O_NONBLOCK);
  ;
}

/* *****************************************************************************
This library requires either kqueue or epoll to be available.
Please help us be implementing support for your OS.
*/
#else
int reactor_add(intptr_t uuid) {
  return -1;
}
int reactor_add_timer(intptr_t uuid, long milliseconds) {
  return -1;
}

int reactor_remove(intptr_t uuid) {
  return -1;
}
int reactor_remove_timer(intptr_t uuid) {
  return -1;
}
#error(This library requires either kqueue or epoll to be available. Please help us be implementing support for your OS.)
#endif

/* *****************************************************************************
The Reactor loop.
*/

/*
define some macros to help us write a cleaner main function.
*/

/* KQueue */
#ifdef EV_SET
/* global timout value for the reactors */
static struct timespec _reactor_timeout = {
    .tv_sec = (REACTOR_TICK / 1000),
    .tv_nsec = ((REACTOR_TICK % 1000) * 1000000)};
#define _CRAETE_QUEUE_ kqueue()
#define _EVENT_TYPE_ struct kevent
// #define _EVENTS_ ((_EVENT_TYPE_*)(reactor->internal_data.events))
#define _WAIT_FOR_EVENTS_(events) \
  kevent(reactor_fd, NULL, 0, (events), REACTOR_MAX_EVENTS, &_reactor_timeout);
#define _GET_FDUUID_(events, _ev_) ((intptr_t)((events)[(_ev_)].udata))
#define _EVENTERROR_(events, _ev_) (events)[(_ev_)].flags&(EV_EOF | EV_ERROR)
#define _EVENTREADY_(events, _ev_) (events)[(_ev_)].filter == EVFILT_WRITE
#define _EVENTDATA_(events, _ev_)           \
  (events)[(_ev_)].filter == EVFILT_READ || \
      (events)[(_ev_)].filter == EVFILT_TIMER

/* EPoll */
#elif defined(EPOLLIN)
static int _reactor_timeout = REACTOR_TICK;
#define _CRAETE_QUEUE_ epoll_create1(0)
#define _EVENT_TYPE_ struct epoll_event
// #define _EVENTS_ ((_EVENT_TYPE_*)reactor->internal_data.events)
#define _QUEUE_READY_FLAG_ EPOLLOUT
#define _WAIT_FOR_EVENTS_(events) \
  epoll_wait(reactor_fd, (events), REACTOR_MAX_EVENTS, _reactor_timeout)
#define _GET_FDUUID_(events, _ev_) ((intptr_t)((events)[(_ev_)].data.ptr))
#define _EVENTERROR_(events, _ev_) \
  ((events)[(_ev_)].events & (~(EPOLLIN | EPOLLOUT)))
#define _EVENTREADY_(_events, _ev_) (_events)[(_ev_)].events& EPOLLOUT
#define _EVENTDATA_(_events, _ev_) (_events)[(_ev_)].events& EPOLLIN

#else /* no epoll, no kqueue - this is where support ends */
#error(This library requires either kqueue or epoll to be available. Please help us be implementing support for your OS.)
#endif

/**
Initializes the reactor, making the reactor "live".

Returns -1 on error, otherwise returns 0.
*/
ssize_t reactor_init() {
  if (reactor_fd > 0)
    return -1;
  reactor_fd = _CRAETE_QUEUE_;
  return 0;
}

/**
Closes the reactor, releasing it's resources (except the actual struct Reactor,
which might have been allocated on the stack and should be handled by the
caller).
*/
void reactor_stop() {
  close(reactor_fd);
  reactor_fd = -1;
}

/* *****************************************************************************
The main Reactor Review function
*/

/**
Reviews any pending events (up to REACTOR_MAX_EVENTS)

Returns -1 on error, otherwise returns 0.
*/
int reactor_review() {
  if (reactor_fd < 0)
    return -1;
  _EVENT_TYPE_ events[REACTOR_MAX_EVENTS];

  /* wait for events and handle them */
  int active_count = _WAIT_FOR_EVENTS_(events);
  if (active_count > 0) {
    for (int i = 0; i < active_count; i++) {
      if (_EVENTERROR_(events, i)) {
        // errors are hendled as disconnections (on_close)
        // printf("on_close for %lu\n", _GET_FDUUID_(events, i));
        sock_close(_GET_FDUUID_(events, i));
      } else {
        // no error, then it's an active event(s)
        if (_EVENTREADY_(events, i)) {
          // printf("on_ready for %lu\n", _GET_FDUUID_(events, i));
          if (sock_flush(_GET_FDUUID_(events, i)) < 0) {
            sock_close(_GET_FDUUID_(events, i));
          } else {
            // printf("on_ready callback for %lu fd(%d)\n",
            //        _GET_FDUUID_(events, i),
            //        sock_uuid2fd(_GET_FDUUID_(events, i)));
            reactor_on_ready(_GET_FDUUID_(events, i));
          }
        }
        if (_EVENTDATA_(events, i)) {
          // printf("on_data callback for %lu fd(%d)\n",
          //        _GET_FDUUID_(events, i),
          //        sock_uuid2fd(_GET_FDUUID_(events, i)));
          reactor_on_data(_GET_FDUUID_(events, i));
        }
      }
    }  // end for loop
  } else if (active_count < 0) {
    // perror("Please close the reactor, it's dying...");
    return -1;
  }
  return active_count;
}

// we're done with these
#undef _CRAETE_QUEUE_
#undef _EVENT_TYPE_
#undef _EVENTS
#undef _INIT_TIMEOUT_
#undef _WAIT_FOR_EVENTS_
#undef _GET_FDUUID_
#undef _EVENTERROR_
#undef _EVENTREADY_
#undef _EVENTDATA_
