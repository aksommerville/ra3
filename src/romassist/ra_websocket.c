#include "ra_internal.h"

/* New WebSocket connection.
 */
 
int ra_ws_connect(struct http_socket *sock,void *userdata) {
  fprintf(stderr,"%s\n",__func__);
  return -1;
}

/* Lost WebSocket connection.
 */

int ra_ws_disconnect(struct http_socket *sock,void *userdata) {
  fprintf(stderr,"%s\n",__func__);
  return -1;
}

/* Content received via WebSocket.
 */

int ra_ws_message(struct http_socket *sock,int type,const void *v,int c,void *userdata) {
  fprintf(stderr,"%s\n",__func__);
  return -1;
}
