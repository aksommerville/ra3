#include "../mn_internal.h"

/* Object definition.
 */
 
struct mn_widget_input {
  struct gui_widget hdr;
};

#define WIDGET ((struct mn_widget_input*)widget)

/* Cleanup.
 */
 
static void _input_del(struct gui_widget *widget) {
}

/* Init.
 */
 
static int _input_init(struct gui_widget *widget) {
  return 0;
}

/* Measure.
 */
 
static void _input_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  *w=*h=200;//TODO
}

/* Pack.
 */
 
static void _input_pack(struct gui_widget *widget) {
}

/* Draw.
 */
 
static void _input_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x0000ffff);
}

/* Motion.
 */
 
static void _input_motion(struct gui_widget *widget,int dx,int dy) {
}

/* Activate.
 */
 
static void input_activate(struct gui_widget *widget) {
}

/* Signal.
 */
 
static void _input_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_ACTIVATE: input_activate(widget); break;
    case GUI_SIGID_CANCEL: gui_dismiss_modal(widget->gui,widget); break;
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_input={
  .name="input",
  .objlen=sizeof(struct mn_widget_input),
  .del=_input_del,
  .init=_input_init,
  .measure=_input_measure,
  .pack=_input_pack,
  .draw=_input_draw,
  .motion=_input_motion,
  .signal=_input_signal,
};
