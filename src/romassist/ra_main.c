#include "ra_internal.h"
#include <time.h>
#include <signal.h>
#include <unistd.h>

struct ra ra={0};

/* Signal handler.
 */
 
static void ra_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(ra.sigc)>=3) {
        fprintf(stderr,"Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* Init database.
 */
 
static int ra_init_db() {
  if (!ra.dbroot) {
    fprintf(stderr,"%s: Please provide --dbroot=PATH\n",ra.exename);
    return -2;
  }
  if (!(ra.db=db_new(ra.dbroot))) return -1;
  fprintf(stderr,"%s: Opened database at %s.\n",ra.exename,ra.dbroot);
  
  /* Opportunity for one-off DB actions that I don't feel like exposing the right way.
   */
  if (0) {
    // eg, delete all games for certain platforms, after verifying they have no metadata
    uint32_t str_c64=db_string_lookup(ra.db,"c64",3);
    uint32_t str_genesis=db_string_lookup(ra.db,"genesis",7);
    uint32_t str_n64=db_string_lookup(ra.db,"n64",3);
    uint32_t str_atari5200=db_string_lookup(ra.db,"atari5200",9);
    uint32_t str_scv=db_string_lookup(ra.db,"scv",3);
    int rmc=0;
    int totalc=db_game_count(ra.db);
    const struct db_game *game=db_game_get_by_index(ra.db,totalc-1);
    int i=db_game_count(ra.db);
    for (;i-->0;game--) {
      if (
        (game->platform==str_c64)||
        (game->platform==str_genesis)||
        (game->platform==str_n64)||
        (game->platform==str_atari5200)||
        (game->platform==str_scv)
      ) {
        rmc++;
        if (game->rating||game->pubtime||game->genre||game->author) {
          fprintf(stderr,"!!! game %d '%s' has some nonzero fields\n",game->gameid,game->name);
        }
        db_game_delete(ra.db,game->gameid);
      }
    }
    db_save(ra.db);
    fprintf(stderr,"Deleted %d of %d games.\n",rmc,totalc);
    fprintf(stderr,"db action complete. reporting failure to abort process.\n");
    return -1;
  }
  
  return 0;
}

/* Init HTTP server.
 */
 
static int ra_init_http() {
  if (!(ra.http=http_context_new())) return -1;
  if (!http_serve(ra.http,ra.http_port)) {
    fprintf(stderr,"%s: Failed to open TCP server on port %d.\n",ra.exename,ra.http_port);
    return -2;
  }
  
  if (!http_listen(ra.http,0,"/api/**",ra_http_api,0)) return -1;
  if (!http_listen_websocket(ra.http,"/ws/menu",ra_ws_connect_menu,ra_ws_disconnect,ra_ws_message,0)) return -1;
  if (!http_listen_websocket(ra.http,"/ws/game",ra_ws_connect_game,ra_ws_disconnect,ra_ws_message,0)) return -1;
  if (!http_listen(ra.http,HTTP_METHOD_GET,"/**",ra_http_static,0)) return -1;
  
  fprintf(stderr,"%s: Serving HTTP on port %d.\n",ra.exename,ra.http_port);
  return 0;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  int status=0;
  
  signal(SIGINT,ra_rcvsig);
  srand(time(0));
  if (ra_configure(argc,argv)<0) { status=1; goto _done_; }
  if (ra_init_db()<0) { status=1; goto _done_; }
  if (ra_init_http()<0) { status=1; goto _done_; }
  
  fprintf(stderr,"%s: Running.\n",ra.exename);
  
  while (1) {
    if (ra.sigc) break;
    if (http_update(ra.http,1000)<0) {
      fprintf(stderr,"%s: Error updating HTTP (could be anything).\n",ra.exename);
      status=1;
      goto _done_;
    }
    if (db_save(ra.db)<0) {
      fprintf(stderr,"%s:!!! Error saving database. Will keep open and try again soon.\n",ra.exename);
    }
    if (ra_process_update(&ra.process)<0) {
      fprintf(stderr,"%s: Error updating child process.\n",ra.exename);
      status=1;
      goto _done_;
    }
  }
  
 _done_:;

  // It seems ridiculous, but I'm finding under GLX, the menu process typically takes 300-700 ms to terminate.
  // Why is it so long???
  ra_process_terminate_and_wait(&ra.process,1000);
  
  if (!status) db_save(ra.db);
  db_del(ra.db);
  http_context_del(ra.http);
  ra_process_cleanup(&ra.process);

  if (status) fprintf(stderr,"%s: Abnormal exit.\n",ra.exename);
  else fprintf(stderr,"%s: Normal exit.\n",ra.exename);
  return status;
}
