/* http_socket.h
 * Private.
 */
 
#ifndef HTTP_SOCKET_H
#define HTTP_SOCKET_H

#define HTTP_PROTOCOL_UNSET 0
#define HTTP_PROTOCOL_HTTP_SERVER 1
#define HTTP_PROTOCOL_HTTP_CLIENT 2
#define HTTP_PROTOCOL_WEBSOCKET 3
#define HTTP_PROTOCOL_FAKEWEBSOCKET 4

struct http_socket {
  int refc;
  struct http_context *context;
  int fd;
  int protocol;
  
  /* Two transfer buffers.
   * These have a moving head and tail but are not circular.
   */
  char *rbuf,*wbuf;
  int rbufp,rbufc,rbufa;
  int wbufp,wbufc,wbufa;
  
  /* Logical request and response containers.
   * Both required if we are conducting an HTTP transaction.
   * (req) is present for WebSocket connections too.
   */
  struct http_xfer *req,*rsp;
  
  /* The listener that matched (req), if we've gotten that far.
   */
  struct http_listener *listener;
  
  /* These callbacks are relevant to client-side WebSockets.
   */
  void *userdata;
  int (*cb_disconnect)(struct http_socket *socket,void *userdata);
  int (*cb_message)(struct http_socket *socket,int type,const void *v,int c,void *userdata);
};
 
void http_socket_del(struct http_socket *socket);
int http_socket_ref(struct http_socket *socket);

struct http_socket *http_socket_new(struct http_context *context);

/* Grow buffers.
 * These ensure that at least (addc) bytes available at the tail.
 * Returns actual amount available (>=addc) or <0 on errors.
 * Optionally return a pointer to the tail (v+p+c).
 */
int http_socket_rbuf_require(void *tailpp,struct http_socket *socket,int addc);
int http_socket_wbuf_require(void *tailpp,struct http_socket *socket,int addc);

/* Conveniences to examine and advance the read buffer head.
 */
int http_socket_rbuf_get(void *headpp,const struct http_socket *socket);
int http_socket_rbuf_consume(struct http_socket *socket,int c);

/* Conveniences to append to the output buffer.
 */
int http_socket_wbuf_append(struct http_socket *socket,const void *src,int srcc);
int http_socket_wbuf_appendf(struct http_socket *socket,const char *fmt,...);

/* Flush content between the file and my buffers.
 * Owner should call these whenever the socket polls.
 */
int http_socket_read(struct http_socket *socket);
int http_socket_write(struct http_socket *socket);

/* Call after reading, to advance protocol-level activity.
 */
int http_socket_digest_input(struct http_socket *socket);

/* Reacting to a read failure, eg remote end closed.
 * Should it be a hard error or was this a reasonable time to close it?
 */
int http_socket_ok_to_close(const struct http_socket *socket);

#endif
