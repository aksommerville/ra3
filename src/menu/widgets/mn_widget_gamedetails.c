/* mn_widget_gamedetails.
 * Top row of home page.
 */
 
#include "../mn_internal.h"

/* Object definition.
 */
 
struct mn_widget_gamedetails {
  struct gui_widget hdr;
};

#define WIDGET ((struct mn_widget_gamedetails*)widget)

/* Delete.
 */
 
static void _gamedetails_del(struct gui_widget *widget) {
}

/* Init.
 */
 
static int _gamedetails_init(struct gui_widget *widget) {
  return 0;
}

/* Measure.
 */
 
static void _gamedetails_measure(int *w,int *h,struct gui_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
}

/* Pack.
 */
 
static void _gamedetails_pack(struct gui_widget *widget) {
}

/* Draw.
 */
 
static void _gamedetails_draw(struct gui_widget *widget,int x,int y) {
  //gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x0000ffff);
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_gamedetails={
  .name="gamedetails",
  .objlen=sizeof(struct mn_widget_gamedetails),
  .del=_gamedetails_del,
  .init=_gamedetails_init,
  .measure=_gamedetails_measure,
  .pack=_gamedetails_pack,
  .draw=_gamedetails_draw,
};
