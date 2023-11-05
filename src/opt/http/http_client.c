#include "http_internal.h"

/* Requests with implicit method.
 */
 
struct http_xfer *http_get(
  struct http_context *context,
  const char *url,int urlc,
  int (*cb)(struct http_xfer *rsp,void *userdata),
  void *userdata
) {
  return 0;//TODO
}

struct http_xfer *http_post(
  struct http_context *context,
  const char *url,int urlc,
  const char *content_type,
  const void *body,int bodyc,
  int (*cb)(struct http_xfer *rsp,void *userdata),
  void *userdata
) {
  return 0;//TODO
}

struct http_xfer *http_put(
  struct http_context *context,
  const char *url,int urlc,
  const char *content_type,
  const void *body,int bodyc,
  int (*cb)(struct http_xfer *rsp,void *userdata),
  void *userdata
) {
  return 0;//TODO
}

struct http_xfer *http_delete(
  struct http_context *context,
  const char *url,int urlc,
  int (*cb)(struct http_xfer *rsp,void *userdata),
  void *userdata
) {
  return 0;//TODO
}

/* General HTTP request.
 */

struct http_xfer *http_request(
  struct http_context *context,
  const char *host,int hostc,int port,
  int (*cb)(struct http_xfer *rsp,void *userdata),
  void *userdata
) {
  return 0;//TODO
}

/* Cancel request.
 */

void http_context_cancel_request(struct http_context *context,struct http_xfer *xfer) {
  //TODO
}

/* Connect WebSocket.
 */

struct http_socket *http_websocket_connect(
  struct http_context *context,
  const char *url,int urlc,
  int (*cb_disconnect)(struct http_socket *socket,void *userdata),
  int (*cb_message)(struct http_socket *socket,int type,const void *v,int c,void *userdata),
  void *userdata
) {
  return 0;//TODO
}
