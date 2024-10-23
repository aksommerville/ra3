#include "eh_internal.h"
#include "opt/serial/serial.h"

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
  if (eh.delegate.generate_pcm) {
    eh.delegate.generate_pcm(v,c);
  } else {
    eh_aucvt_output(v,c,&eh.aucvt);
  }
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

/* Toggle fullscreen.
 * This is bigger than it sounds: The setting should stick across launches.
 */
 
static void eh_toggle_fullscreen() {
  if (!eh.video->type->set_fullscreen) return;
  eh.video->type->set_fullscreen(eh.video,eh.video->fullscreen?0:1);
  eh.fullscreen=eh.video->fullscreen?1:0;
  eh_config_save();
}

/* Digested input event from inmgr.
 */
 
static void eh_trigger_action(int action) {
  switch (action) {
    case EH_BTN_QUIT: if (eh.allow_quit_button) eh.terminate=1; break;
    case EH_BTN_SCREENCAP: eh.screencap_requested=1; break;
    case EH_BTN_FULLSCREEN: eh_toggle_fullscreen(); break;
    case EH_BTN_PAUSE: {
        if (eh.hard_pause) {
          eh.hard_pause=0;
          eh.hard_pause_stepc=0;
        } else {
          eh.hard_pause=1;
        }
      } break;
    case EH_BTN_DEBUG: fprintf(stderr,"TODO debug\n"); break;
    case EH_BTN_STEP: if (eh.hard_pause) eh.hard_pause_stepc++; break;
    case EH_BTN_FASTFWD: eh.fastfwd=eh.fastfwd?0:1; fprintf(stderr,"fastfwd=%d\n",eh.fastfwd); break; //disabled for now to protect our CPUs
    case EH_BTN_SAVESTATE: fprintf(stderr,"TODO savestate\n"); break;
    case EH_BTN_LOADSTATE: fprintf(stderr,"TODO loadstate\n"); break;
  }
}
 
int eh_cb_digested_event(void *userdata,const struct eh_inmgr_event *event) {
  //fprintf(stderr,"%s %d.0x%08x=%d [0x%04x] (%d.%d=%d)\n",__func__,event->playerid,event->btnid,event->value,event->state,event->srcdevid,event->srcbtnid,event->srcvalue);
  // Normal input processing is not event-driven for us. Client polls inmgr via eh_input_get().
  // We do catch all the stateless actions right here.
  if (event->value&&(event->btnid&0xffff0000)) {
    eh_trigger_action(event->btnid);
    return 0;
  }
  return 0;
}

/* Signal from inmgr that the configuration is dirty.
 */
 
void eh_cb_inmgr_config_dirty(void *userdata) {
  eh.inmgr_dirty=1;
}

/* Persistent WebSocket connection.
 */
 
void eh_cb_ws_connect(void *userdata) {
}

void eh_cb_ws_disconnect(void *userdata) {
}

void eh_cb_ws_message(int opcode,const void *v,int c,void *userdata) {
  //fprintf(stderr,"%s opcode=%d c=%d\n",__func__,opcode,c);
  //if (opcode==1) fprintf(stderr,"v: %.*s\n",c,(char*)v);
  // We don't respond to any binary packets.
  if (opcode!=1) return;
  // And so nice, every packet we do respond to, only contains "id".
  struct sr_decoder decoder={.v=v,.c=c};
  if (sr_decode_json_object_start(&decoder)>=0) {
    const char *k;
    int kc;
    while ((kc=sr_decode_json_next(&k,&decoder))>0) {
      if ((kc==2)&&!memcmp(k,"id",2)) {
        char id[64];
        int idc=sr_decode_json_string(id,sizeof(id),&decoder);
        if ((idc>0)&&(idc<=sizeof(id))) {
        
          if ((idc==16)&&!memcmp(id,"requestScreencap",16)) { eh_trigger_action(EH_BTN_SCREENCAP); return; }
          if ((idc==5)&&!memcmp(id,"pause",5)) { eh_trigger_action(EH_BTN_PAUSE); return; }
          if ((idc==6)&&!memcmp(id,"resume",6)) { eh_trigger_action(EH_BTN_PAUSE); return; } // our PAUSE is a toggle. oh well, close enough
          if ((idc==4)&&!memcmp(id,"step",4)) { eh_trigger_action(EH_BTN_STEP); return; }
          if ((idc==12)&&!memcmp(id,"httpresponse",12)) { if (eh.delegate.http_response) eh.delegate.http_response(v,c); return; }
        
          if (eh.delegate.websocket_incoming) eh.delegate.websocket_incoming(id,idc,v,c);
          return;
        }
        break;
      } else {
        if (sr_decode_json_skip(&decoder)<0) break;
      }
    }
  }
}
