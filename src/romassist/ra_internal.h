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

// I doubt we'll ever see more than 2 at a time. 8 is plenty.
#define RA_WEBSOCKET_LIMIT 8

#define RA_WEBSOCKET_ROLE_NONE 0 /* extra slot not in use */
#define RA_WEBSOCKET_ROLE_MENU 1
#define RA_WEBSOCKET_ROLE_GAME 2

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
  
  struct ra_websocket_extra {
    int role;
    struct http_socket *socket; // WEAK. socket's userdata points back to this struct.
    int64_t screencap_request_time;
  } websocket_extrav[RA_WEBSOCKET_LIMIT];
  uint32_t gameid_reported; // What our menus currently think is running.
  
} ra;

int ra_configure(int argc,char **argv);

// HTTP callbacks.
int ra_http_static(struct http_xfer *req,struct http_xfer *rsp,void *userdata);
int ra_http_api(struct http_xfer *req,struct http_xfer *rsp,void *userdata);
int ra_ws_connect_menu(struct http_socket *sock,void *userdata);
int ra_ws_connect_game(struct http_socket *sock,void *userdata);
int ra_ws_disconnect(struct http_socket *sock,void *userdata);
int ra_ws_message(struct http_socket *sock,int type,const void *v,int c,void *userdata);

/* The meat of the operation, for one file.
 * Some additional outer layers live in ra_http.c.
 */
int ra_autoscreencap(uint32_t gameid,const char *rompath);

/* Tell menu clients this this game is now running.
 */
int ra_report_gameid(uint32_t gameid);

#endif
