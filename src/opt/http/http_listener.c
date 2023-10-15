#include "http_internal.h"

/* Delete.
 */
 
void http_listener_del(struct http_listener *listener) {
  if (!listener) return;
  if (listener->refc-->1) return;
  
  if (listener->path) free(listener->path);
  
  free(listener);
}

/* Retain.
 */
 
int http_listener_ref(struct http_listener *listener) {
  if (!listener) return -1;
  if (listener->refc<1) return -1;
  if (listener->refc>=INT_MAX) return -1;
  listener->refc++;
  return 0;
}

/* New.
 */
 
struct http_listener *http_listener_new(struct http_context *context) {
  if (!context) return 0;
  struct http_listener *listener=calloc(1,sizeof(struct http_listener));
  if (!listener) return 0;
  
  listener->refc=1;
  listener->context=context;
  
  return listener;
}

/* Set path.
 */
 
int http_listener_set_path(struct http_listener *listener,const char *src,int srcc) {
  if (!listener) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (listener->path) free(listener->path);
  listener->path=nv;
  listener->pathc=srcc;
  return 0;
}
