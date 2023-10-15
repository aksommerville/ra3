#include "http_internal.h"

/* Delete.
 */
 
void http_context_del(struct http_context *context) {
  if (!context) return;
  if (context->refc-->1) return;
  
  if (context->pollfdv) free(context->pollfdv);
  
  if (context->listenerv) {
    while (context->listenerc-->0) http_listener_del(context->listenerv[context->listenerc]);
    free(context->listenerv);
  }
  
  if (context->serverv) {
    while (context->serverc-->0) http_server_del(context->serverv[context->serverc]);
    free(context->serverv);
  }
  
  if (context->socketv) {
    while (context->socketc-->0) http_socket_del(context->socketv[context->socketc]);
    free(context->socketv);
  }
  
  if (context->extfdv) {
    free(context->extfdv);
  }
  
  free(context);
}

/* Retain.
 */
 
int http_context_ref(struct http_context *context) {
  if (!context) return -1;
  if (context->refc<1) return -1;
  if (context->refc==INT_MAX) return -1;
  context->refc++;
  return 0;
}

/* New.
 */
 
struct http_context *http_context_new() {
  struct http_context *context=calloc(1,sizeof(struct http_context));
  if (!context) return 0;
  
  context->refc=1;
  
  return context;
}

/* Grow pollfdv.
 */
 
struct pollfd *http_context_pollfdv_require(struct http_context *context) {
  if (context->pollfdc>=context->pollfda) {
    int na=context->pollfda+16;
    if (na>INT_MAX/sizeof(struct pollfd)) return 0;
    void *nv=realloc(context->pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return 0;
    context->pollfdv=nv;
    context->pollfda=na;
  }
  struct pollfd *pollfd=context->pollfdv+context->pollfdc++;
  memset(pollfd,0,sizeof(struct pollfd));
  return pollfd;
}

/* Attach a new unconfigured listener.
 */
 
struct http_listener *http_context_add_new_listener(struct http_context *context) {
  if (!context) return 0;
  if (context->listenerc>=context->listenera) {
    int na=context->listenera+16;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(context->listenerv,sizeof(void*)*na);
    if (!nv) return 0;
    context->listenerv=nv;
    context->listenera=na;
  }
  struct http_listener *listener=http_listener_new(context);
  if (!listener) return 0;
  context->listenerv[context->listenerc++]=listener;
  return listener;
}

/* Find and remove listener.
 */
 
void http_context_remove_listener(struct http_context *context,struct http_listener *listener) {
  if (!context||!listener) return;
  int i=context->listenerc;
  while (i-->0) {
    if (context->listenerv[i]!=listener) continue;
    context->listenerc--;
    memmove(context->listenerv+i,context->listenerv+i+1,sizeof(void*)*(context->listenerc-i));
    http_listener_del(listener);
    return;
  }
}

/* Attach a new unconfigured server.
 */
 
struct http_server *http_context_add_new_server(struct http_context *context) {
  if (!context) return 0;
  if (context->serverc>=context->servera) {
    int na=context->servera+4;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(context->serverv,sizeof(void*)*na);
    if (!nv) return 0;
    context->serverv=nv;
    context->servera=na;
  }
  struct http_server *server=http_server_new(context);
  if (!server) return 0;
  context->serverv[context->serverc++]=server;
  return server;
}

/* Find and remove server.
 */
 
void http_context_remove_server(struct http_context *context,struct http_server *server) {
  if (!context||!server) return;
  int i=context->serverc;
  while (i-->0) {
    if (context->serverv[i]!=server) continue;
    context->serverc--;
    memmove(context->serverv+i,context->serverv+i+1,sizeof(void*)*(context->serverc-i));
    http_server_del(server);
    return;
  }
}

/* Attach a new socket.
 */
 
struct http_socket *http_context_add_new_socket(struct http_context *context,int fd) {
  if (!context||(fd<0)) return 0;
  if (context->socketc>=context->socketa) {
    int na=context->socketa+16;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(context->socketv,sizeof(void*)*na);
    if (!nv) return 0;
    context->socketv=nv;
    context->socketa=na;
  }
  struct http_socket *socket=http_socket_new(context);
  if (!socket) return 0;
  context->socketv[context->socketc++]=socket;
  socket->fd=fd;
  return socket;
}

/* Remove socket.
 */
 
void http_context_remove_socket(struct http_context *context,struct http_socket *socket) {
  if (!context||!socket) return;
  int i=context->socketc;
  while (i-->0) {
    if (context->socketv[i]!=socket) continue;
    context->socketc--;
    memmove(context->socketv+i,context->socketv+i+1,sizeof(void*)*(context->socketc-i));
    http_socket_del(socket);
    return;
  }
}

/* Get server or socket by fd or index.
 */
 
struct http_server *http_context_get_server_by_index(const struct http_context *context,int p) {
  if (!context||(p<0)) return 0;
  if (p>=context->serverc) return 0;
  return context->serverv[p];
}

struct http_server *http_context_get_server_by_fd(const struct http_context *context,int fd) {
  if (!context||(fd<0)) return 0;
  int i=context->serverc;
  while (i-->0) {
    struct http_server *server=context->serverv[i];
    if (server->fd==fd) return server;
  }
  return 0;
}
 
struct http_socket *http_context_get_socket_by_fd(const struct http_context *context,int fd) {
  if (!context||(fd<0)) return 0;
  int i=context->socketc;
  while (i-->0) {
    struct http_socket *socket=context->socketv[i];
    if (socket->fd==fd) return socket;
  }
  return 0;
}

/* Create server, public convenience.
 */
 
struct http_server *http_serve(struct http_context *context,int port) {
  if (port<1) return 0;
  struct http_server *server=http_context_add_new_server(context);
  if (!server) return 0;
  
  if (
    (http_server_init_tcp_stream(server)<0)||
    (http_server_bind(server,"localhost",port)<0)||
    (http_server_listen(server,10)<0)
  ) {
    http_context_remove_server(context,server);
    return 0;
  }

  return server;
}

/* Create listener, public convenience.
 */

struct http_listener *http_listen(
  struct http_context *context,
  int method,
  const char *path,
  int (*cb)(struct http_xfer *req,struct http_xfer *rsp,void *userdata),
  void *userdata
) {
  if ((method<0)||!cb) return 0;
  struct http_listener *listener=http_context_add_new_listener(context);
  if (!listener) return 0;
  
  listener->method=method;
  listener->cb_serve=cb;
  listener->userdata=userdata;
  
  if (http_listener_set_path(listener,path,-1)<0) {
    http_context_remove_listener(context,listener);
    return 0;
  }
  
  return listener;
}

/* Create websocket listener, public convenience.
 */

struct http_listener *http_listen_websocket(
  struct http_context *context,
  const char *path,
  int (*cb_connect)(struct http_socket *socket,void *userdata),
  int (*cb_disconnect)(struct http_socket *socket,void *userdata),
  int (*cb_message)(struct http_socket *socket,int type,const void *v,int c,void *userdata),
  void *userdata
) {
  struct http_listener *listener=http_context_add_new_listener(context);
  if (!listener) return 0;
  
  listener->method=HTTP_METHOD_WEBSOCKET;
  listener->cb_connect=cb_connect;
  listener->cb_disconnect=cb_disconnect;
  listener->cb_message=cb_message;
  listener->userdata=userdata;
  
  if (http_listener_set_path(listener,path,-1)<0) {
    http_context_remove_listener(context,listener);
    return 0;
  }
  
  return listener;
}

/* Find listener for request.
 */
 
struct http_listener *http_context_find_listener_for_request(
  const struct http_context *context,
  const struct http_xfer *req
) {
  if (!context||!req) return 0;
  
  // Get the request path and method as stated.
  const char *path=0;
  int pathc=http_xfer_get_path(&path,req);
  if (pathc<0) return 0;
  int method=http_xfer_get_method(req);
  
  // If it's a WebSocket upgrade request, find WebSocket listeners only.
  if (method==HTTP_METHOD_GET) {
    const char *connection=0;
    int connectionc=http_xfer_get_header(&connection,req,"Connection",10);
    if ((connectionc==7)&&!memcmp(connection,"Upgrade",7)) {
      const char *upgrade=0;
      int upgradec=http_xfer_get_header(&upgrade,req,"Upgrade",7);
      if ((upgradec==9)&&!memcmp(upgrade,"websocket",9)) {
        method=HTTP_METHOD_WEBSOCKET;
      }
    }
  }
  
  // Find a matching listener.
  struct http_listener **p=context->listenerv;
  int i=context->listenerc;
  for (;i-->0;p++) {
    struct http_listener *listener=*p;
    if (listener->method) {
      if (listener->method!=method) continue;
    } else {
      if (method==HTTP_METHOD_WEBSOCKET) continue; // WEBSOCKET doesn't match wildcard methods.
    }
    if (listener->pathc&&!http_wildcard_match(listener->path,listener->pathc,path,pathc)) continue;
    return listener;
  }
  return 0;
}

/* Extra file descriptors, where we lend out our poller.
 */
 
int http_context_add_fd(struct http_context *context,int fd,int (*cb)(int fd,void *userdata),void *userdata) {
  if (!context||!cb||(fd<0)) return -1;
  int i=context->extfdc;
  while (i-->0) if (context->extfdv[i].fd==fd) return -1;
  if (context->extfdc>=context->extfda) {
    int na=context->extfda+16;
    if (na>INT_MAX/sizeof(struct http_extfd)) return -1;
    void *nv=realloc(context->extfdv,sizeof(struct http_extfd)*na);
    if (!nv) return -1;
    context->extfdv=nv;
    context->extfda=na;
  }
  struct http_extfd *extfd=context->extfdv+context->extfdc++;
  extfd->fd=fd;
  extfd->userdata=userdata;
  extfd->cb=cb;
  return 0;
}

void http_context_remove_fd(struct http_context *context,int fd) {
  if (!context) return;
  int i=context->extfdc;
  while (i-->0) {
    struct http_extfd *extfd=context->extfdv+i;
    if (extfd->fd==fd) {
      context->extfdc--;
      memmove(extfd,extfd+1,sizeof(struct http_extfd)*(context->extfdc-i));
      return;
    }
  }
}

struct http_extfd *http_context_get_extfd_by_fd(const struct http_context *context,int fd) {
  struct http_extfd *extfd=context->extfdv;
  int i=context->extfdc;
  for (;i-->0;extfd++) if (extfd->fd==fd) return extfd;
  return 0;
}
