/* *****************************************************************************
Section Start Marker










                       Polling State Machine - epoll











***************************************************************************** */
#ifndef H_FACIL_IO_H   /* Development inclusion - ignore line */
#include "0011 base.c" /* Development inclusion - ignore line */
#endif                 /* Development inclusion - ignore line */

static inline void fio_force_close_in_poll(intptr_t uuid) {
  fio_force_close(uuid);
}

#ifndef FIO_POLL_MAX_EVENTS
/* The number of events to collect with each call to epoll or kqueue. */
#define FIO_POLL_MAX_EVENTS 96
#endif

/* *****************************************************************************
Start Section
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

FIO_IFUNC void fio_poll_close(void) {
  for (int i = 0; i < 3; ++i) {
    if (evio_fd[i] != -1) {
      close(evio_fd[i]);
      evio_fd[i] = -1;
    }
  }
}

FIO_IFUNC void fio_poll_init(void) {
  fio_poll_close();
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
  fio_poll_close();
  exit(errno);
  return;
}

FIO_IFUNC int fio___poll_add2(int fd, uint32_t events, int ep_fd) {
  struct epoll_event chevent;
  int ret;
  do {
    errno = 0;
    chevent = (struct epoll_event){
        .events = events,
        .data.fd = fd,
    };
    ret = epoll_ctl(ep_fd, EPOLL_CTL_MOD, fd, &chevent);
    if (ret == -1 && errno == ENOENT) {
      errno = 0;
      chevent = (struct epoll_event){
          .events = events,
          .data.fd = fd,
      };
      ret = epoll_ctl(ep_fd, EPOLL_CTL_ADD, fd, &chevent);
    }
  } while (errno == EINTR);

  return ret;
}

FIO_IFUNC void fio_poll_add_read(intptr_t fd) {
  fio___poll_add2(fd,
                  (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                  evio_fd[1]);
  return;
}

FIO_IFUNC void fio_poll_add_write(intptr_t fd) {
  fio___poll_add2(fd,
                  (EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                  evio_fd[2]);
  return;
}

FIO_IFUNC void fio_poll_add(intptr_t fd) {
  if (fio___poll_add2(fd,
                      (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                      evio_fd[1]) == -1)
    return;
  fio___poll_add2(fd,
                  (EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                  evio_fd[2]);
  return;
}

FIO_IFUNC void fio_poll_remove_fd(intptr_t fd) {
  struct epoll_event chevent = {.events = (EPOLLOUT | EPOLLIN), .data.fd = fd};
  epoll_ctl(evio_fd[1], EPOLL_CTL_DEL, fd, &chevent);
  epoll_ctl(evio_fd[2], EPOLL_CTL_DEL, fd, &chevent);
}

FIO_SFUNC size_t fio_poll(void) {
  int timeout_millisec = fio___timer_calc_first_interval();
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
          // errors are hendled as disconnections (on_close)
          fio_force_close_in_poll(fd2uuid(events[i].data.fd));
        } else {
          // no error, then it's an active event(s)
          if (events[i].events & EPOLLOUT) {
            fio_defer_urgent(deferred_on_ready,
                             (void *)fd2uuid(events[i].data.fd),
                             NULL);
          }
          if (events[i].events & EPOLLIN)
            fio_defer(deferred_on_data,
                      (void *)fd2uuid(events[i].data.fd),
                      NULL);
        }
      } // end for loop
      total += active_count;
    }
  }
  return total;
}

/* *****************************************************************************
Section Start Marker










                       Polling State Machine - kqueue











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

FIO_IFUNC void fio_poll_close(void) { close(evio_fd); }

FIO_IFUNC void fio_poll_init(void) {
  fio_poll_close();
  evio_fd = kqueue();
  if (evio_fd == -1) {
    FIO_LOG_FATAL("couldn't open kqueue.\n");
    exit(errno);
  }
}

FIO_IFUNC void fio_poll_add_read(intptr_t fd) {
  struct kevent chevent[1];
  EV_SET(chevent,
         fd,
         EVFILT_READ,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0,
         0,
         ((void *)fd));
  do {
    errno = 0;
    kevent(evio_fd, chevent, 1, NULL, 0, NULL);
  } while (errno == EINTR);
  return;
}

FIO_IFUNC void fio_poll_add_write(intptr_t fd) {
  struct kevent chevent[1];
  EV_SET(chevent,
         fd,
         EVFILT_WRITE,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0,
         0,
         ((void *)fd));
  do {
    errno = 0;
    kevent(evio_fd, chevent, 1, NULL, 0, NULL);
  } while (errno == EINTR);
  return;
}

FIO_IFUNC void fio_poll_add(intptr_t fd) {
  struct kevent chevent[2];
  EV_SET(chevent,
         fd,
         EVFILT_READ,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0,
         0,
         ((void *)fd));
  EV_SET(chevent + 1,
         fd,
         EVFILT_WRITE,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0,
         0,
         ((void *)fd));
  do {
    errno = 0;
  } while (kevent(evio_fd, chevent, 2, NULL, 0, NULL) == -1 && errno == EINTR);
  return;
}

FIO_IFUNC void fio_poll_remove_fd(intptr_t fd) {
  FIO_LOG_WARNING("fio_poll_remove_fd called for %d", (int)fd);
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

FIO_SFUNC size_t fio_poll(void) {
  if (evio_fd < 0)
    return -1;
  int timeout_millisec = fio___timer_calc_first_interval();
  struct kevent events[FIO_POLL_MAX_EVENTS] = {{0}};

  const struct timespec timeout = {
      .tv_sec = (timeout_millisec / 1024),
      .tv_nsec = ((timeout_millisec & (1023UL)) * 1000000)};
  /* wait for events and handle them */
  int active_count =
      kevent(evio_fd, NULL, 0, events, FIO_POLL_MAX_EVENTS, &timeout);

  if (active_count > 0) {
    for (int i = 0; i < active_count; i++) {
      // test for event(s) type
      if (events[i].filter == EVFILT_WRITE) {
        fio_defer_urgent(deferred_on_ready,
                         ((void *)fd2uuid(events[i].udata)),
                         NULL);
      } else if (events[i].filter == EVFILT_READ) {
        fio_defer(deferred_on_data, (void *)fd2uuid(events[i].udata), NULL);
      }
      if (events[i].flags & (EV_EOF | EV_ERROR)) {
        fio_force_close_in_poll(fd2uuid(events[i].udata));
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
Section Start Marker










                       Polling State Machine - poll











***************************************************************************** */
#elif FIO_ENGINE_POLL

#define FIO_POLL
#define FIO_POLL_HAS_UDATA_COLLECTION 0
#include "fio-stl.h"

/**
 * Returns a C string detailing the IO engine selected during compilation.
 *
 * Valid values are "kqueue", "epoll" and "poll".
 */
char const *fio_engine(void) { return "poll"; }

FIO_SFUNC void fio___poll_ev_wrap_data(int fd, void *udata) {
  intptr_t uuid = fd2uuid(fd);
  fio_defer(deferred_on_data, (void *)uuid, udata);
}
FIO_SFUNC void fio___poll_ev_wrap_ready(int fd, void *udata) {
  intptr_t uuid = fd2uuid(fd);
  fio_defer_urgent(deferred_on_ready, (void *)uuid, udata);
}
FIO_SFUNC void fio___poll_ev_wrap_close(int fd, void *udata) {
  intptr_t uuid = fd2uuid(fd);
  fio_force_close_in_poll(uuid);
  (void)udata;
}

static fio_poll_s fio___poll_data = FIO_POLL_INIT(fio___poll_ev_wrap_data,
                                                  fio___poll_ev_wrap_ready,
                                                  fio___poll_ev_wrap_close);
FIO_IFUNC void fio_poll_close(void) { fio_poll_destroy(&fio___poll_data); }

FIO_IFUNC void fio_poll_init(void) {}

FIO_IFUNC void fio_poll_remove_fd(intptr_t fd) {
  fio_poll_forget(&fio___poll_data, fd);
}

FIO_IFUNC void fio_poll_add_read(intptr_t fd) {
  fio_poll_monitor(&fio___poll_data, fd, NULL, POLLIN);
}

FIO_IFUNC void fio_poll_add_write(intptr_t fd) {
  fio_poll_monitor(&fio___poll_data, fd, NULL, POLLOUT);
}

FIO_IFUNC void fio_poll_add(intptr_t fd) {
  fio_poll_monitor(&fio___poll_data, fd, NULL, POLLIN | POLLOUT);
}

/** returns non-zero if events were scheduled, 0 if idle */
static size_t fio_poll(void) {
  int timeout_millisec = fio___timer_calc_first_interval();
  return (size_t)(fio_poll_review(&fio___poll_data, timeout_millisec) > 0);
}

#endif /* FIO_ENGINE_POLL */
