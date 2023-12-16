/* http_context.h
 * Private.
 */
 
#ifndef HTTP_CONTEXT_H
#define HTTP_CONTEXT_H

struct http_context {
  int refc;
  struct pollfd *pollfdv;
  int pollfdc,pollfda;
  
  struct http_listener **listenerv;
  int listenerc,listenera;
  struct http_server **serverv;
  int serverc,servera;
  struct http_socket **socketv;
  int socketc,socketa;
  
  struct http_extfd {
    int fd;
    void *userdata;
    int (*cb)(int fd,void *userdata);
  } *extfdv;
  int extfdc,extfda;
};

struct pollfd *http_context_pollfdv_require(struct http_context *context);

struct http_listener *http_context_add_new_listener(struct http_context *context);
void http_context_remove_listener(struct http_context *context,struct http_listener *listener);

struct http_server *http_context_add_new_server(struct http_context *context);
void http_context_remove_server(struct http_context *context,struct http_server *server);

struct http_socket *http_context_add_new_socket(struct http_context *context,int fd);
void http_context_remove_socket(struct http_context *context,struct http_socket *socket);
void http_context_drop_all_sockets(struct http_context *context);

struct http_server *http_context_get_server_by_fd(const struct http_context *context,int fd);
struct http_socket *http_context_get_socket_by_fd(const struct http_context *context,int fd);
struct http_extfd *http_context_get_extfd_by_fd(const struct http_context *context,int fd);

struct http_listener *http_context_find_listener_for_request(
  const struct http_context *context,
  const struct http_xfer *req
);

#endif
