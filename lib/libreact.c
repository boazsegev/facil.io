/*
copyright: Boaz segev, 2015
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#include "libreact.h"

// #include <stdlib.h>
// #include <stdio.h>
// #include <time.h>
// #include <unistd.h>
// #include <signal.h>
// #include <limits.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <sys/un.h>

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
static inline int _reactor_set_fd_pooling_(int queue, int fd, int flags) {
  return -1;
}
#error(no epoll, no kqueue - this aint no server platform! ... this library requires either kqueue or epoll to be available.)
#endif

// // performing a hook is a bad design idea. This should be handled by the
// // client, who probably already mapped the incoming fd to a new object/
// //
// // a helper function that ensures the `on_close` was called and sets the
// // flag for the connection
// static inline void _reactor_on_add_hook_(struct ReactorSettings *reactor, int
// fd) {
//   if(  1 == reactor->private.map[fd] ) {
//     // perform hook
//   }
//   reactor->private.map[fd] = 1;
// }

// an instance function that will gracefully shut down the reactor.
static inline void _reactor_stop_(struct ReactorSettings* reactor) {
  reactor->private.run = 0;
}
// adds a file descriptor to the reactor
static inline int _reactor_add_(struct ReactorSettings* reactor, int fd) {
  assert(reactor->private.reactor_fd);
  assert(reactor->last > fd);
  // don't make sure that the `on_close` callback was called,
  // as it's likely that the caller already mapped a new object to the fd.
  // this would be the clien't responsability.
  reactor->private.map[fd] = 1;
  return _reactor_set_fd_polling_(reactor->private.reactor_fd, fd, ADD_FD, 0);
}
// adds a special file descriptor to the reactor, setting it up as a timer on
// kqueue/epoll. might fail if future versions support `select`.
//
// this might require (on linux) a `timerfd_create` call.
static inline int _reactor_add_as_timer_(struct ReactorSettings* reactor,
                                         int fd,
                                         long milliseconds) {
  assert(reactor->private.reactor_fd);
  assert(reactor->last > fd);
  reactor->private.map[fd] = 1;
  return _reactor_set_fd_polling_(reactor->private.reactor_fd, fd, ADD_TM,
                                  milliseconds);
}
// removes a file descriptor from the reactor without calling and callbacks.
static inline int _reactor_hijack_(struct ReactorSettings* reactor, int fd) {
  assert(reactor->private.reactor_fd);
  assert(reactor->last > fd);
  reactor->private.map[fd] = 0;
  return _reactor_set_fd_polling_(reactor->private.reactor_fd, fd, RM_FD, 0);
}
// Opens a new file decriptor for creating timer events. On BSD this will revert
// to an `fileno(tmpfile())` (with error handling) and on Linux it will call
// `timerfd_create(CLOCK_MONOTONIC, ...)`.
static inline int _reactor_open_timer_fd_() {
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
static inline void _reactor_reset_timer(struct ReactorSettings* selt, int fd) {
  char data[8];  // void * is 8 byte long
  if (read(fd, &data, 8) < 0)
    data[0] = 0;
}

// undefine the macro helpers we're not using anymore
#undef ADD_FD
#undef RM_FD
#undef ADD_TM

// an instance function that will gracefully shut down the reactor.
static inline int _reactor_exists_(struct ReactorSettings* reactor, int fd) {
  return reactor->private.map[fd] ? fd : 0;
}

///////////////////
// define some macros to help us write a cleaner main function.
#ifdef EV_SET  // KQueue
#define _CRAETE_QUEUE_ kqueue()
#define _REMOVE_FROM_QUEUE_(reactor, sock) _reactor_hijack_(reactor, sock)
#define _EVENT_TYPE_ struct kevent
#define _INIT_TIMEOUT_                   \
  struct timespec timeout;               \
  timeout.tv_sec = reactor->tick / 1000; \
  timeout.tv_nsec = (reactor->tick % 1000) * 1000000;
#define _WAIT_FOR_EVENTS_                              \
  kevent(reactor->private.reactor_fd, NULL, 0, events, \
         reactor->event_per_cycle, &timeout);
#define _GETFD_(_ev_) events[(_ev_)].ident
#define _EVENTERROR_(_ev_) (events[(_ev_)].flags & (EV_EOF | EV_ERROR))
#define _EVENTREADY_(_ev_) (events[(_ev_)].filter == EVFILT_WRITE)
#define _EVENTDATA_(_ev_)                  \
  (events[(_ev_)].filter == EVFILT_READ || \
   events[(_ev_)].filter == EVFILT_TIMER)

#elif defined(EPOLLIN)  // EPoll
#define _CRAETE_QUEUE_ epoll_create1(0)
#define _REMOVE_FROM_QUEUE_(reactor, sock)  // no need. It's automated.
#define _EVENT_TYPE_ struct epoll_event
#define _INIT_TIMEOUT_ int timeout = reactor->tick
#define _QUEUE_READY_FLAG_ EPOLLOUT
#define _WAIT_FOR_EVENTS_                                                   \
  epoll_wait(reactor->private.reactor_fd, events, reactor->event_per_cycle, \
             timeout)
#define _GETFD_(_ev_) events[(_ev_)].data.fd
#define _EVENTERROR_(_ev_) (events[(_ev_)].events & (~(EPOLLIN | EPOLLOUT)))
#define _EVENTREADY_(_ev_) (events[(_ev_)].events & EPOLLOUT)
#define _EVENTDATA_(_ev_) (events[(_ev_)].events & EPOLLIN)

#else  // no epoll, no kqueue - this aint no server platform!
#error(no epoll, no kqueue - this aint no server platform! ... this library
requires either kqueue or epoll to be available.)
#endif

///////////////////
// the main function
int reactor_start(struct ReactorSettings* reactor) {
  reactor->add = _reactor_add_;
  reactor->hijack = _reactor_hijack_;
  reactor->add_as_timer = _reactor_add_as_timer_;
  reactor->open_timer_fd = _reactor_open_timer_fd_;
  reactor->reset_timer = _reactor_reset_timer;
  reactor->stop = _reactor_stop_;
  reactor->exists = _reactor_exists_;
  reactor->private.run = 1;
  if (reactor->tick == 0) {
    int* changer = (int*)&reactor->tick;
    *changer = 1000;
  } else if (reactor->tick < 0) {
    int* changer = (int*)&reactor->tick;
    *changer = 0;
  }
  if (reactor->event_per_cycle <= 0) {
    int* changer = (int*)&reactor->event_per_cycle;
    *changer = 64;
  }
  if (reactor->last <= 0) {
    int* changer = (int*)&reactor->last;
#ifdef _SC_OPEN_MAX
    *changer = sysconf(_SC_OPEN_MAX) - 1;
#elif defined(OPEN_MAX)
    *changer = OPEN_MAX - 1;
#else
    *changer = 0;
#endif
  }
  if (!reactor->last)
    return -1;

  // make a connection map and initialize it.
  char actv_conn_map[reactor->last + 1];
  for (int i = 0; i <= reactor->last; i++) {
    actv_conn_map[i] = 0;
  }
  reactor->private.map = actv_conn_map;
  // set timout object called `timeout`
  _INIT_TIMEOUT_;
  // set events cache
  _EVENT_TYPE_ events[reactor->event_per_cycle];
  // set queue fd
  {
    int* changer = (int*)&reactor->private.reactor_fd;
    *changer = _CRAETE_QUEUE_;
  }

  // did we get an error asking for the queue?
  if (reactor->private.reactor_fd < 0)
    return -1;

  // add the initial fd to the event poll
  if (reactor->first)
    if (reactor->add(reactor, reactor->first))
      return -1;
  // call the initialization callback
  if (reactor->on_init)
    reactor->on_init(reactor);
  // will be used to get the actual events in the
  // current queue cycle
  int active_count = 0;

  // start the loop...
  while (reactor->private.run) {
    // review any connection that might have been closed
    // by the user ... ? should we?
    for (int i = 3; i < reactor->last; i++) {
      if (actv_conn_map[i]) {
        if (fcntl(i, F_GETFL) < 0 && errno == EBADF) {
          actv_conn_map[i] = 0;
          // printf("locally closed %d\n", i);
          if (reactor->on_close)
            reactor->on_close(reactor, i);
          _REMOVE_FROM_QUEUE_(reactor, i);
        }
      }
    }
    reactor->last_tick = time(NULL);
    if (reactor->on_tick)
      reactor->on_tick(reactor);

    // wait for events and handle them
    active_count = _WAIT_FOR_EVENTS_;
    if (active_count > 0) {
      for (int i = 0; i < active_count; i++) {
        if (_EVENTERROR_(i)) {
          // errors are hendled as disconnections (on_close)
          // printf("remote closed %lu\n", _GETFD_(i));
          close(_GETFD_(i));
          _REMOVE_FROM_QUEUE_(reactor, _GETFD_(i));
          actv_conn_map[_GETFD_(i)] = 0;  // clear the flag
          if (reactor->on_close)
            reactor->on_close(reactor, _GETFD_(i));
          // printf("remote closed %lu handled\n", _GETFD_(i));
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
      goto finish;
    } else if (reactor->on_idle)
      reactor->on_idle(reactor);
  }

finish:
  for (int i = 0; i < reactor->last; i++) {
    if (actv_conn_map[i]) {
      if (reactor->on_shutdown)
        reactor->on_shutdown(reactor, i);
      close(i);
      if (reactor->on_close)
        reactor->on_close(reactor, i);
    }
  }
  close(reactor->private.reactor_fd);
  return 0;
}

// we're done with these
#undef _CRAETE_QUEUE_
#undef _REMOVE_FROM_QUEUE_
#undef _EVENT_TYPE_
#undef _INIT_TIMEOUT_
#undef _WAIT_FOR_EVENTS_
#undef _GETFD_
#undef _EVENTERROR_
#undef _EVENTREADY_
#undef _EVENTDATA_

//
// /////////////////////
//
// #ifdef EV_SET // KQueue
//
// #elif defined(EPOLLIN) // EPoll
//
// #else // this aint no server platform!
//
// #error(no epoll, no kqueue - this aint no server platform! ... this library
// requires either kqueue or epoll to be available.)
//
// #endif
//
// ab -n 100000 -c 200 -k http://127.0.0.1:3000/
// wrk -c4000 -d 50 -t12 http://localhost:3000/
