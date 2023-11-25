#include "../mn_internal.h"

/* Object definition.
 */
 
struct mn_widget_interface {
  struct gui_widget hdr;
};

#define WIDGET ((struct mn_widget_interface*)widget)

/* Cleanup.
 */
 
static void _interface_del(struct gui_widget *widget) {
}

/* Init.
 */
 
static int _interface_init(struct gui_widget *widget) {
  return 0;
}

/* Measure.
 */
 
static void _interface_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  *w=*h=200;//TODO
}

/* Pack.
 */
 
static void _interface_pack(struct gui_widget *widget) {
}

/* Draw.
 */
 
static void _interface_draw(struct gui_widget *widget,int x,int y) {
}

/* Motion.
 */
 
static void _interface_motion(struct gui_widget *widget,int dx,int dy) {
}

/* Activate.
 */
 
static void interface_activate(struct gui_widget *widget) {
}

/* Signal.
 */
 
static void _interface_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_ACTIVATE: interface_activate(widget); break;
    case GUI_SIGID_CANCEL: gui_dismiss_modal(widget->gui,widget); break;
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_interface={
  .name="interface",
  .objlen=sizeof(struct mn_widget_interface),
  .del=_interface_del,
  .init=_interface_init,
  .measure=_interface_measure,
  .pack=_interface_pack,
  .draw=_interface_draw,
  .motion=_interface_motion,
  .signal=_interface_signal,
};
