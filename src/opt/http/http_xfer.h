/* http_xfer.h
 * Private.
 */
 
#ifndef HTTP_XFER_H
#define HTTP_XFER_H

#define HTTP_XFER_STATE_UNSET 0
#define HTTP_XFER_STATE_RCV_LINE 1 /* Awaiting Request-Line or Status-Line */
#define HTTP_XFER_STATE_RCV_HEADER 2
#define HTTP_XFER_STATE_RCV_BODY 3
#define HTTP_XFER_STATE_READY 4 /* Everything received */
#define HTTP_XFER_STATE_SEND 5 /* Outgoing xfer only */
#define HTTP_XFER_STATE_DEFERRED 6 /* Response pending from app */
#define HTTP_XFER_STATE_DEFERRAL_COMPLETE 7 /* App response ready */

struct http_xfer {
  int refc;
  struct http_context *context;
  int state;
  
  char *line; // Request-Line or Status-Line
  int linec;
  struct http_dict headers;
  char *body;
  int bodyc,bodya;
  
  int body_pendingc; // remaining body length
  int chunked;
};
 
void http_xfer_del(struct http_xfer *xfer);
int http_xfer_ref(struct http_xfer *xfer);

struct http_xfer *http_xfer_new(struct http_context *context);

int http_xfer_set_line(struct http_xfer *xfer,const char *src,int srcc);

int http_socket_encode_xfer(struct http_socket *socket,struct http_xfer *xfer);

#endif
