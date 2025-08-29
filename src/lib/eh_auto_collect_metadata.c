#include "eh_internal.h"

/* Update.
 */
 
int eh_auto_collect_metadata_update(struct eh_auto_collect_metadata *acm) {
  switch ((acm->stage++)&3) {
    case 0: {
        inmgr_artificial_event(1,INMGR_BTN_SOUTH,1);
      } break;
    case 1: {
        inmgr_artificial_event(1,INMGR_BTN_SOUTH,0);
      } break;
    case 2: {
        inmgr_artificial_event(1,INMGR_BTN_AUX1,1);
      } break;
    case 3: {
        inmgr_artificial_event(1,INMGR_BTN_AUX1,0);
      } break;
  }
  return 0;
}

/* Analyze framebuffer.
 */
 
int eh_auto_collect_metadata_fb(const void *fb) {
  /*
    .w=eh.delegate.video_width,
    .h=eh.delegate.video_height,
    .format=eh.delegate.video_format,
    .rmask=eh.delegate.rmask,
    .gmask=eh.delegate.gmask,
    .bmask=eh.delegate.bmask,
    .ctab=eh.render->ctab,
  */
  //...ugh actually this is crazy, it's too much effort.
  return 0;
}
