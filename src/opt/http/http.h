/* http.h
 * Simple HTTP/WebSocket client and server.
 * No security! Do not use on the public internet!
 */
 
#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>

struct http_context;
struct http_server;
struct http_socket;
struct http_listener;
struct http_xfer; // request or response
struct http_dict; // headers
struct sr_encoder;

#define HTTP_METHOD_GET       1
#define HTTP_METHOD_POST      2
#define HTTP_METHOD_PUT       3
#define HTTP_METHOD_PATCH     4
#define HTTP_METHOD_DELETE    5
#define HTTP_METHOD_HEAD      6
#define HTTP_METHOD_UNKNOWN   7 /* something else, not necessarily the word "UNKNOWN". but also that */
#define HTTP_METHOD_WEBSOCKET 8 /* internal use */

/* Context: Global coordination object.
 * You should create one of these, for the program's lifetime.
 * All other objects are associated with a context.
 */
void http_context_del(struct http_context *context);
int http_context_ref(struct http_context *context);
struct http_context *http_context_new();

/* Update everything that can be updated and trigger callbacks as warranted.
 * May sleep for (toms) milliseconds.
 * Zero to return immediately if no I/O possible, or negative to wait forever.
 */
int http_update(struct http_context *context,int toms);

/* You may hijack the context's poll for arbitrary input files.
 * (input only, for now at least).
 * (fd,userdata) are borrowed weakly by the context.
 */
int http_context_add_fd(struct http_context *context,int fd,int (*cb)(int fd,void *userdata),void *userdata);
void http_context_remove_fd(struct http_context *context,int fd);

/* Servers.
 **************************************************************************/

/* List all servers. Also a reasonable way to ask "are there any servers?".
 * Servers which fail at accept() will quietly remove themselves.
 * If you want to fail in that case, reasonable, but you have to poll for it.
 */
struct http_server *http_context_get_server_by_index(const struct http_context *context,int p);

/* Create a server and start listening.
 * We will only listen on localhost, as this library is not suitable for facing the public internet.
 * You'll normally do this once per context. But can listen on as many ports as you like.
 * Returns WEAK.
 * TODO I copied this from Full Moon, which indeed should be localhost-only. Maybe relax that restriction for Romassist.
 */
struct http_server *http_serve(struct http_context *context,int port);

/* Create a listener and attach to some method and path.
 * Listeners are tested in the order you add them, and only the first match wins.
 * So if you have a catch-all listener, install it last.
 * (method) may be zero to match all.
 * In (path), a single star matches between slashes, and two stars match any amount of anything.
 * (path) does not include the query string.
 * *** For efficiency's sake, we do not decode percent-escapes in the path. ***
 * It's expected that anything addressable shouldn't need to be escaped.
 * You get just one callback, when the request body has been fully received.
 */
struct http_listener *http_listen(
  struct http_context *context,
  int method,
  const char *path,
  int (*cb)(struct http_xfer *req,struct http_xfer *rsp,void *userdata),
  void *userdata
);

/* Create a listener for WebSocket connections.
 * We trigger (cb_connect) after the handshake succeeds.
 */
struct http_listener *http_listen_websocket(
  struct http_context *context,
  const char *path,
  int (*cb_connect)(struct http_socket *socket,void *userdata),
  int (*cb_disconnect)(struct http_socket *socket,void *userdata),
  int (*cb_message)(struct http_socket *socket,int type,const void *v,int c,void *userdata),
  void *userdata
);

/* You don't need to close websockets that triggered (cb_disconnect).
 * And (cb_disconnect) does not trigger for ones you close explicitly.
 * "Sending" only queues the packet for delivery, I/O begins at the next context update.
 */
int http_websocket_close(struct http_socket *socket);
int http_websocket_send(struct http_socket *socket,int type,const void *v,int c);

/* Userdata for all sockets is null by default.
 * We never touch it.
 */
void *http_socket_get_userdata(struct http_socket *socket);
void http_socket_set_userdata(struct http_socket *socket,void *userdata);

/* Selected API for reading request xfers.
 * More details are available in http_xfer.h.
 * If you are reading a bunch of query params, it's way more efficient to iterate than to get each individually.
 * Query parameters longer than some internal limit will be quietly ignored. Currently 1024 bytes.
 */
int http_xfer_get_status(const struct http_xfer *xfer);
int http_xfer_get_status_message(void *dstpp,const struct http_xfer *xfer);
int http_xfer_get_method(const struct http_xfer *xfer);
int http_xfer_get_path(void *dstpp,const struct http_xfer *xfer); // no query; may contain escapes
int http_xfer_get_header(void *dstpp,const struct http_xfer *xfer,const char *k,int kc);
int http_xfer_get_header_int(int *dst,const struct http_xfer *xfer,const char *k,int kc);
int http_xfer_get_body(void *dstpp,const struct http_xfer *xfer);
int http_xfer_for_query(const struct http_xfer *xfer,int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),void *userdata); // => (k,v) encoded
int http_xfer_get_query_string(char *dst,int dsta,const struct http_xfer *xfer,const char *k,int kc);
int http_xfer_get_query_int(int *dst,const struct http_xfer *xfer,const char *k,int kc);

/* Selected API for writing response xfers.
 * More details are available in http_xfer.h
 * Response status is 200 if you don't set it.
 */
int http_xfer_set_status(struct http_xfer *xfer,int code,const char *fmt,...);
int http_xfer_set_header(struct http_xfer *xfer,const char *k,int kc,const char *v,int vc);
int http_xfer_set_header_int(struct http_xfer *xfer,const char *k,int kc,int v);
int http_xfer_append_body(struct http_xfer *xfer,const void *src,int srcc);
int http_xfer_append_bodyf(struct http_xfer *xfer,const char *fmt,...);
struct sr_encoder *http_xfer_get_body_encoder(struct http_xfer *xfer);

/* Normally, a listener fills its response synchronously.
 * If you need to delay, eg to make a callout of your own, call "hold" before returning from the listener callback,
 * then call "ready" at any time in the future when you've finished processing it.
 */
int http_xfer_hold(struct http_xfer *xfer);
int http_xfer_ready(struct http_xfer *xfer);
int http_xfer_ref(struct http_xfer *xfer);
void http_xfer_del(struct http_xfer *xfer);

/* Clients.
 * XXX I didn't write this stuff the first time thru, just the headers, and now
 * I'm thinking it would be better to split client stuff into a separate unit.
 * No sense implementing all kinds of server logic for an app that only acts as a client.
 ****************************************************************************/

/* Conveniences to begin a request with a specific method.
 * You should provide a full encoded URL eg "http://www.google.com/search%20for%20text?q=penguin%21"
 * If you leave off the scheme, that's fine, we'll figure it out.
 * DNS and TCP connection happen synchronously during this call; all transfers are deferred to the next context update.
 * (cb) will fire exactly once, during a future context update or cleanup.
 * All return a WEAK reference to the request xfer on success; normally all you care about is null or not.
 */
struct http_xfer *http_get(
  struct http_context *context,
  const char *url,int urlc,
  int (*cb)(struct http_xfer *rsp,void *userdata),
  void *userdata
);
struct http_xfer *http_post(
  struct http_context *context,
  const char *url,int urlc,
  const char *content_type,
  const void *body,int bodyc,
  int (*cb)(struct http_xfer *rsp,void *userdata),
  void *userdata
);
struct http_xfer *http_put(
  struct http_context *context,
  const char *url,int urlc,
  const char *content_type,
  const void *body,int bodyc,
  int (*cb)(struct http_xfer *rsp,void *userdata),
  void *userdata
);
struct http_xfer *http_delete(
  struct http_context *context,
  const char *url,int urlc,
  int (*cb)(struct http_xfer *rsp,void *userdata),
  void *userdata
);

/* Begin a request, general case.
 * You must populate this xfer before the next update, or cancel it.
 * Returns WEAK.
 */
struct http_xfer *http_request(
  struct http_context *context,
  const char *host,int hostc,int port,
  int (*cb)(struct http_xfer *rsp,void *userdata),
  void *userdata
);

void http_context_cancel_request(struct http_context *context,struct http_xfer *xfer);

int http_xfer_set_header(struct http_xfer *xfer,const char *k,int kc,const char *v,int vc);
int http_xfer_append_body(struct http_xfer *xfer,const void *src,int srcc);
int http_xfer_set_body(struct http_xfer *xfer,const void *src,int srcc);
int http_xfer_append_bodyf(struct http_xfer *xfer,const char *fmt,...);

struct http_socket *http_websocket_connect(
  struct http_context *context,
  const char *url,int urlc,
  int (*cb_disconnect)(struct http_socket *socket,void *userdata),
  int (*cb_message)(struct http_socket *socket,int type,const void *v,int c,void *userdata),
  void *userdata
);

/* General helpers.
 *************************************************************************/
 
const char *http_method_repr(int method);
int http_method_eval(const char *src,int srcc);
int http_url_encode(char *dst,int dsta,const char *src,int srcc);
int http_url_decode(char *dst,int dsta,const char *src,int srcc);
int http_int_eval(int *dst,const char *src,int srcc);
int http_memcasecmp(const void *a,const void *b,int c);
int http_measure_space(const char *src,int srcc);
int http_measure_line(const char *src,int srcc); // => thru \r\n, or zero
int http_wildcard_match(const char *pat,int patc,const char *src,int srcc);

#endif
