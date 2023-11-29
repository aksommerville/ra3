#include "http_internal.h"
#include <fcntl.h>
#include <unistd.h>

/* Delete.
 */
 
void http_server_del(struct http_server *server) {
  if (!server) return;
  if (server->refc-->1) return;
  
  if (server->fd>=0) close(server->fd);
  
  free(server);
}

/* Retain.
 */
 
int http_server_ref(struct http_server *server) {
  if (!server) return -1;
  if (server->refc<1) return -1;
  if (server->refc>=INT_MAX) return -1;
  server->refc++;
  return 0;
}

/* New.
 */
 
struct http_server *http_server_new(struct http_context *context) {
  if (!context) return 0;
  struct http_server *server=calloc(1,sizeof(struct http_server));
  if (!server) return 0;
  
  server->refc=1;
  server->context=context;
  server->fd=-1;
  
  return server;
}

/* Setup.
 */
 
int http_server_init_tcp_stream(struct http_server *server) {
  if (!server||(server->fd>=0)) return -1;
  if ((server->fd=socket(PF_INET,SOCK_STREAM,IPPROTO_TCP))<0) return -1;
  int one=1;
  // Cast to (void*) because in Windows it is typed 'char*' for fuck knows what reason...
  setsockopt(server->fd,SOL_SOCKET,SO_REUSEADDR,(void*)&one,sizeof(one));
  return 0;
}

int http_server_bind(struct http_server *server,const char *host,int port) {
  if (!server||(server->fd<0)) return -1;
  //if (!host||!host[0]) host="localhost";
  struct addrinfo hints={
    .ai_family=AF_INET,
    .ai_socktype=SOCK_STREAM,
    .ai_protocol=IPPROTO_TCP,
    .ai_flags=AI_PASSIVE|AI_NUMERICSERV|AI_ADDRCONFIG,
  };
  char service[16];
  snprintf(service,sizeof(service),"%d",port);
  struct addrinfo *ai=0;
  if (getaddrinfo(host,service,&hints,&ai)<0) return -1;
  int err=bind(server->fd,ai->ai_addr,ai->ai_addrlen);
  freeaddrinfo(ai);
  if (err<0) return -1;
  return 0;
}

int http_server_listen(struct http_server *server,int clientc) {
  if (!server||(server->fd<0)) return -1;
  if (listen(server->fd,clientc)<0) return -1;
  return 0;
}

/* Accept.
 */
 
int http_server_accept(struct http_server *server) {
  if (!server||(server->fd<0)) return -1;
  char sockaddr[256];
  socklen_t sockaddrc=sizeof(sockaddr);
  int clientfd=accept(server->fd,(struct sockaddr*)sockaddr,&sockaddrc);
  if (clientfd<0) {
    fprintf(stderr,"accept failed: %m\n");
    http_context_remove_server(server->context,server);
    return 0;
  }
  struct http_socket *socket=http_context_add_new_socket(server->context,clientfd);
  if (!socket) {
    close(clientfd);
    return -1;
  }
  return 0;
}
