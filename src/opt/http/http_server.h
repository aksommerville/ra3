/* http_server.h
 * Private.
 */
 
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

struct http_server {
  int refc;
  struct http_context *context;
  int fd;
  int open_to_public;
  int port;
};
 
void http_server_del(struct http_server *server);
int http_server_ref(struct http_server *server);

struct http_server *http_server_new(struct http_context *context);

int http_server_init_tcp_stream(struct http_server *server);
int http_server_bind(struct http_server *server,const char *host,int port,int open_to_public);
int http_server_listen(struct http_server *server,int clientc);

/* Accept one connection, update all state accordingly.
 * Normally creates a new socket in the context.
 */
int http_server_accept(struct http_server *server);

#endif
