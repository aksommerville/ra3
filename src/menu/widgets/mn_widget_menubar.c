/* mn_widget_menubar.
 * Top row of home page.
 */
 
#include "../mn_internal.h"

/* Object definition.
 */
 
struct mn_widget_menubar {
  struct gui_widget hdr;
};

#define WIDGET ((struct mn_widget_menubar*)widget)

/* Delete.
 */
 
static void _menubar_del(struct gui_widget *widget) {
}

/* Init.
 */
 
static int _menubar_init(struct gui_widget *widget) {
  return 0;
}

/* Measure.
 */
 
static void _menubar_measure(int *w,int *h,struct gui_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=40;//TODO
}

/* Pack.
 */
 
static void _menubar_pack(struct gui_widget *widget) {
}

/* Draw.
 */
 
static void _menubar_draw(struct gui_widget *widget,int x,int y) {
  //gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0xff0000ff);
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_menubar={
  .name="menubar",
  .objlen=sizeof(struct mn_widget_menubar),
  .del=_menubar_del,
  .init=_menubar_init,
  .measure=_menubar_measure,
  .pack=_menubar_pack,
  .draw=_menubar_draw,
};
