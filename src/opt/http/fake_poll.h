/* fake_poll.h
 * Define and stub out everything from sys/poll.h, for Windows.
 * ...ugh... also netdb.h and sys/socket.h
 */

#ifndef FAKE_POLL_H
#define FAKE_POLL_H

struct pollfd {
  int fd,events,revents;
};

#define POLLIN 1
#define POLLOUT 2
#define POLLERR 4
#define POLLHUP 8

static inline int poll(struct pollfd *fdv,int fdc,int to_ms) {
  return 0;
}

struct addrinfo {
  int ai_flags;
  int ai_family;
  int ai_socktype;
  int ai_protocol;
  void *ai_addr;
  int ai_addrlen;
};

#define AI_PASSIVE 1
#define AI_NUMERICSERV 2
#define AI_ADDRCONFIG 4

static inline int getaddrinfo(const char *host,const char *service,struct addrinfo *hints,struct addrinfo **ai) {
  return -1;
}

static inline void freeaddrinfo(struct addrinfo *ai) {
}

typedef int socklen_t;

#if 0

struct sockaddr {
  int sa_family;
};

#define AF_UNSPEC 1
#define IPPROTO_TCP 1
#define SOCK_STREAM 1
#define PF_INET 1
#define SOL_SOCKET 1
#define SO_REUSEADDR 1

static inline int socket(int family,int mode,int protocol) {
  return -1;
}

static inline int bind(int fd,const void *saddr,int saddrc) {
  return -1;
}

static inline int listen(int fd,int clientc) {
  return -1;
}

static inline int accept(int fd,struct sockaddr *saddr,int *saddrc) {
  return -1;
}

static inline int setsockopt(int fd,int level,int k,void *v,int c) {
  return -1;
}
#endif

#endif
