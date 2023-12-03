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
  int update_enable;
  char *migrate; // "host:port", to pull content from that installation, instead of normal operation
  
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

/* Initialize DB first. Not HTTP or Upgrade.
 * Returns exit status.
 */
int ra_migrate_main();

/* Structured helper for uploading games.
 * Caller should populate the Input part, then call ra_game_upload_prepare.
 * Preparing selects a local path, a platform, and a display name.
 * Preparing may intern strings but otherwise doesn't modify the db.
 * After preparing, ra_game_upload_commit to write the file and add a game record to the db.
 * (serial) is not required at prepare.
 * Maybe we'll use it eventually for format detection or something, but null will always be legal.
 */
struct ra_game_upload {
// Input:
  const char *base;
  int basec;
  const char *platform;
  int platformc;
  const void *serial; // May be null if only the length is known.
  int serialc;
// Output:
  char *path; // Must be NUL-terminated, in addition to the explicit length below.
  int pathc;
  char name[DB_GAME_NAME_LIMIT];
  uint32_t platform_stringid;
  uint32_t gameid;
};
void ra_game_upload_cleanup(struct ra_game_upload *upload);
int ra_game_upload_prepare(struct ra_game_upload *upload);
int ra_game_upload_commit(struct ra_game_upload *upload);

#endif
