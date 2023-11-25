#include "../mn_internal.h"

/* Object definition.
 */
 
struct mn_widget_audio {
  struct gui_widget hdr;
};

#define WIDGET ((struct mn_widget_audio*)widget)

/* Cleanup.
 */
 
static void _audio_del(struct gui_widget *widget) {
}

/* Init.
 */
 
static int _audio_init(struct gui_widget *widget) {
  return 0;
}

/* Measure.
 */
 
static void _audio_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  *w=*h=200;//TODO
}

/* Pack.
 */
 
static void _audio_pack(struct gui_widget *widget) {
}

/* Draw.
 */
 
static void _audio_draw(struct gui_widget *widget,int x,int y) {
}

/* Motion.
 */
 
static void _audio_motion(struct gui_widget *widget,int dx,int dy) {
}

/* Activate.
 */
 
static void audio_activate(struct gui_widget *widget) {
}

/* Signal.
 */
 
static void _audio_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_ACTIVATE: audio_activate(widget); break;
    case GUI_SIGID_CANCEL: gui_dismiss_modal(widget->gui,widget); break;
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_audio={
  .name="audio",
  .objlen=sizeof(struct mn_widget_audio),
  .del=_audio_del,
  .init=_audio_init,
  .measure=_audio_measure,
  .pack=_audio_pack,
  .draw=_audio_draw,
  .motion=_audio_motion,
  .signal=_audio_signal,
};
