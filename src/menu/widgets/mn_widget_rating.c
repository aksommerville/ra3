#include "../mn_internal.h"
#include "opt/gui/gui_font.h"

/* Object definition.
 */
 
struct mn_widget_rating {
  struct gui_widget hdr;
  int focus; // boolean (we have no children, it's all right here)
  int v;
  void (*cb)(struct gui_widget *widget,int v,void *userdata);
  void *userdata;
  int vdrawn; // what values we've current got in the textures
  struct gui_font *font; // WEAK
  struct gui_texture *tex;
  int margin;
};

#define WIDGET ((struct mn_widget_rating*)widget)

/* Cleanup.
 */
 
static void _rating_del(struct gui_widget *widget) {
  gui_texture_del(WIDGET->tex);
}

/* Init.
 */
 
static int _rating_init(struct gui_widget *widget) {
  WIDGET->margin=10;
  WIDGET->vdrawn=-1;
  if (!(WIDGET->font=gui_font_get(widget->gui,"giantmono",9))) return -1;
  return 0;
}

/* Measure.
 */
 
static void _rating_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  *w=(WIDGET->margin<<1)+(WIDGET->font->charw<<1);
  *h=(WIDGET->margin<<1)+WIDGET->font->lineh;
}

/* Pack.
 */
 
static void _rating_pack(struct gui_widget *widget) {
}

/* Draw.
 */
 
static void _rating_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x202020ff);
  if (WIDGET->tex) {
    gui_draw_texture(widget->gui,x+WIDGET->margin,y+WIDGET->margin,WIDGET->tex);
  }
}

/* Redraw labels.
 */
 
static void rating_redraw_labels(struct gui_widget *widget) {
  if (WIDGET->v!=WIDGET->vdrawn) {
    gui_texture_del(WIDGET->tex);
    char v[2]={'0'+WIDGET->v/10,'0'+WIDGET->v%10};
    WIDGET->tex=gui_texture_from_text(widget->gui,WIDGET->font,v,2,0xffffff);
    WIDGET->vdrawn=WIDGET->v;
  }
}

/* Motion.
 */
 
static void _rating_motion(struct gui_widget *widget,int dx,int dy) {
  if (dy) WIDGET->v-=dy*10; // '-' not '+': UP means MORE
  else WIDGET->v+=dx;
  if (WIDGET->v<0) WIDGET->v=0;
  else if (WIDGET->v>99) WIDGET->v=99;
  MN_SOUND(MOTION)
  rating_redraw_labels(widget);
}

/* Submit.
 */
 
static void rating_submit(struct gui_widget *widget) {
  if (WIDGET->cb) WIDGET->cb(widget,WIDGET->v,WIDGET->userdata);
}

/* Signal.
 */
 
static void _rating_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_CANCEL: MN_SOUND(CANCEL) gui_dismiss_modal(widget->gui,widget); break;
    case GUI_SIGID_ACTIVATE: rating_submit(widget); break;
    case GUI_SIGID_FOCUS: WIDGET->focus=1; break;
    case GUI_SIGID_BLUR: WIDGET->focus=0; break;
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_rating={
  .name="rating",
  .objlen=sizeof(struct mn_widget_rating),
  .del=_rating_del,
  .init=_rating_init,
  .measure=_rating_measure,
  .pack=_rating_pack,
  .draw=_rating_draw,
  .motion=_rating_motion,
  .signal=_rating_signal,
};

/* Public setup.
 */
 
int mn_widget_rating_setup(
  struct gui_widget *widget,
  void (*cb)(struct gui_widget *rating,int v,void *userdata),
  void *userdata,
  int v
) {
  if (!widget||(widget->type!=&mn_widget_type_rating)) return -1;
  WIDGET->cb=cb;
  WIDGET->userdata=userdata;
  WIDGET->v=v;
  rating_redraw_labels(widget);
  return 0;
}
