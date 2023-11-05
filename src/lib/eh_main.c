#include "eh_internal.h"
#include "opt/fs/fs.h"
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
  fprintf(stderr,"%s: Normal exit. %s\n",eh.exename,eh_clock_report(&eh.clock));
  eh_drivers_quit();
}

/* Startup.
 * Configuration is loaded, drivers are running, and we've asserted (eh.rompath).
 */
 
static int eh_startup_vm() {
  if (eh.delegate.load_file) return eh.delegate.load_file(eh.rompath);
  if (eh.delegate.load_serial) {
    void *serial=0;
    int serialc=file_read(&serial,eh.rompath);
    if (serialc<0) {
      fprintf(stderr,"%s: Failed to read file.\n",eh.rompath);
      return -2;
    }
    int err=eh.delegate.load_serial(serial,serialc,eh.rompath);
    free(serial);
    return err;
  }
  fprintf(stderr,"%s: Emuhost delegate does not implement load_file or load_serial.\n",eh.exename);
  return -2;
}

/* Update.
 * Poll our drivers, delay, update client, everything.
 */
 
static int eh_update() {

  // Regulate timing. This may block.
  int framec=eh_clock_tick(&eh.clock);

  // Update all drivers.
  int i=eh.inputc;
  struct eh_input_driver **input=eh.inputv+i-1;
  for (;i-->0;input--) {
    if ((*input)->type->update(*input)<0) {
      fprintf(stderr,"%s: Error polling input (%s).\n",eh.exename,(*input)->type->name);
      return -2;
    }
  }
  if (eh.video->type->update) {
    if (eh.video->type->update(eh.video)<0) {
      fprintf(stderr,"%s: Error updating window manager (%s).\n",eh.exename,eh.video->type->name);
      return -2;
    }
  }
  if (eh.audio->type->update) {
    eh.audio->type->update(eh.audio);
  }
  if (fakews_update(eh.fakews,0)<0) {
    fprintf(stderr,"%s: Error updating network.\n",eh.exename);
    return -2;
  }
  
  // If we're hard-paused, stop here.
  if (eh.hard_pause) {
    if (!eh.hard_pause_stepc) return 0;
    eh.hard_pause_stepc--;
  }
  
  // If we're fast-forwarding, execute 5 frames instead of 1. (framec is always 1 in normal times).
  if (eh.fastfwd) {
    framec=5;
  }
  
  // Update the client.
  eh_render_before(eh.render);
  while (framec-->0) {
    int err=eh.delegate.update(framec);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error updating VM.\n",eh.exename);
      return -2;
    }
  }
  eh_render_after(eh.render);
  
  return 0;
}

/* Main.
 */
 
int eh_main(int argc,char **argv,const struct eh_delegate *delegate) {
  int err;
  eh.delegate=*delegate;
  fprintf(stderr,"%s: Starting up...\n",(argc>=1)?argv[0]:"emuhost");
  
  signal(SIGINT,eh_rcvsig);
  
  if ((err=eh_configure(argc,argv))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error from eh_configure.\n",eh.exename);
    return 1;
  }
  if (eh.terminate) {
    // eh_configure() may set (terminate), eg when it processed --help
    return 0;
  }
  if (!eh.rompath) {
    fprintf(stderr,"%s: Expected ROM path.\n",eh.exename);
    return 1;
  }
  
  if ((err=eh_drivers_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error from eh_drivers_init.\n",eh.exename);
    return 1;
  }
  
  if ((err=eh_startup_vm())<0) {
    if (err!=2) fprintf(stderr,"%s: Unspecified error starting vm.\n",eh.exename);
    return 1;
  }
  
  eh.audio->type->play(eh.audio,1);
  
  eh_clock_init(&eh.clock,eh.delegate.video_rate);
  
  fprintf(stderr,"%s: Running...\n",eh.exename);
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
