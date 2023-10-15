/* http_listener.h
 * Private.
 */
 
#ifndef HTTP_LISTENER_H
#define HTTP_LISTENER_H

struct http_listener {
  int refc;
  struct http_context *context;
  
  int method;
  char *path; // with wildcards
  int pathc;
  
  int (*cb_serve)(struct http_xfer *req,struct http_xfer *rsp,void *userdata);
  int (*cb_connect)(struct http_socket *socket,void *userdata);
  int (*cb_disconnect)(struct http_socket *socket,void *userdata);
  int (*cb_message)(struct http_socket *socket,int type,const void *v,int c,void *userdata);
  void *userdata;
};
 
void http_listener_del(struct http_listener *listener);
int http_listener_ref(struct http_listener *listener);

struct http_listener *http_listener_new(struct http_context *context);

int http_listener_set_path(struct http_listener *listener,const char *src,int srcc);

#endif
