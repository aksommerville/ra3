#include "../mn_internal.h"

/* Object definition.
 */
 
struct mn_widget_network {
  struct gui_widget hdr;
};

#define WIDGET ((struct mn_widget_network*)widget)

/* Cleanup.
 */
 
static void _network_del(struct gui_widget *widget) {
}

/* Init.
 */
 
static int _network_init(struct gui_widget *widget) {
  return 0;
}

/* Measure.
 */
 
static void _network_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  *w=*h=200;//TODO
}

/* Pack.
 */
 
static void _network_pack(struct gui_widget *widget) {
}

/* Draw.
 */
 
static void _network_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0xffff00ff);
}

/* Motion.
 */
 
static void _network_motion(struct gui_widget *widget,int dx,int dy) {
}

/* Activate.
 */
 
static void network_activate(struct gui_widget *widget) {
}

/* Signal.
 */
 
static void _network_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_ACTIVATE: network_activate(widget); break;
    case GUI_SIGID_CANCEL: gui_dismiss_modal(widget->gui,widget); break;
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_network={
  .name="network",
  .objlen=sizeof(struct mn_widget_network),
  .del=_network_del,
  .init=_network_init,
  .measure=_network_measure,
  .pack=_network_pack,
  .draw=_network_draw,
  .motion=_network_motion,
  .signal=_network_signal,
};
