#include "../gui_internal.h"

/* Object definition.
 */
 
struct gui_widget_label {
  struct gui_widget hdr;
};

#define WIDGET ((struct gui_widget_label*)widget)

/* Delete.
 */
 
static void _label_del(struct gui_widget *widget) {
}

/* Draw.
 */
 
static void _label_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0xffff00ff);
}

/* Type definition.
 */

const struct gui_widget_type gui_widget_type_label={
  .name="label",
  .objlen=sizeof(struct gui_widget_label),
  .del=_label_del,
  .draw=_label_draw,
};
