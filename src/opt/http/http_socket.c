#include "http_internal.h"
#include <fcntl.h>
#include <unistd.h>

/* Delete.
 */
 
void http_socket_del(struct http_socket *socket) {
  if (!socket) return;
  if (socket->refc-->1) return;
  
  if (socket->fd>=0) {
    shutdown(socket->fd,SHUT_WR);
    close(socket->fd);
  }
  http_xfer_del(socket->req);
  http_xfer_del(socket->rsp);
  http_listener_del(socket->listener);
  
  free(socket);
}

/* Retain.
 */
 
int http_socket_ref(struct http_socket *socket) {
  if (!socket) return -1;
  if (socket->refc<1) return -1;
  if (socket->refc>=INT_MAX) return -1;
  socket->refc++;
  return 0;
}

/* New.
 */
 
struct http_socket *http_socket_new(struct http_context *context) {
  if (!context) return 0;
  struct http_socket *socket=calloc(1,sizeof(struct http_socket));
  if (!socket) return 0;
  
  socket->refc=1;
  socket->context=context;
  socket->fd=-1;
  socket->protocol=HTTP_PROTOCOL_UNSET;
  
  return socket;
}

/* Grow buffers.
 */
 
#define OK(pfx) { \
  if (tailpp) *(void**)tailpp=socket->pfx##buf+socket->pfx##bufp+socket->pfx##bufc; \
  return socket->pfx##bufa-socket->pfx##bufc-socket->pfx##bufp; \
}

#define SHUFFLE(pfx) { \
  memmove(socket->pfx##buf,socket->pfx##buf+socket->pfx##bufp,socket->pfx##bufc); \
  socket->pfx##bufp=0; \
  if (socket->pfx##bufc+addc<=socket->pfx##bufa) OK(pfx) \
}

#define REALLOC(pfx) { \
  if (na<INT_MAX-1024) na=(na+1024)&~1023; \
  void *nv=realloc(socket->pfx##buf,na); \
  if (!nv) return -1; \
  socket->pfx##buf=nv; \
  socket->pfx##bufa=na; \
}
 
int http_socket_rbuf_require(void *tailpp,struct http_socket *socket,int addc) {
  if (!socket) return -1;
  if (addc<=0) OK(r)
  if (socket->rbufp+socket->rbufc>INT_MAX-addc) return -1;
  int na=socket->rbufp+socket->rbufc+addc;
  if (na<=socket->rbufa) OK(r)
  if (socket->rbufp) SHUFFLE(r)
  REALLOC(r)
  OK(r)
}

int http_socket_wbuf_require(void *tailpp,struct http_socket *socket,int addc) {
  if (!socket) return -1;
  if (addc<=0) OK(w)
  if (socket->wbufp+socket->wbufc>INT_MAX-addc) return -1;
  int na=socket->wbufp+socket->wbufc+addc;
  if (na<=socket->wbufa) OK(w)
  if (socket->wbufp) SHUFFLE(w)
  REALLOC(w)
  OK(w)
}

#undef OK
#undef SHUFFLE
#undef REALLOC

/* Convenient access to read head.
 */
 
int http_socket_rbuf_get(void *headpp,const struct http_socket *socket) {
  if (!socket) return 0;
  if (headpp) *(void**)headpp=socket->rbuf+socket->rbufp;
  return socket->rbufc;
}

int http_socket_rbuf_consume(struct http_socket *socket,int c) {
  if (!socket||(c<0)||(c>socket->rbufc)) return -1;
  if (socket->rbufc-=c) socket->rbufp+=c;
  else socket->rbufp=0;
  return 0;
}

/* Convenient access to write head.
 */
 
int http_socket_wbuf_append(struct http_socket *socket,const void *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  void *tail=0;
  if (http_socket_wbuf_require(&tail,socket,srcc)<0) return -1;
  memcpy(tail,src,srcc);
  socket->wbufc+=srcc;
  return 0;
}

int http_socket_wbuf_appendf(struct http_socket *socket,const char *fmt,...) {
  if (!socket) return -1;
  if (!fmt||!fmt[0]) return 0;
  while (1) {
    va_list vargs;
    va_start(vargs,fmt);
    int err=vsnprintf(socket->wbuf+socket->wbufp+socket->wbufc,socket->wbufa-socket->wbufc-socket->wbufp,fmt,vargs);
    if ((err<0)||(err>=INT_MAX)) return -1;
    if (socket->wbufp+socket->wbufc<socket->wbufa-err) {
      socket->wbufc+=err;
      return 0;
    }
    if (http_socket_wbuf_require(0,socket,err+1)<0) return -1;
  }
}

/* Done writing.
 */
 
static int http_socket_write_complete(struct http_socket *socket) {
  if (socket->protocol==HTTP_PROTOCOL_WEBSOCKET) return 0;
  if (socket->protocol==HTTP_PROTOCOL_FAKEWEBSOCKET) return 0;
  http_xfer_del(socket->req); socket->req=0;
  http_xfer_del(socket->rsp); socket->rsp=0;
  http_listener_del(socket->listener); socket->listener=0;
  return 0;
}

/* Read from file to buffer.
 */
 
int http_socket_read(struct http_socket *socket) {
  if (!socket||(socket->fd<0)) return -1;
  void *dst=0;
  int dsta=http_socket_rbuf_require(&dst,socket,1);
  if (dsta<0) return -1;
  int err=read(socket->fd,dst,dsta);
  if (err<=0) return -1;
  socket->rbufc+=err;
  return 0;
}

/* Write from buffer to file.
 */
 
int http_socket_write(struct http_socket *socket) {
  if (!socket||(socket->fd<0)) return -1;
  if (socket->wbufc<1) return 0;
  int err=write(socket->fd,socket->wbuf+socket->wbufp,socket->wbufc);
  if (err<=0) return -1;
  if (socket->wbufc-=err) {
    socket->wbufp+=err;
  } else {
    socket->wbufp=0;
    http_socket_write_complete(socket);
  }
  return 0;
}

/* OK to close?
 */
 
int http_socket_ok_to_close(const struct http_socket *socket) {
  if (!socket) return 1;
  if (socket->fd<0) return 1;
  if (socket->protocol==HTTP_PROTOCOL_WEBSOCKET) return 1;
  if (socket->protocol==HTTP_PROTOCOL_FAKEWEBSOCKET) return 1;
  if (socket->req||socket->rsp) return 0;
  return 1;
}

/* Close websocket.
 */
 
int http_websocket_close(struct http_socket *socket) {
  if (!socket) return -1;
  //TODO farewell packet? That would get complicated; we'd have to wait for it to send.
  http_context_remove_socket(socket->context,socket);
  return 0;
}

int http_websocket_send(struct http_socket *socket,int type,const void *v,int c) {
  if (!socket) return -1;
  if ((c<0)||(c&&!v)) return -1;
  
  if (socket->protocol==HTTP_PROTOCOL_WEBSOCKET) {
    // Preamble can go up to 10 bytes.
    char preamble[10];
    int preamblec=0;
    preamble[preamblec++]=0x80|(type&0x0f); // 0x80=terminator, we don't allow continued packets.
    if (c<0x7e) { // short
      preamble[preamblec++]=c;
    } else if (c<0x10000) { // medium
      preamble[preamblec++]=0x7e;
      preamble[preamblec++]=c>>8;
      preamble[preamblec++]=c;
    } else { // huge
      preamble[preamblec++]=0x7f;
      preamble[preamblec++]=0;
      preamble[preamblec++]=0;
      preamble[preamblec++]=0;
      preamble[preamblec++]=0;
      preamble[preamblec++]=c>>24;
      preamble[preamblec++]=c>>16;
      preamble[preamblec++]=c>>8;
      preamble[preamblec++]=c;
    }
    if (http_socket_wbuf_append(socket,preamble,preamblec)<0) return -1;
    if (http_socket_wbuf_append(socket,v,c)<0) return -1;
    return 0;
  }
  
  if (socket->protocol==HTTP_PROTOCOL_FAKEWEBSOCKET) {
    if ((type<0)||(type>0xff)||(c>0xffff)) return -1;
    char preamble[3]={type,c>>8,c};
    if (http_socket_wbuf_append(socket,preamble,sizeof(preamble))<0) return -1;
    if (http_socket_wbuf_append(socket,v,c)<0) return -1;
    return 0;
  }
  
  return 0;
}

/* Trivial accessors.
 */
 
void *http_socket_get_userdata(struct http_socket *socket) {
  if (!socket) return 0;
  return socket->userdata;
}

void http_socket_set_userdata(struct http_socket *socket,void *userdata) {
  if (!socket) return;
  socket->userdata=userdata;
}
