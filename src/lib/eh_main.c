#include "eh_internal.h"
#include <signal.h>
#include <unistd.h>

struct eh eh={0};

/* Signal handler.
 */
 
static void eh_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(eh.sigc)>=3) {
        fprintf(stderr,"%s: Too many unprocessed signals.\n",eh.exename);
        exit(1);
      } break;
  }
}

/* Cleanup.
 * Called only on normal exits.
 * Opportunity for performance logging, etc.
 */
 
static void eh_cleanup() {
  fprintf(stderr,"%s: Normal exit.\n",eh.exename);
  eh_drivers_quit();
}

/* Update.
 * Poll our drivers, delay, update client, everything.
 */
 
static int eh_update() {
  usleep(100000);//TODO
  return 0;
}

/* Main.
 */
 
int eh_main(int argc,char **argv,const struct eh_delegate *delegate) {
  int err;
  eh.delegate=*delegate;
  
  signal(SIGINT,eh_rcvsig);
  
  if ((err=eh_configure(argc,argv))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error from eh_configure.\n",eh.exename);
    return 1;
  }
  if (eh.terminate) {
    // eh_configure() may set (terminate), eg when it processed --help
    return 0;
  }
  
  if ((err=eh_drivers_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error from eh_drivers_init.\n",eh.exename);
    return 1;
  }
  
  while (1) {
    if (eh.sigc) break;
    if (eh.terminate) break;
    if ((err=eh_update())<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error from eh_update.\n",eh.exename);
      return 1;
    }
  }
  
  eh_cleanup();
  return 0;
}
