#include "../mn_internal.h"
#include <math.h>

/* Object definition.
 */
 
struct mn_widget_home {
  struct gui_widget hdr;
  double clockt;
};

#define WIDGET ((struct mn_widget_home*)widget)

/* Delete.
 */
 
static void _home_del(struct gui_widget *widget) {
}

/* Init.
 */
 
static int _home_init(struct gui_widget *widget) {
  widget->opaque=1;
  return 0;
}

/* Pack.
 */
 
static void _home_pack(struct gui_widget *widget) {
}

/* Draw.
 */
 
static void _home_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x0080ffff);
  int midx=x+(widget->w>>1);
  int midy=y+(widget->h>>1);
  WIDGET->clockt+=0.05;
  if (WIDGET->clockt>M_PI) WIDGET->clockt-=M_PI*2.0;
  double radius=100.0;
  int farx=midx+(int)(cos(WIDGET->clockt)*radius);
  int fary=midy-(int)(sin(WIDGET->clockt)*radius);
  gui_draw_line(widget->gui,midx,midy,farx,fary,0xffffffff);
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_home={
  .name="home",
  .objlen=sizeof(struct mn_widget_home),
  .del=_home_del,
  .init=_home_init,
  .pack=_home_pack,
  .draw=_home_draw,
};
