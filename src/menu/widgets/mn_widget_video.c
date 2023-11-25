#include "../mn_internal.h"

/* Object definition.
 */
 
struct mn_widget_video {
  struct gui_widget hdr;
};

#define WIDGET ((struct mn_widget_video*)widget)

/* Cleanup.
 */
 
static void _video_del(struct gui_widget *widget) {
}

/* Init.
 */
 
static int _video_init(struct gui_widget *widget) {
  return 0;
}

/* Measure.
 */
 
static void _video_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  *w=*h=200;//TODO
}

/* Pack.
 */
 
static void _video_pack(struct gui_widget *widget) {
}

/* Draw.
 */
 
static void _video_draw(struct gui_widget *widget,int x,int y) {
}

/* Motion.
 */
 
static void _video_motion(struct gui_widget *widget,int dx,int dy) {
}

/* Activate.
 */
 
static void video_activate(struct gui_widget *widget) {
}

/* Signal.
 */
 
static void _video_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_ACTIVATE: video_activate(widget); break;
    case GUI_SIGID_CANCEL: gui_dismiss_modal(widget->gui,widget); break;
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_video={
  .name="video",
  .objlen=sizeof(struct mn_widget_video),
  .del=_video_del,
  .init=_video_init,
  .measure=_video_measure,
  .pack=_video_pack,
  .draw=_video_draw,
  .motion=_video_motion,
  .signal=_video_signal,
};
