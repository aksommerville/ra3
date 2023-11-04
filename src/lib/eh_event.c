#include "eh_internal.h"

/* Window closed.
 */
 
void eh_cb_close(void *dummy) {
  eh.terminate=1;
}

/* Window resized.
 */
 
void eh_cb_resize(int w,int h,void *dummy) {
  eh_render_bounds_dirty(eh.render);
}

/* Window gained or lost focus.
 */
 
void eh_cb_focus(int focus,void *dummy) {
  // I don't expect to use this.
}

/* Raw keyboard event from window manager.
 */
 
int eh_cb_key(int keycode,int value,void *dummy) {
  //fprintf(stderr,"%s 0x%08x=%d\n",__func__,keycode,value);
  eh_inmgr_button(eh.inmgr,eh.devid_keyboard,keycode,value);
  return 0;
}

/* Text event from window manager.
 */
 
void eh_cb_text(int codepoint,void *dummy) {
  // I don't think we're going to use this, but not sure.
  // Drivers are equipped to provide it, but our public API doesn't mention it yet.
  // Emulators would have no use for it, but the menu might.
}

/* Audio buffer ready to fill.
 */
 
void eh_cb_pcm(int16_t *v,int c,void *dummy) {
  memset(v,0,c<<1);//TODO
}

/* Input device connected.
 */
 
void eh_cb_connect(int devid,void *dummy) {
  struct eh_input_driver *driver=0;
  int i=eh.inputc;
  while (i-->0) {
    if (eh.inputv[i]->type->has_device(eh.inputv[i],devid)) {
      driver=eh.inputv[i];
      break;
    }
  }
  eh_inmgr_connect(eh.inmgr,driver,devid);
}

/* Input device disconnected.
 */
 
void eh_cb_disconnect(int devid,void *dummy) {
  eh_inmgr_disconnect(eh.inmgr,devid);
}

/* Input device state changed.
 */
 
void eh_cb_button(int devid,int btnid,int value,void *dummy) {
  //fprintf(stderr,"%s %d.0x%08x=%d\n",__func__,devid,btnid,value);
  eh_inmgr_button(eh.inmgr,devid,btnid,value);
}

/* Digested input event from inmgr.
 */
 
int eh_cb_digested_event(void *userdata,const struct eh_inmgr_event *event) {
  //fprintf(stderr,"%s %d.0x%08x=%d [0x%04x] (%d.%d=%d)\n",__func__,event->playerid,event->btnid,event->value,event->state,event->srcdevid,event->srcbtnid,event->srcvalue);
  // Normal input processing is not event-driven for us. Client polls inmgr via eh_input_get().
  // We do catch all the stateless actions right here.
  if (event->value&&(event->btnid&0xffff0000)) switch (event->btnid) {
    case EH_BTN_QUIT: eh.terminate=1; break;
    case EH_BTN_SCREENCAP: fprintf(stderr,"TODO screencap\n"); break;
    case EH_BTN_FULLSCREEN: {
        if (eh.video->type->set_fullscreen) eh.video->type->set_fullscreen(eh.video,eh.video->fullscreen?0:1);
      } break;
    case EH_BTN_PAUSE: fprintf(stderr,"TODO pause\n"); break;
    case EH_BTN_DEBUG: fprintf(stderr,"TODO debug\n"); break;
    case EH_BTN_STEP: fprintf(stderr,"TODO step\n"); break;
    case EH_BTN_FASTFWD: fprintf(stderr,"TODO fastfwd\n"); break;
    case EH_BTN_SAVESTATE: fprintf(stderr,"TODO savestate\n"); break;
    case EH_BTN_LOADSTATE: fprintf(stderr,"TODO loadstate\n"); break;
  }
  return 0;
}

/* Signal from inmgr that the configuration is dirty.
 */
 
void eh_cb_inmgr_config_dirty(void *userdata) {
  fprintf(stderr,"%s\n",__func__);
}
