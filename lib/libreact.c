/*
copyright: Boaz segev, 2015
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

// an inner helper function that removes and adds events
#ifdef EV_SET
#define ADD_FD EV_ADD | EV_ENABLE | EV_CLEAR
#define RM_FD EV_DELETE
#define ADD_TM EVFILT_TIMER
static inline int _reactor_set_fd_polling_(int queue,
                                           int fd,
                                           u_short action,
                                           long milliseconds) {
  if (milliseconds) {
    struct kevent chevent;
    EV_SET(&chevent, fd, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, milliseconds, 0);
    return kevent(queue, &chevent, 1, NULL, 0, NULL);
  } else {
    struct kevent chevent[2];
    EV_SET(chevent, fd, EVFILT_READ, action, 0, 0, 0);
    EV_SET(chevent + 1, fd, EVFILT_WRITE, action, 0, 0, 0);
    return kevent(queue, chevent, 2, NULL, 0, NULL);
  }
  // EV_SET(&chevent, fd, (milliseconds ? EVFILT_TIMER : EVFILT_READ), action,
  // 0,
  //        milliseconds, 0);
}
/////////////////////
// EPoll
#elif defined(EPOLLIN)
#define ADD_FD EPOLL_CTL_ADD
#define RM_FD EPOLL_CTL_DEL
#define ADD_TM EPOLL_CTL_ADD
static inline int _reactor_set_fd_polling_(int queue,
                                           int fd,
                                           int action,
                                           long milliseconds) {
  struct epoll_event chevent;
  chevent.data.fd = fd;
  chevent.events =
      EPOLLOUT | EPOLLIN | EPOLLET | EPOLLERR | EPOLLRDHUP | EPOLLHUP;
  if (milliseconds) {
    struct itimerspec new_t_data;
    new_t_data.it_value.tv_sec = new_t_data.it_interval.tv_sec =
        milliseconds / 1000;
    new_t_data.it_value.tv_nsec = new_t_data.it_interval.tv_nsec =
        (milliseconds % 1000) * 1000000;
    timerfd_settime(fd, 0, &new_t_data, NULL);
  }
  return epoll_ctl(queue, action, fd, &chevent);
}
////////////////////////
// no epoll, no kqueue - this aint no server platform!
#else
static inline int _reactor_set_fd_poling_(int queue, int fd, int flags) {
  return -1;
}
#error(no epoll, no kqueue - this aint no server platform! ... this library requires either kqueue or epoll to be available.)
#endif

/**
Adds a file descriptor to the reactor, so that callbacks will be called for it's
events.

Returns -1 on error, otherwise return value is system dependent.
*/
int reactor_add(struct Reactor* reactor, int fd) {
  assert(reactor->private.reactor_fd);
  assert(reactor->maxfd >= fd);
  // don't make sure that the `on_close` callback was called,
  // as it's likely that the caller already mapped a new object to the fd.
  // this would be the clien't responsability.
  reactor->private.map[fd] = 1;
  return _reactor_set_fd_polling_(reactor->private.reactor_fd, fd, ADD_FD, 0);
}
/**
Adds a file descriptor as a timer object.

Returns -1 on error, otherwise return value is system dependent.
*/
int reactor_add_timer(struct Reactor* reactor, int fd, long milliseconds) {
  assert(reactor->private.reactor_fd);
  assert(reactor->maxfd >= fd);
  reactor->private.map[fd] = 1;
  return _reactor_set_fd_polling_(reactor->private.reactor_fd, fd, ADD_TM,
                                  milliseconds);
}

/**
Removes a file descriptor from the reactor - further callbacks won't be called.

Returns -1 on error, otherwise return value is system dependent. If the file
descriptor wasn't owned by
the reactor, it isn't an error.
*/
int reactor_remove(struct Reactor* reactor, int fd) {
  assert(reactor->private.reactor_fd);
  assert(reactor->maxfd >= fd);
  // don't make sure that the `on_close` callback was called,
  // as it's likely that the caller already mapped a new object to the fd.
  // this would be the clien't responsability.
  reactor->private.map[fd] = 0;
  return _reactor_set_fd_polling_(reactor->private.reactor_fd, fd, RM_FD, 0);
}
/**
Closes a file descriptor, calling it's callback if it was registered with the
reactor.
*/
void reactor_close(struct Reactor* reactor, int fd) {
  assert(reactor->maxfd >= fd);
  static pthread_mutex_t locker = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&locker);
  if (reactor->private.map[fd]) {
    close(fd);
    reactor->private.map[fd] = 0;
    pthread_mutex_unlock(&locker);
    if (reactor->on_close)
      reactor->on_close(reactor, fd);
    // this is automatic on epoll... what about kqueue?
    _reactor_set_fd_polling_(reactor->private.reactor_fd, fd, RM_FD, 0);
  }
  pthread_mutex_unlock(&locker);
}

/**
epoll requires the timer to be "reset" before repeating. Kqueue requires no such
thing.

This method promises that the timer will be repeated when running on epoll. This
method is redundent on kqueue.
*/
void reactor_reset_timer(int fd) {
#ifdef EPOLLIN   // KQueue
  char data[8];  // void * is 8 byte long
  if (read(fd, &data, 8) < 0)
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
int reactor_make_timer(void) {
#ifdef EV_SET  // KQueue
  FILE* tmp = tmpfile();
  return tmp ? fileno(tmp) : -1;
#elif defined(EPOLLIN)  // EPoll
  return timerfd_create(CLOCK_MONOTONIC, O_NONBLOCK);
#else                   // no epoll, no kqueue - this aint no serverplatform!
#error(no epoll, no kqueue - this aint no server platform! ... this library requires either kqueue or epoll to be available.)
  return -1;  // until we learn more about the implementation
#endif
}

// undefine the macro helpers we're not using anymore
#undef ADD_FD
#undef RM_FD
#undef ADD_TM

///////////////////
// define some macros to help us write a cleaner main function.
#ifdef EV_SET  // KQueue

static struct timespec _reactor_timeout = {
    .tv_sec = (REACTOR_TICK / 1000),
    .tv_nsec = ((REACTOR_TICK % 1000) * 1000000)};

#define _CRAETE_QUEUE_ kqueue()
#define _EVENT_TYPE_ struct kevent
#define _EVENTS_ ((_EVENT_TYPE_*)reactor->private.events)
#define _WAIT_FOR_EVENTS_                                                    \
  kevent(reactor->private.reactor_fd, NULL, 0, _EVENTS_, REACTOR_MAX_EVENTS, \
         &_reactor_timeout);
#define _GETFD_(_ev_) _EVENTS_[(_ev_)].ident
#define _EVENTERROR_(_ev_) (_EVENTS_[(_ev_)].flags & (EV_EOF | EV_ERROR))
#define _EVENTREADY_(_ev_) (_EVENTS_[(_ev_)].filter == EVFILT_WRITE)
#define _EVENTDATA_(_ev_)                    \
  (_EVENTS_[(_ev_)].filter == EVFILT_READ || \
   _EVENTS_[(_ev_)].filter == EVFILT_TIMER)

#elif defined(EPOLLIN)  // EPoll

static int _reactor_timeout = REACTOR_TICK;

#define _CRAETE_QUEUE_ epoll_create1(0)
#define _EVENT_TYPE_ struct epoll_event
#define _EVENTS_ ((_EVENT_TYPE_*)reactor->private.events)
#define _QUEUE_READY_FLAG_ EPOLLOUT
#define _WAIT_FOR_EVENTS_                                               \
  epoll_wait(reactor->private.reactor_fd, _EVENTS_, REACTOR_MAX_EVENTS, \
             _reactor_timeout)
#define _GETFD_(_ev_) _EVENTS_[(_ev_)].data.fd
#define _EVENTERROR_(_ev_) (_EVENTS_[(_ev_)].events & (~(EPOLLIN | EPOLLOUT)))
#define _EVENTREADY_(_ev_) (_EVENTS_[(_ev_)].events & EPOLLOUT)
#define _EVENTDATA_(_ev_) (_EVENTS_[(_ev_)].events & EPOLLIN)

#else  // no epoll, no kqueue - this aint no server platform!
#error(no epoll, no kqueue - this aint no server platform! ... this library
requires either kqueue or epoll to be available.)
#endif

static void reactor_destroy(struct Reactor* reactor) {
  if (reactor->private.map)
    free(reactor->private.map);
  if (reactor->private.events)
    free(reactor->private.events);
  if (reactor->private.reactor_fd)
    close(reactor->private.reactor_fd);
  reactor->private.map = NULL;
  reactor->private.events = NULL;
  reactor->private.reactor_fd = 0;
}
/**
Initializes the reactor, making the reactor "live".

Returns -1 on error, otherwise returns 0.
*/
int reactor_init(struct Reactor* reactor) {
  if (reactor->maxfd <= 0)
    return -1;
  reactor->private.reactor_fd = _CRAETE_QUEUE_;
  reactor->private.map = calloc(1, reactor->maxfd + 1);
  reactor->private.events = calloc(sizeof(_EVENT_TYPE_), REACTOR_MAX_EVENTS);
  if (!reactor->private.reactor_fd || !reactor->private.map ||
      !reactor->private.events) {
    reactor_destroy(reactor);
    return -1;
  }
  return 0;
}

/**
Closes the reactor, releasing it's resources (except the actual struct Reactor,
which might have been allocated on the stack and should be handled by the
caller).
*/
void reactor_stop(struct Reactor* reactor) {
  if (!reactor->private.map || !reactor->private.reactor_fd)
    return;
  for (int i = 0; i <= reactor->maxfd; i++) {
    if (reactor->private.map[i]) {
      if (reactor->on_shutdown)
        reactor->on_shutdown(reactor, i);
      reactor_close(reactor, i);
    }
  }
  reactor_destroy(reactor);
}

/**
Reviews any pending events (up to REACTOR_MAX_EVENTS)

Returns -1 on error, otherwise returns 0.
*/
int reactor_review(struct Reactor* reactor) {
  if (!reactor->private.reactor_fd)
    return -1;

  // set the last tick
  time(&reactor->last_tick);
  // will be used to get the actual events in the
  // current queue cycle
  int active_count = 0;

  // // review locally closed connections (do we do this?)
  // int close_events = 0;
  // for (int i = 3; i <= reactor->maxfd; i++) {
  //   if (reactor->private.map[i]) {
  //     if (fcntl(i, F_GETFL) < 0 && errno == EBADF) {
  //       close_events++;
  //       reactor_close(reactor, i);
  //     }
  //   }
  // }
  // wait for events and handle them
  active_count = _WAIT_FOR_EVENTS_;
  if (active_count > 0) {
    for (int i = 0; i < active_count; i++) {
      if (_EVENTERROR_(i)) {
        // errors are hendled as disconnections (on_close)
        reactor_close(reactor, _GETFD_(i));
      } else {
        // no error, then it's an active event(s)
        if (_EVENTREADY_(i) && reactor->on_ready) {
          // printf("on_ready for %d\n", _GETFD_(i));
          reactor->on_ready(reactor, _GETFD_(i));
        }
        if (_EVENTDATA_(i) && reactor->on_data) {
          // printf("on_data %d\n", _GETFD_(i));
          reactor->on_data(reactor, _GETFD_(i));
        }
      }
    }  // end for loop
  } else if (active_count < 0) {
    // perror("closing reactor");
    return -1;
  }
  return active_count;  // + close_events;
}

// we're done with these
#undef _CRAETE_QUEUE_
#undef _EVENT_TYPE_
#undef _EVENTS
#undef _INIT_TIMEOUT_
#undef _WAIT_FOR_EVENTS_
#undef _GETFD_
#undef _EVENTERROR_
#undef _EVENTREADY_
#undef _EVENTDATA_
