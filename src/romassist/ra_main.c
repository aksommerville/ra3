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

  if (!status) db_save(ra.db);
  db_del(ra.db);
  http_context_del(ra.http);
  ra_process_cleanup(&ra.process);

  if (status) fprintf(stderr,"%s: Abnormal exit.\n",ra.exename);
  else fprintf(stderr,"%s: Normal exit.\n",ra.exename);
  return status;
}
