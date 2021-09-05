/* *****************************************************************************
IO Polling
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

/* *****************************************************************************
IO Polling settings
***************************************************************************** */

#ifndef FIO_POLL_MAX_EVENTS
/* The number of events to collect with each call to epoll or kqueue. */
#define FIO_POLL_MAX_EVENTS 96
#endif

/* *****************************************************************************
Polling Timeout Calculation
***************************************************************************** */

static int fio_poll_timeout_calc(void) {
  int t = fio_timer_next_at(&fio_data.timers) - fio_data.tick;
  if (t == -1 || t > FIO_POLL_TICK)
    t = FIO_POLL_TICK;
  return t;
}

/* *****************************************************************************
Polling Wrapper Tasks
***************************************************************************** */

FIO_IFUNC void fio___poll_ev_wrap__user_task(void *fn_, void *io_) {
  union {
    void (*fn)(void *, void *);
    void *p;
  } u = {.p = fn_};
  fio_s *io = io_;
  if (!fio_is_valid(io))
    goto io_invalid;
  MARK_FUNC();

  fio_queue_push(FIO_QUEUE_IO(io), .fn = u.fn, .udata1 = fio_dup(io));
  fio_user_thread_wake();
  return;
io_invalid:
  FIO_LOG_DEBUG("IO validation failed for IO %p (User task)", (void *)io);
}

FIO_IFUNC void fio___poll_ev_wrap__io_task(void *fn_, void *io_) {
  union {
    void (*fn)(void *, void *);
    void *p;
  } u = {.p = fn_};
  fio_s *io = io_;
  if (!fio_is_valid(io))
    goto io_invalid;
  MARK_FUNC();

  fio_queue_push(FIO_QUEUE_SYSTEM, .fn = u.fn, .udata1 = fio_dup(io));
  return;
io_invalid:
  FIO_LOG_DEBUG("IO validation failed for IO %p (IO task)", (void *)io);
}

/* delays callback scheduling so it occurs after pending free / init events. */
FIO_IFUNC void fio___poll_ev_wrap__schedule(void (*wrapper)(void *, void *),
                                            void (*fn)(void *, void *),
                                            void *io) {
  /* delay task processing to after all IO object references increased */
  union {
    void (*fn)(void *, void *);
    void *p;
  } u = {.fn = fn};
  fio_queue_push(FIO_QUEUE_SYSTEM, .fn = wrapper, .udata1 = u.p, .udata2 = io);
}

/* *****************************************************************************
Polling Callbacks
***************************************************************************** */

FIO_IFUNC void fio___poll_on_data(int fd, void *io) {
  (void)fd;
  // FIO_LOG_DEBUG("event on_data detected for IO %p", io);
  fio___poll_ev_wrap__schedule(fio___poll_ev_wrap__user_task,
                               fio_ev_on_data,
                               io);
  MARK_FUNC();
}
FIO_IFUNC void fio___poll_on_ready(int fd, void *io) {
  (void)fd;
  // FIO_LOG_DEBUG("event on_ready detected for IO %p", io);
  fio___poll_ev_wrap__schedule(fio___poll_ev_wrap__io_task,
                               fio_ev_on_ready,
                               io);
  MARK_FUNC();
}

FIO_IFUNC void fio___poll_on_close(int fd, void *io) {
  (void)fd;
  // FIO_LOG_DEBUG("event on_close detected for IO %p", io);
  fio___poll_ev_wrap__schedule(fio___poll_ev_wrap__io_task,
                               fio_ev_on_close,
                               io);
  MARK_FUNC();
}

/* *****************************************************************************
EPoll
***************************************************************************** */
#if FIO_ENGINE_EPOLL
#include <sys/epoll.h>

/**
 * Returns a C string detailing the IO engine selected during compilation.
 *
 * Valid values are "kqueue", "epoll" and "poll".
 */
char const *fio_engine(void) { return "epoll"; }

/* epoll tester, in and out */
static int evio_fd[3] = {-1, -1, -1};

FIO_SFUNC void fio_monitor_destroy(void) {
  for (int i = 0; i < 3; ++i) {
    if (evio_fd[i] != -1) {
      close(evio_fd[i]);
      evio_fd[i] = -1;
    }
  }
}

FIO_SFUNC void fio_monitor_init(void) {
  fio_monitor_destroy();
  for (int i = 0; i < 3; ++i) {
    evio_fd[i] = epoll_create1(EPOLL_CLOEXEC);
    if (evio_fd[i] == -1)
      goto error;
  }
  for (int i = 1; i < 3; ++i) {
    struct epoll_event chevent = {
        .events = (EPOLLOUT | EPOLLIN),
        .data.fd = evio_fd[i],
    };
    if (epoll_ctl(evio_fd[0], EPOLL_CTL_ADD, evio_fd[i], &chevent) == -1)
      goto error;
  }
  return;
error:
  FIO_LOG_FATAL("couldn't initialize epoll.");
  fio_monitor_destroy();
  exit(errno);
  return;
}

FIO_IFUNC int fio___epoll_add2(int fd,
                               void *udata,
                               uint32_t events,
                               int ep_fd) {
  int ret = -1;
  struct epoll_event chevent;
  if (fd == -1)
    return ret;
  do {
    errno = 0;
    chevent = (struct epoll_event){
        .events = events,
        .data.ptr = udata,
    };
    ret = epoll_ctl(ep_fd, EPOLL_CTL_MOD, fd, &chevent);
    if (ret == -1 && errno == ENOENT) {
      errno = 0;
      chevent = (struct epoll_event){
          .events = events,
          .data.ptr = udata,
      };
      ret = epoll_ctl(ep_fd, EPOLL_CTL_ADD, fd, &chevent);
    }
  } while (errno == EINTR);

  return ret;
}

FIO_IFUNC void fio_monitor_monitor(int fd, void *udata, uintptr_t flags) {
  if ((flags & POLLIN))
    fio___epoll_add2(fd,
                     udata,
                     (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                     evio_fd[1]);
  if ((flags & POLLOUT))
    fio___epoll_add2(fd,
                     udata,
                     (EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                     evio_fd[2]);
  return;
}

FIO_IFUNC void fio_monitor_forget(int fd) {
  struct epoll_event chevent = {.events = (EPOLLOUT | EPOLLIN)};
  epoll_ctl(evio_fd[1], EPOLL_CTL_DEL, fd, &chevent);
  epoll_ctl(evio_fd[2], EPOLL_CTL_DEL, fd, &chevent);
}

FIO_SFUNC int fio_monitor_review(int timeout_millisec) {
  struct epoll_event internal[2];
  struct epoll_event events[FIO_POLL_MAX_EVENTS];
  int total = 0;
  /* wait for events and handle them */
  int internal_count = epoll_wait(evio_fd[0], internal, 2, timeout_millisec);
  if (internal_count == 0)
    return internal_count;
  for (int j = 0; j < internal_count; ++j) {
    int active_count =
        epoll_wait(internal[j].data.fd, events, FIO_POLL_MAX_EVENTS, 0);
    if (active_count > 0) {
      for (int i = 0; i < active_count; i++) {
        if (events[i].events & (~(EPOLLIN | EPOLLOUT))) {
          // errors are handled as disconnections (on_close)
          fio___poll_on_close(0, events[i].data.ptr);
        } else {
          // no error, then it's an active event(s)
          if (events[i].events & EPOLLIN)
            fio___poll_on_data(0, events[i].data.ptr);
          if (events[i].events & EPOLLOUT)
            fio___poll_on_ready(0, events[i].data.ptr);
        }
      } // end for loop
      total += active_count;
    }
  }
  return total;
}

/* *****************************************************************************
KQueue
***************************************************************************** */
#elif FIO_ENGINE_KQUEUE
#include <sys/event.h>

/**
 * Returns a C string detailing the IO engine selected during compilation.
 *
 * Valid values are "kqueue", "epoll" and "poll".
 */
char const *fio_engine(void) { return "kqueue"; }

static int evio_fd = -1;

FIO_SFUNC void fio_monitor_destroy(void) {
  if (evio_fd != -1)
    close(evio_fd);
}

FIO_SFUNC void fio_monitor_init(void) {
  fio_monitor_destroy();
  evio_fd = kqueue();
  if (evio_fd == -1) {
    FIO_LOG_FATAL("couldn't open kqueue.\n");
    exit(errno);
  }
}

FIO_IFUNC void fio_monitor_monitor(int fd, void *udata, uintptr_t flags) {
  struct kevent chevent[2];
  int i = 0;
  if ((flags & POLLIN)) {
    EV_SET(chevent,
           fd,
           EVFILT_READ,
           EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
           0,
           0,
           udata);
    ++i;
  }
  if ((flags & POLLOUT)) {
    EV_SET(chevent + i,
           fd,
           EVFILT_WRITE,
           EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
           0,
           0,
           udata);
    ++i;
  }
  do {
    errno = 0;
  } while (kevent(evio_fd, chevent, i, NULL, 0, NULL) == -1 && errno == EINTR);
  return;
}

FIO_SFUNC void fio_monitor_forget(int fd) {
  if (evio_fd < 0)
    return;
  struct kevent chevent[2];
  EV_SET(chevent, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
  EV_SET(chevent + 1, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
  do {
    errno = 0;
    kevent(evio_fd, chevent, 2, NULL, 0, NULL);
  } while (errno == EINTR);
}

FIO_SFUNC int fio_monitor_review(int timeout_) {
  if (evio_fd < 0)
    return -1;
  struct kevent events[FIO_POLL_MAX_EVENTS] = {{0}};

  const struct timespec timeout = {.tv_sec = (timeout_ / 1024),
                                   .tv_nsec =
                                       ((timeout_ & (1023UL)) * 1000000)};
  /* wait for events and handle them */
  int active_count =
      kevent(evio_fd, NULL, 0, events, FIO_POLL_MAX_EVENTS, &timeout);

  if (active_count > 0) {
    for (int i = 0; i < active_count; i++) {
      // test for event(s) type
      if (events[i].filter == EVFILT_WRITE) {
        fio___poll_on_ready(0, events[i].udata);
      } else if (events[i].filter == EVFILT_READ) {
        fio___poll_on_data(0, events[i].udata);
      }
      if (events[i].flags & (EV_EOF | EV_ERROR)) {
        fio___poll_on_close(0, events[i].udata);
      }
    }
  } else if (active_count < 0) {
    if (errno == EINTR)
      return 0;
    return -1;
  }
  return active_count;
}

/* *****************************************************************************
Poll
***************************************************************************** */
#elif FIO_ENGINE_POLL

#define FIO_POLL
#include "fio-stl.h"

/**
 * Returns a C string detailing the IO engine selected during compilation.
 *
 * Valid values are "kqueue", "epoll" and "poll".
 */
char const *fio_engine(void) { return "poll"; }

static fio_poll_s fio___poll_data =
    FIO_POLL_INIT(fio___poll_on_data, fio___poll_on_ready, fio___poll_on_close);
FIO_SFUNC void fio_monitor_destroy(void) { fio_poll_destroy(&fio___poll_data); }

FIO_SFUNC void fio_monitor_init(void) {
  fio___poll_data = (fio_poll_s)FIO_POLL_INIT(fio___poll_on_data,
                                              fio___poll_on_ready,
                                              fio___poll_on_close);
}

FIO_IFUNC void fio_monitor_monitor(int fd, void *udata, uintptr_t flags) {
  fio_poll_monitor(&fio___poll_data, fd, udata, flags);
}
FIO_SFUNC void fio_monitor_forget(int fd) {
  fio_poll_forget(&fio___poll_data, fd);
}

/** returns non-zero if events were scheduled, 0 if idle */
FIO_SFUNC int fio_monitor_review(int timeout) {
  return fio_poll_review(&fio___poll_data, timeout);
}

#endif /* FIO_ENGINE_POLL */
