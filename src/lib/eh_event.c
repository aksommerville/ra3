#include "eh_internal.h"

/* Window closed.
 */
 
void eh_cb_close(void *dummy) {
  //fprintf(stderr,"%s\n",__func__);
  eh.terminate=1;
}

/* Window resized.
 */
 
void eh_cb_resize(int w,int h,void *dummy) {
  //fprintf(stderr,"%s %d,%d\n",__func__,w,h);
  eh_render_bounds_dirty(eh.render);
}

/* Window gained or lost focus.
 */
 
void eh_cb_focus(int focus,void *dummy) {
  //fprintf(stderr,"%s %d\n",__func__,focus);
}

/* Raw keyboard event from window manager.
 */
 
int eh_cb_key(int keycode,int value,void *dummy) {
  //fprintf(stderr,"%s 0x%08x=%d\n",__func__,keycode,value);
  return 0;
}

/* Text event from window manager.
 */
 
void eh_cb_text(int codepoint,void *dummy) {
  //fprintf(stderr,"%s U+%x\n",__func__,codepoint);
}

/* Audio buffer ready to fill.
 */
 
void eh_cb_pcm(int16_t *v,int c,void *dummy) {
  memset(v,0,c<<1);//TODO
}

/* Input device connected.
 */
 
void eh_cb_connect(int devid,void *dummy) {
  fprintf(stderr,"%s %d\n",__func__,devid);
}

/* Input device disconnected.
 */
 
void eh_cb_disconnect(int devid,void *dummy) {
  fprintf(stderr,"%s %d\n",__func__,devid);
}

/* Input device state changed.
 */
 
void eh_cb_button(int devid,int btnid,int value,void *dummy) {
  fprintf(stderr,"%s %d.%d=%d\n",__func__,devid,btnid,value);
}
