/* http_xfer.h
 * Private.
 */
 
#ifndef HTTP_XFER_H
#define HTTP_XFER_H

#include "opt/serial/serial.h"

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
  struct sr_encoder body;
  
  int body_pendingc; // remaining body length
  int chunked;
  
  struct sockaddr raddr; // Clients only.
};
 
void http_xfer_del(struct http_xfer *xfer);
int http_xfer_ref(struct http_xfer *xfer);

/* This is safe to use, and doesn't actually depend on the context for anything.
 * But it's not a normal thing to do; normally context creates and destroys all xfers on its own.
 */
struct http_xfer *http_xfer_new(struct http_context *context);

int http_xfer_set_line(struct http_xfer *xfer,const char *src,int srcc);

int http_socket_encode_xfer(struct http_socket *socket,struct http_xfer *xfer);

#endif
