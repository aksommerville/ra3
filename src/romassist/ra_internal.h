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
#include "ra_upgrade.h"

// I doubt we'll ever see more than 2 at a time. 8 is plenty.
#define RA_WEBSOCKET_LIMIT 8

#define RA_WEBSOCKET_ROLE_NONE 0 /* extra slot not in use */
#define RA_WEBSOCKET_ROLE_MENU 1
#define RA_WEBSOCKET_ROLE_GAME 2

/* So many consecutive terminations of the menu with the same db timestamp (ie 1 minute),
 * if no game launched in between, we abort hard.
 * Protects against errors in the menu, where we would otherwise get stuck in a loop starting it up again.
 */
#define RA_MENU_TERM_LIMIT 5

extern struct ra {

  // Populated at configure.
  const char *exename;
  char *dbroot;
  char *htdocs;
  char *menu;
  int http_port;
  int terminable;
  int allow_poweroff; // If nonzero, POST /api/shutdown calls `poweroff`. Zero, only this process terminates.
  
  volatile int sigc;
  struct db *db;
  struct http_context *http;
  struct ra_process process;
  uint32_t menu_termv[RA_MENU_TERM_LIMIT]; // timestamps of menu terminations since the last game launch.
  int menu_termc;
  struct ra_upgrade upgrade;
  
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

int ra_websocket_send_to_role(int role,int packet_type,const void *v,int c);

/* The meat of the operation, for one file.
 * Some additional outer layers live in ra_http.c.
 */
int ra_autoscreencap(uint32_t gameid,const char *rompath);

/* Tell menu clients this this game is now running.
 */
int ra_report_gameid(uint32_t gameid);

#endif
