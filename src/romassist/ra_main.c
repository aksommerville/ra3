#include "ra_internal.h"
#include "opt/http/http_context.h"
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
    #if 0
    struct db_wordcloud wc={.db=ra.db};
    if (db_wordcloud_gather(&wc)<0) {
      fprintf(stderr,"db_wordcloud_gather failed!\n");
      return -1;
    }
    db_wordcloud_filter_by_count(&wc,0,INT_MAX); // <-- TODO
    fprintf(stderr,"wordcloud found %d words in our %d games.\n",wc.entryc,db_game_count(ra.db));
    int dumpc=100; if (dumpc>wc.entryc) dumpc=wc.entryc;
    fprintf(stderr,"Top %d:\n",dumpc);
    const struct db_wordcloud_entry *entry=wc.entryv;
    int i=0; for (;i<dumpc;i++,entry++) {
      fprintf(stderr,"  %5d %.*s\n",entry->instc,entry->vc,entry->v);
    }
    db_wordcloud_cleanup(&wc);
    #endif
    #if 1
    struct db_detailgram dg={.db=ra.db};
    const char *field="genre";
    if (db_detailgram_gather(&dg,field)<0) {
      fprintf(stderr,"db_detailgram_gather failed!\n");
      return -1;
    }
    fprintf(stderr,"detailgram found %d values for '%s'\n",dg.entryc,field);
    //db_detailgram_filter_gamec(&dg,2,INT_MAX);
    db_detailgram_sort_rating_avg_desc(&dg);
    int dumpc=100; if (dumpc>dg.entryc) dumpc=dg.entryc;
    fprintf(stderr,"Top %d:\n",dumpc);//TODO actually "Top" is misleading here, they're not sorted meaningfully
    const struct db_detailgram_entry *entry=dg.entryv;
    int i=dumpc; for (;i-->0;entry++) {
      const char *name=0;
      int namec=db_string_get(&name,ra.db,entry->v);
      if (namec<0) namec=0;
      int yearlo=0,yearhi=0;
      db_time_unpack(&yearlo,0,0,0,0,entry->pubtime_lo);
      db_time_unpack(&yearhi,0,0,0,0,entry->pubtime_hi);
      fprintf(stderr,
        "  %30.*s: %4d games. rating=%2d..%2d~%2d. pubtime=%4d..%4d\n",
        namec,name,entry->gamec,
        entry->rating_lo,entry->rating_hi,entry->rating_avg,
        yearlo,yearhi
      );
    }
    #endif
    fprintf(stderr,"db action complete. reporting failure to abort process.\n");
    return -1;
  }
  
  return 0;
}

/* Init HTTP server.
 */
 
static int ra_init_http() {
  if (!(ra.http=http_context_new())) return -1;
  
  struct http_server *server=0;
  #define TRYSERVE \
    if (ra.http_port==ra.public_port) { \
      server=http_serve(ra.http,ra.http_port,1); \
      if (!server) { /* Couldn't get INADDR_ANY? Try localhost instead. */ \
        server=http_serve(ra.http,ra.http_port,0); \
      } \
    } else { \
      server=http_serve(ra.http,ra.http_port,0); \
    }
  TRYSERVE
  if (!server) {
    fprintf(stderr,"%s: Failed to open TCP server on port %d. Starting without, and we'll retry periodically.\n",ra.exename,ra.http_port);
  }
  #undef TRYSERVE
  
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
  
  if (ra.migrate) return ra_migrate_main();
  
  if (ra_init_http()<0) { status=1; goto _done_; }
  if (ra_upgrade_startup()<0) { status=1; goto _done_; }
  
  fprintf(stderr,"%s: Running.\n",ra.exename);
  
  while (1) {
    if (ra.sigc) break;

    // 2024-02-04: Desperately trying to get ra to boot with the pi. It has no network initially, so http_serve fails.
    // Can we retry that repeatedly until it works?
    if (!http_context_get_server_by_index(ra.http,0)) {
      if (http_serve(ra.http,ra.http_port,0)) {
        fprintf(stderr,"%s:%d !!! got http server on a deferred retry\n",__FILE__,__LINE__);
      }
    }

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
