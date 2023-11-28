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
    struct db_play *play=db_play_get_by_index(ra.db,0);
    int i=db_play_count(ra.db);
    while (i-->0) fprintf(stderr,"%10d\n",play->dur_m);
    fprintf(stderr,"db action complete. reporting failure to abort process. playc=%d\n",db_play_count(ra.db));
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

/* Look for termination cases from the menu.
 * Returns <0 to fail hard -- looks like a failure loop.
 * 0 to terminate normally -- we're configured to terminate when user quits the menu.
 * >0 to proceed, we restore a sane state.
 */
 
static int ra_main_check_termination() {
  if (ra.process.gameid&&!ra.process.next_launch) {
    // Game is running, reset the menu-failure-termination counter.
    ra.menu_termc=0;
  }
  if (ra.process.menu_terminated) {
    ra.process.menu_terminated=0;
    if (ra.terminable) {
      fprintf(stderr,"%s: Menu terminated without a game queued. Terminating. (--no-terminable to reopen)\n",ra.exename);
      return 0;
    }
    if (ra.menu_termc>=RA_MENU_TERM_LIMIT) {
      memmove(ra.menu_termv,ra.menu_termv+1,sizeof(ra.menu_termv)-sizeof(ra.menu_termv[0]));
      ra.menu_termv[RA_MENU_TERM_LIMIT]=db_time_now();
    } else {
      ra.menu_termv[ra.menu_termc++]=db_time_now();
    }
    if (ra.menu_termc>=RA_MENU_TERM_LIMIT) {
      int i=RA_MENU_TERM_LIMIT,allsame=1;
      while (i-->1) if (ra.menu_termv[i]!=ra.menu_termv[0]) {
        allsame=0;
        break;
      }
      if (allsame) {
        fprintf(stderr,"%s: %d consecutive menu terminations within one minute. Aborting.\n",ra.exename,RA_MENU_TERM_LIMIT);
        return -1;
      }
    }
    fprintf(stderr,"%s: Will re-launch terminated menu.\n",ra.exename);
    return 1;
  }
  return 1;
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
  if (ra_upgrade_startup()<0) { status=1; goto _done_; }
  
  fprintf(stderr,"%s: Running.\n",ra.exename);
  
  while (1) {
    if (ra.sigc) break;
    if (http_update(ra.http,1000)<0) {
      fprintf(stderr,"%s: Error updating HTTP (could be anything).\n",ra.exename);
      status=1;
      break;
    }
    if (ra_upgrade_update()<0) {
      fprintf(stderr,"%s: Error performing upgrades.\n",ra.exename);
      status=1;
      break;
    }
    if (db_save(ra.db)<0) {
      fprintf(stderr,"%s:!!! Error saving database. Will keep open and try again soon.\n",ra.exename);
    }
    if (ra_process_update(&ra.process)<0) {
      fprintf(stderr,"%s: Error updating child process.\n",ra.exename);
      status=1;
      break;
    }
    switch (ra_main_check_termination()) {
      case -1: status=1; goto _done_;
      case 0: status=0; goto _done_;
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
