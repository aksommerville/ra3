#include "mn_internal.h"

struct mn mn={0};

/* Render. Caller sets fences.
 */
 
static void mn_render() {
  //TODO
}

/* Move cursor.
 */
 
static int mn_event_move(int dx,int dy) {
  fprintf(stderr,"%s %+d,%+d\n",__func__,dx,dy);//TODO
  return 0;
}

/* Activate what's highlighted.
 */
 
static int mn_event_activate() {
  fprintf(stderr,"%s\n",__func__);//TODO
  return 0;
}

/* Secondary activation. East and North buttons. Bring up a context menu or something.
 */
 
static int mn_event_secondary() {
  fprintf(stderr,"%s\n",__func__);//TODO
  return 0;
}

/* Cancel topmost thing in progress. West button.
 */
 
static int mn_event_cancel() {
  fprintf(stderr,"%s\n",__func__);//TODO
  return 0;
}

/* Summon or dismiss our modal menu.
 */
 
static int mn_event_pause() {
  fprintf(stderr,"%s\n",__func__);//TODO
  return 0;
}

/* Move selection by one page left or right.
 */
 
static int mn_event_page(int d) {
  fprintf(stderr,"%s %+d\n",__func__,d);//TODO
  return 0;
}

/* Button event.
 */
 
static int mn_event_btn(uint16_t btnid,int v) {
  if (v) switch (btnid) {
    case EH_BTN_LEFT: return mn_event_move(-1,0);
    case EH_BTN_RIGHT: return mn_event_move(1,0);
    case EH_BTN_UP: return mn_event_move(0,-1);
    case EH_BTN_DOWN: return mn_event_move(0,1);
    case EH_BTN_SOUTH: return mn_event_activate();
    case EH_BTN_WEST: return mn_event_cancel();
    case EH_BTN_EAST: return mn_event_secondary();
    case EH_BTN_NORTH: return mn_event_secondary();
    case EH_BTN_AUX1: return mn_event_pause();
    case EH_BTN_AUX2: return mn_event_pause();
    case EH_BTN_AUX3: return mn_event_pause();
    case EH_BTN_L1: return mn_event_page(-1);
    case EH_BTN_L2: return mn_event_page(-1);
    case EH_BTN_R1: return mn_event_page(1);
    case EH_BTN_R2: return mn_event_page(1);
  }
  return 0;
}

/* Update.
 */
 
static int mn_update() {
  int err;

  uint16_t input=eh_input_get(0);
  if (input!=mn.pvinput) {
    uint16_t btnid=0x0001;
    for (;btnid;btnid<<=1) {
      if (input&btnid) {
        if (!(mn.pvinput&btnid)) {
          if ((err=mn_event_btn(btnid,1))<0) return err;
        }
      } else {
        if (mn.pvinput&btnid) {
          if ((err=mn_event_btn(btnid,0))<0) return err;
        }
      }
    }
    mn.pvinput=input;
  }
          
  //TODO process input

  if (mn.video_dirty) {
    mn.video_dirty=0;
    int winw=0,winh=0;
    eh_video_get_size(&winw,&winh);
    eh_video_begin();
    mn_render();
    eh_video_end();
  }
  return 0;
}

/* Generate PCM.
 */
 
static void mn_generate_pcm(void *v,int c) {
  int rate=eh_audio_get_rate();
  int chanc=eh_audio_get_chanc();
  memset(v,0,c<<1);//TODO
}

/* Main.
 */
 
int main(int argc,char **argv) {
  struct eh_delegate delegate={
    .video_width=0, // 0,0 to request an OpenGL context at variable resolution.
    .video_height=0,
    .video_format=0,
    .video_rate=0, // 0=don't care
    .audio_rate=0, // 0=don't care
    .audio_chanc=1,
    .audio_format=EH_AUDIO_FORMAT_S16N,
    .playerc=1,
    .configure=0, // no need
    .load_file=0, // no need
    .load_serial=0, // no need
    .update=mn_update,
    .generate_pcm=mn_generate_pcm,
    .reset=0, // no need
  };
  mn.video_dirty=1;
  return eh_main(argc,argv,&delegate);
}
