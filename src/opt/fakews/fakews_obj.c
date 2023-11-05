#include "fakews_internal.h"

/* Delete.
 */

void fakews_del(struct fakews *fakews) {
  if (!fakews) return;
  if (fakews->host) free(fakews->host);
  if (fakews->raddr) free(fakews->raddr);
  if (fakews->rbuf) free(fakews->rbuf);
  if (fakews->wbuf) free(fakews->wbuf);
  if (fakews->fd>=0) close(fakews->fd);
  if (fakews->path) free(fakews->path);
  free(fakews);
}

/* New.
 */

struct fakews *fakews_new(
  const char *host,int hostc,
  int port,
  const char *path,int pathc,
  void (*cb_connect)(void *userdata),
  void (*cb_disconnect)(void *userdata),
  void (*cb_message)(int opcode,const void *v,int c,void *userdata),
  void *userdata
) {
  if (!host) hostc=0; else if (hostc<0) { hostc=0; while (host[hostc]) hostc++; }
  if (hostc<1) return 0;
  if (!path) pathc=0; else if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  if (pathc<1) return 0;
  if ((port<1)||(port>0xffff)) return 0;
  
  struct fakews *fakews=calloc(1,sizeof(struct fakews));
  if (!fakews) return 0;
  
  if (!(fakews->host=malloc(hostc+1))) {
    free(fakews);
    return 0;
  }
  memcpy(fakews->host,host,hostc);
  fakews->host[hostc]=0;
  fakews->hostc=hostc;
  
  if (!(fakews->path=malloc(pathc+1))) {
    free(fakews->host);
    free(fakews);
    return 0;
  }
  memcpy(fakews->path,path,pathc);
  fakews->path[pathc]=0;
  fakews->pathc=pathc;
 
  fakews->fd=-1; 
  fakews->port=port;
  fakews->cb_connect=cb_connect;
  fakews->cb_disconnect=cb_disconnect;
  fakews->cb_message=cb_message;
  
  return fakews;
}

/* Current time.
 */
 
static int64_t fakews_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (int64_t)tv.tv_sec*1000000ll+tv.tv_usec;
}

/* Require connection.
 */
 
static int fakews_require_connection(struct fakews *fakews) {
  if (!fakews) return -1;
  if (fakews->fd>=0) return 0;
  int64_t now=fakews_now();
  if (now<fakews->next_autoconnect_time) return -1;
  fakews->next_autoconnect_time=now+FAKEWS_AUTOCONNECT_INTERVAL_US;
  return fakews_connect_now(fakews);
}

/* Write from buffer to socket.
 */
 
static int fakews_write(struct fakews *fakews) {
  if (fakews->wbufc<1) return 0;
  int err=write(fakews->fd,fakews->wbuf+fakews->wbufp,fakews->wbufc);
  if (err<=0) return -1;
  if (fakews->wbufc-=err) fakews->wbufp+=err;
  else fakews->wbufp=0;
  return 0;
}

/* Read from socket to buffer.
 */
 
static int fakews_read(struct fakews *fakews) {
  
  if (fakews->rbufp+fakews->rbufc>=fakews->rbufa) {
    if (fakews->rbufp) {
      memmove(fakews->rbuf,fakews->rbuf+fakews->rbufp,fakews->rbufc);
    } else {
      if (fakews->rbufa>=FAKEWS_RBUF_SANITY_LIMIT) return -1;
      int na=fakews->rbufa+1024;
      void *nv=realloc(fakews->rbuf,na);
      if (!nv) return -1;
      fakews->rbuf=nv;
      fakews->rbufa=na;
    }
  }
  
  int err=read(fakews->fd,fakews->rbuf+fakews->rbufp+fakews->rbufc,fakews->rbufa-fakews->rbufc-fakews->rbufp);
  if (err<=0) return -1;
  fakews->rbufc+=err;
  
  while (fakews->rbufc>=3) {
    const uint8_t *src=(uint8_t*)fakews->rbuf+fakews->rbufp;
    int paylen=(src[1]<<8)|src[2];
    int pktlen=3+paylen;
    if (fakews->rbufc<pktlen) break;
    if (fakews->cb_message) {
      fakews->cb_message(src[0],src+3,paylen,fakews->userdata);
    }
    fakews->rbufp+=pktlen;
    fakews->rbufc-=pktlen;
  }
  
  if (!fakews->rbufc) fakews->rbufp=0;
  return 0;
}

/* Update.
 */
 
static int fakews_delay_if_positive(int toms) {
  if (toms>0) usleep(toms*1000);
  return 0;
}

static int fakews_close(struct fakews *fakews) {
  if (fakews->cb_disconnect) fakews->cb_disconnect(fakews->userdata);
  close(fakews->fd);
  fakews->fd=-1;
  return 0;
}

int fakews_update(struct fakews *fakews,int toms) {
  if (!fakews) return fakews_delay_if_positive(toms);
  if (fakews_require_connection(fakews)<0) return fakews_delay_if_positive(toms);
  struct pollfd pollfd={.fd=fakews->fd,.events=POLLIN|POLLERR|POLLHUP};
  if (fakews->wbufc) pollfd.events|=POLLOUT;
  if (poll(&pollfd,1,toms)<0) return fakews_close(fakews);
  if (pollfd.revents&(POLLERR|POLLHUP)) return fakews_close(fakews);
  if (fakews->wbufc&&(pollfd.revents&POLLOUT)) {
    if (fakews_write(fakews)<0) return fakews_close(fakews);
  }
  if (pollfd.revents&POLLIN) {
    if (fakews_read(fakews)<0) return fakews_close(fakews);
  }
  return 0;
}

/* Test connection.
 */

int fakews_is_connected(const struct fakews *fakews) {
  if (!fakews) return 0;
  return (fakews->fd>=0)?1:0;
}

/* Connect.
 */
  
int fakews_connect_now(struct fakews *fakews) {
  if (!fakews) return -1;
  if (fakews->fd>=0) return 0;
  
  /* DNS. Should only happen once for the life of the fakews. We never drop it.
   */
  if (!fakews->raddr) {
    char zport[32];
    snprintf(zport,sizeof(zport),"%d",fakews->port);
    struct addrinfo hint={
      .ai_family=AF_UNSPEC,
      .ai_socktype=SOCK_STREAM,
      .ai_protocol=0,
      .ai_flags=AI_NUMERICSERV|AI_ADDRCONFIG
    };
    struct addrinfo *aiv=0;
    int err=getaddrinfo(fakews->host,zport,&hint,&aiv);
    if (err<0) return -1;
    struct addrinfo *ai=aiv;
    for (;ai;ai=ai->ai_next) {
      if (!ai->ai_addr) continue;
      // Filter addresses? I think anything is probably ok.
      fakews->family=ai->ai_family;
      fakews->socktype=ai->ai_socktype;
      fakews->protocol=ai->ai_protocol;
      if (!(fakews->raddr=malloc(ai->ai_addrlen))) {
        freeaddrinfo(aiv);
        return -1;
      }
      memcpy(fakews->raddr,ai->ai_addr,ai->ai_addrlen);
      fakews->raddrc=ai->ai_addrlen;
      break;
    }
    freeaddrinfo(aiv);
    if (!fakews->raddr) return -1;
  }
  
  /* Create and connect socket.
   */
  char req[256];
  int reqc=snprintf(req,sizeof(req),"FAKEWEBSOCKET %.*s HTTP/1.1\r\n\r\n",fakews->pathc,fakews->path);
  if ((reqc<1)||(reqc>=sizeof(req))) return -1;
  if ((fakews->fd=socket(fakews->family,fakews->socktype,fakews->protocol))<0) return -1;
  if (connect(fakews->fd,fakews->raddr,fakews->raddrc)<0) {
    fprintf(stderr,"Failed to connect to %.*s:%d: %m\n",fakews->hostc,fakews->host,fakews->port);
    close(fakews->fd);
    fakews->fd=-1;
    return -1;
  }
  if (write(fakews->fd,req,reqc)<0) {
    fprintf(stderr,"Failed to send fake websocket handshake to %.*s:%d: %m\n",fakews->hostc,fakews->host,fakews->port);
    close(fakews->fd);
    fakews->fd=-1;
    return -1;
  }
  fprintf(stderr,"Connected to %.*s:%d\n",fakews->hostc,fakews->host,fakews->port);
  
  return 0;
}

/* Queue packet for delivery.
 */
 
int fakews_send(struct fakews *fakews,int opcode,const void *v,int c) {
  if (!fakews||(fakews->fd<0)) return -1;
  if ((opcode<0)||(opcode>0xff)) return -1;
  if ((c<0)||(c>0xffff)||(c&&!v)) return -1;
  
  int addc=3+c;
  if (fakews->wbufp+fakews->wbufc>fakews->wbufa-addc) {
    if (fakews->wbufp) {
      memmove(fakews->wbuf,fakews->wbuf+fakews->wbufp,fakews->wbufc);
      fakews->wbufp=0;
    }
    int na=fakews->wbufc+addc;
    if (na<INT_MAX-1024) na=(na+1024)&~1023;
    if (na>FAKEWS_WBUF_SANITY_LIMIT) return -1;
    if (na>fakews->wbufa) {
      void *nv=realloc(fakews->wbuf,na);
      if (!nv) return -1;
      fakews->wbuf=nv;
      fakews->wbufa=na;
    }
  }
  
  char *dst=fakews->wbuf+fakews->wbufp+fakews->wbufc;
  *dst++=opcode;
  *dst++=c>>8;
  *dst++=c;
  memcpy(dst,v,c);
  fakews->wbufc+=addc;
  
  return 0;
}
