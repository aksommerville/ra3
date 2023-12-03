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

/* XXX One-off: Create alphabetical directories for pico8 and gameboy and move files as needed.
 * Before this, pico8 had all the files loose in one directory, and gameboy was called "gb" with its non-alpha named "_09"
 * After, my whole ROM collection will be formatted uniformly. (it matters, for bulk import and migration)
 */
 
#include "opt/fs/fs.h"
 
static int XXX_normalize_file_names() {
  fprintf(stderr,"Normalizing ROM file paths...\n");
  uint32_t str_pico8=db_string_lookup(ra.db,"pico8",5);
  uint32_t str_gameboy=db_string_lookup(ra.db,"gameboy",7);
  if (!str_pico8||!str_gameboy) {
    fprintf(stderr,"!!! strings not interned. 'pico8'=%d 'gameboy'=%d\n",str_pico8,str_gameboy);
    return -1;
  }
  int modc=0,otherc=0,alreadyc=0;
  char path[1024];
  int pathc;
  struct db_game *game=db_game_get_by_index(ra.db,0);
  int i=db_game_count(ra.db);
  for (;i-->0;game++) {
    char prefix[3]={0};
         if ((game->base[0]>='a')&&(game->base[0]<='z')) prefix[0]=game->base[0];
    else if ((game->base[0]>='A')&&(game->base[0]<='Z')) prefix[0]=game->base[0]+0x20;
    else if ((game->base[0]>='0')&&(game->base[0]<='9')) { prefix[0]='0'; prefix[1]='9'; }
    else {
      fprintf(stderr,"!!! Unusual basename: '%.32s'\n",game->base);
      return -1;
    }
    if (game->platform==str_gameboy) {
      pathc=snprintf(path,sizeof(path),"/home/andy/rom/gameboy/%s/%.32s",prefix,game->base);
    } else if (game->platform==str_pico8) {
      pathc=snprintf(path,sizeof(path),"/home/andy/rom/pico8/%s/%.32s",prefix,game->base);
    } else {
      otherc++;
      continue;
    }
    if ((pathc<1)||(pathc>=sizeof(path))) {
      fprintf(stderr,"!!! error around game %d '%.32s'\n",game->gameid,game->name);
      return -1;
    }
    char prior[1024];
    int priorc=db_game_get_path(prior,sizeof(prior),ra.db,game);
    if ((priorc<1)||(priorc>=sizeof(prior))) {
      fprintf(stderr,"!!! error around game %d '%.32s'\n",game->gameid,game->name);
      return -1;
    }
    if ((priorc==pathc)&&!memcmp(prior,path,pathc)) {
      alreadyc++;
      continue;
    }
    modc++;
    // It's heavy-handed but mkdirp_parent for every file, just to be safe.
    if (dir_mkdirp_parent(path)<0) return -1;
    // Now, IMPORTANTLY, rename the file. Don't copy it. This is important because I expect some unlisted ones will get left behind; I'll want those to stand out after.
    if (rename(prior,path)<0) {
      fprintf(stderr,"%s: rename: %m\n",path);
      return -1;
    }
    if (db_game_set_path(ra.db,game,path,pathc)<0) return -1;
  }
  fprintf(stderr,"Modified %d files, %d already done, %d no need\n",modc,alreadyc,otherc);
  if (modc) {
    if (db_save(ra.db)<0) {
      fprintf(stderr,"!!!!!!!!!! Failed to save database, after modifying %d files. Database now does not match filesystem.\n",modc);
      return -1;
    }
  }
  return 0;
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
    if (XXX_normalize_file_names()<0) {
      fprintf(stderr,"!!!!! FAILURE !!!!!\n");
      return -1;
    }
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
