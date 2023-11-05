#ifndef FAKEWS_INTERNAL_H
#define FAKEWS_INTERNAL_H

#include "fakews.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>

#define FAKEWS_WBUF_SANITY_LIMIT 0x01000000
#define FAKEWS_RBUF_SANITY_LIMIT 0x00020000 /* Max packet size is 65538. If it gets this large, something is truly borked. */
#define FAKEWS_AUTOCONNECT_INTERVAL_US 10000000

struct fakews {

  char *host;
  int hostc;
  int port;
  struct sockaddr *raddr;
  int raddrc;
  int family,socktype,protocol;
  char *path;
  int pathc;
  
  int fd;
  char *wbuf;
  int wbufp,wbufc,wbufa;
  char *rbuf;
  int rbufp,rbufc,rbufa;
  int64_t next_autoconnect_time;
  
  void (*cb_connect)(void *userdata);
  void (*cb_disconnect)(void *userdata);
  void (*cb_message)(int opcode,const void *v,int c,void *userdata);
  void *userdata;
};

#endif
