/* *****************************************************************************
IO Polling
***************************************************************************** */
#ifndef H_FACIL_IO_H /* development sugar, ignore */
#include "999 dev.h" /* development sugar, ignore */
#endif               /* development sugar, ignore */

#ifndef FIO_POLL_TICK
#define FIO_POLL_TICK 993
#endif

#if !defined(FIO_ENGINE_EPOLL) && !defined(FIO_ENGINE_KQUEUE) &&               \
    !defined(FIO_ENGINE_POLL)
#if defined(HAVE_EPOLL) || __has_include("sys/epoll.h")
#define FIO_ENGINE_EPOLL 1
#elif defined(HAVE_KQUEUE) || __has_include("sys/event.h")
#define FIO_ENGINE_KQUEUE 1
#else
#define FIO_ENGINE_POLL 1
#endif
#endif /* !defined(FIO_ENGINE_EPOLL) ... */

static inline void fio_force_close_in_poll(void *uuid_) {
  fio_uuid_s *uuid = uuid_;
  uuid->state = FIO_UUID_CLOSED;
}

#ifndef FIO_POLL_MAX_EVENTS
/* The number of events to collect with each call to epoll or kqueue. */
#define FIO_POLL_MAX_EVENTS 96
#endif

FIO_IFUNC void fio___poll_ev_wrap_data(int fd, void *uuid_) {
  fio_uuid_s *uuid = uuid_;
  fio_uuid_dup(uuid);
  fio_queue_push(fio_queue_select(uuid->protocol->reserved.flags),
                 fio_ev_on_data,
                 uuid,
                 uuid->udata);
  fio_user_thread_wake();
  (void)fd;
}
FIO_IFUNC void fio___poll_ev_wrap_ready(int fd, void *uuid_) {
  fio_uuid_s *uuid = uuid_;
  fio_uuid_dup(uuid);
  fio_queue_push(&tasks_io_core, fio_ev_on_ready, uuid, uuid->udata);
  (void)fd;
}

FIO_IFUNC void fio___poll_ev_wrap_close(int fd, void *uuid_) {
  fio_uuid_s *uuid = uuid_;
  uuid->state = FIO_UUID_CLOSED;
  fio_uuid_free(uuid);
  (void)fd;
}

FIO_IFUNC int fio_uuid_monitor_tick_len(void) {
  return (FIO_POLL_TICK * fio_data.running) | 7;
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

FIO_IFUNC void fio_uuid_monitor_close(void) {
  for (int i = 0; i < 3; ++i) {
    if (evio_fd[i] != -1) {
      close(evio_fd[i]);
      evio_fd[i] = -1;
    }
  }
}

FIO_IFUNC void fio_uuid_monitor_init(void) {
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

FIO_IFUNC int fio___peoll_add2(fio_uuid_s *uuid, uint32_t events, int ep_fd) {
  int ret = -1;
  struct epoll_event chevent;
  int fd = uuid->fd;
  if (fd == -1)
    return ret;
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

FIO_IFUNC void fio_uuid_monitor_add_read(fio_uuid_s *uuid) {
  fio___peoll_add2(uuid,
                   (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                   evio_fd[1]);
  return;
}

FIO_IFUNC void fio_uuid_monitor_add_write(fio_uuid_s *uuid) {
  fio___peoll_add2(uuid,
                   (EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                   evio_fd[2]);
  return;
}

FIO_IFUNC void fio_uuid_monitor_add(fio_uuid_s *uuid) {
  if (fio___peoll_add2(uuid,
                       (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                       evio_fd[1]) == -1)
    return;
  fio___peoll_add2(uuid,
                   (EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLONESHOT),
                   evio_fd[2]);
  return;
}

FIO_IFUNC void fio_uuid_monitor_remove(fio_uuid_s *uuid) {
  struct epoll_event chevent = {.events = (EPOLLOUT | EPOLLIN),
                                .data.ptr = uuid};
  epoll_ctl(evio_fd[1], EPOLL_CTL_DEL, uuid, &chevent);
  epoll_ctl(evio_fd[2], EPOLL_CTL_DEL, uuid, &chevent);
}

FIO_SFUNC size_t fio_uuid_monitor(void) {
  int timeout_millisec = fio_uuid_monitor_tick_len();
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
          fio___poll_ev_wrap_close(0, events[i].data.ptr);
        } else {
          // no error, then it's an active event(s)
          if (events[i].events & EPOLLOUT) {
            fio___poll_ev_wrap_ready(0, events[i].data.ptr);
          }
          if (events[i].events & EPOLLIN)
            fio___poll_ev_wrap_data(0, events[i].data.ptr);
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

FIO_IFUNC void fio_uuid_monitor_close(void) { close(evio_fd); }

FIO_IFUNC void fio_uuid_monitor_init(void) {
  fio_uuid_monitor_close();
  evio_fd = kqueue();
  if (evio_fd == -1) {
    FIO_LOG_FATAL("couldn't open kqueue.\n");
    exit(errno);
  }
}

FIO_IFUNC void fio_uuid_monitor_add_read(fio_uuid_s *uuid) {
  struct kevent chevent[1];
  EV_SET(chevent,
         uuid->fd,
         EVFILT_READ,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0,
         0,
         ((void *)uuid));
  do {
    errno = 0;
    kevent(evio_fd, chevent, 1, NULL, 0, NULL);
  } while (errno == EINTR);
  return;
}

FIO_IFUNC void fio_uuid_monitor_add_write(fio_uuid_s *uuid) {
  struct kevent chevent[1];
  EV_SET(chevent,
         uuid->fd,
         EVFILT_WRITE,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0,
         0,
         ((void *)uuid));
  do {
    errno = 0;
    kevent(evio_fd, chevent, 1, NULL, 0, NULL);
  } while (errno == EINTR);
  return;
}

FIO_IFUNC void fio_uuid_monitor_add(fio_uuid_s *uuid) {
  struct kevent chevent[2];
  EV_SET(chevent,
         uuid->fd,
         EVFILT_READ,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0,
         0,
         ((void *)uuid));
  EV_SET(chevent + 1,
         uuid->fd,
         EVFILT_WRITE,
         EV_ADD | EV_ENABLE | EV_CLEAR | EV_ONESHOT,
         0,
         0,
         ((void *)uuid));
  do {
    errno = 0;
  } while (kevent(evio_fd, chevent, 2, NULL, 0, NULL) == -1 && errno == EINTR);
  return;
}

FIO_IFUNC void fio_uuid_monitor_remove(fio_uuid_s *uuid) {
  FIO_LOG_WARNING("fio_uuid_monitor_remove called for %d", (int)uuid->fd);
  if (evio_fd < 0)
    return;
  struct kevent chevent[2];
  EV_SET(chevent, uuid->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
  EV_SET(chevent + 1, uuid->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
  do {
    errno = 0;
    kevent(evio_fd, chevent, 2, NULL, 0, NULL);
  } while (errno == EINTR);
}

FIO_SFUNC size_t fio_uuid_monitor_review(void) {
  if (evio_fd < 0)
    return -1;
  int timeout_millisec = fio_uuid_monitor_tick_len();
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
        fio___poll_ev_wrap_ready(0, (void *)events[i].udata);
      } else if (events[i].filter == EVFILT_READ) {
        fio___poll_ev_wrap_data(0, (void *)events[i].udata);
      }
      if (events[i].flags & (EV_EOF | EV_ERROR)) {
        fio___poll_ev_wrap_close(0, (void *)events[i].udata);
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

static fio_poll_s fio___poll_data = FIO_POLL_INIT(fio___poll_ev_wrap_data,
                                                  fio___poll_ev_wrap_ready,
                                                  fio___poll_ev_wrap_close);
FIO_IFUNC void fio_uuid_monitor_close(void) {
  fio_poll_destroy(&fio___poll_data);
}

FIO_IFUNC void fio_uuid_monitor_init(void) {}

FIO_IFUNC void fio_poll_remove_fd(fio_uuid_s *uuid) {
  fio_poll_forget(&fio___poll_data, uuid->fd);
}

FIO_IFUNC void fio_uuid_monitor_add_read(fio_uuid_s *uuid) {
  fio_poll_monitor(&fio___poll_data, uuid->fd, uuid, POLLIN);
}

FIO_IFUNC void fio_uuid_monitor_add_write(fio_uuid_s *uuid) {
  fio_poll_monitor(&fio___poll_data, uuid->fd, uuid, POLLOUT);
}

FIO_IFUNC void fio_uuid_monitor_add(fio_uuid_s *uuid) {
  fio_poll_monitor(&fio___poll_data, uuid->fd, uuid, POLLIN | POLLOUT);
}

/** returns non-zero if events were scheduled, 0 if idle */
FIO_SFUNC size_t fio_uuid_monitor_review(void) {
  return (size_t)(
      fio_poll_review(&fio___poll_data, fio_uuid_monitor_tick_len()) > 0);
}

#endif /* FIO_ENGINE_POLL */
