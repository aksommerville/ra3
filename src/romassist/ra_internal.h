#ifndef RA_INTERNAL_H
#define RA_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include "opt/db/db.h"
#include "opt/http/http.h"
#include "ra_process.h"

extern struct ra {

  // Populated at configure.
  const char *exename;
  char *dbroot;
  char *htdocs;
  char *menu;
  int http_port;
  
  volatile int sigc;
  struct db *db;
  struct http_context *http;
  struct ra_process process;
  
} ra;

int ra_configure(int argc,char **argv);

// HTTP callbacks.
int ra_http_static(struct http_xfer *req,struct http_xfer *rsp,void *userdata);
int ra_http_api(struct http_xfer *req,struct http_xfer *rsp,void *userdata);
int ra_ws_connect(struct http_socket *sock,void *userdata);
int ra_ws_disconnect(struct http_socket *sock,void *userdata);
int ra_ws_message(struct http_socket *sock,int type,const void *v,int c,void *userdata);

#endif
