#include "../mn_internal.h"
#include "opt/gui/gui_font.h"

/* Object definition.
 */
 
struct mn_widget_daterange {
  struct gui_widget hdr;
  int focus; // boolean (we have no children, it's all right here)
  int row; // 0=early, 1=late
  int lovalue,hivalue; // year; chosen values
  int lolimit,hilimit; // year; valid range. outside this, we snap to 0 and 9999
  void (*cb)(struct gui_widget *widget,void *userdata,int lovalue,int hivalue);
  void *userdata;
  int lodrawn,hidrawn; // what values we've current got in the textures
  struct gui_font *font; // WEAK
  struct gui_texture *lotex,*hitex;
  int margin;
  int single; // Nonzero to use the same mechanics but just one row.
};

#define WIDGET ((struct mn_widget_daterange*)widget)

/* Cleanup.
 */
 
static void _daterange_del(struct gui_widget *widget) {
  gui_texture_del(WIDGET->lotex);
  gui_texture_del(WIDGET->hitex);
}

/* Init.
 */
 
static int _daterange_init(struct gui_widget *widget) {
  WIDGET->margin=10;
  WIDGET->lodrawn=-1;
  WIDGET->hidrawn=-1;
  if (!(WIDGET->font=gui_font_get(widget->gui,"giantmono",9))) return -1;
  return 0;
}

/* Measure.
 */
 
static void _daterange_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  *w=(WIDGET->margin<<1)+(WIDGET->font->charw<<2);
  *h=(WIDGET->margin<<1)+(WIDGET->font->lineh*(WIDGET->single?1:2));
}

/* Pack.
 */
 
static void _daterange_pack(struct gui_widget *widget) {
}

/* Draw.
 */
 
static void _daterange_draw(struct gui_widget *widget,int x,int y) {
  const uint32_t hilite=0x204080ff;
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x202020ff);
  int py=y+WIDGET->margin;
  if (WIDGET->lotex) {
    int tw,th;
    gui_texture_get_size(&tw,&th,WIDGET->lotex);
    if (!WIDGET->row) gui_draw_rect(widget->gui,x+WIDGET->margin,py,tw,th,hilite);
    gui_draw_texture(widget->gui,x+WIDGET->margin,py,WIDGET->lotex);
    py+=th;
  }
  if (WIDGET->hitex&&!WIDGET->single) {
    int tw,th;
    gui_texture_get_size(&tw,&th,WIDGET->hitex);
    if (WIDGET->row) gui_draw_rect(widget->gui,x+WIDGET->margin,py,tw,th,hilite);
    gui_draw_texture(widget->gui,x+WIDGET->margin,py,WIDGET->hitex);
  }
}

/* Redraw labels.
 */
 
static void daterange_redraw_labels(struct gui_widget *widget) {
  if (WIDGET->lovalue!=WIDGET->lodrawn) {
    gui_texture_del(WIDGET->lotex);
    char v[4]={'0'+WIDGET->lovalue/1000,'0'+(WIDGET->lovalue/100)%10,'0'+(WIDGET->lovalue/10)%10,'0'+WIDGET->lovalue%10};
    WIDGET->lotex=gui_texture_from_text(widget->gui,WIDGET->font,v,4,0xffffff);
    WIDGET->lodrawn=WIDGET->lovalue;
  }
  if ((WIDGET->hivalue!=WIDGET->hidrawn)&&!WIDGET->single) {
    gui_texture_del(WIDGET->hitex);
    char v[4]={'0'+WIDGET->hivalue/1000,'0'+(WIDGET->hivalue/100)%10,'0'+(WIDGET->hivalue/10)%10,'0'+WIDGET->hivalue%10};
    WIDGET->hitex=gui_texture_from_text(widget->gui,WIDGET->font,v,4,0xffffff);
    WIDGET->hidrawn=WIDGET->hivalue;
  }
}

/* Motion.
 */
 
static void _daterange_motion(struct gui_widget *widget,int dx,int dy) {
  if (dy&&WIDGET->single) {
    dx=dy*10;
    dy=0;
  }
  if (dy) {
    WIDGET->row=WIDGET->row?0:1; // there's only two rows
    MN_SOUND(MOTION)
  } else if (dx) {
    int *v=(WIDGET->row?&WIDGET->hivalue:&WIDGET->lovalue);
    (*v)+=dx;
    if (*v<WIDGET->lolimit) (*v)=(dx<0)?0:WIDGET->lolimit;
    if (*v>WIDGET->hilimit) (*v)=(dx<0)?WIDGET->hilimit:9999;
    if (WIDGET->hivalue<WIDGET->lovalue) {
      if (WIDGET->row) WIDGET->lovalue=WIDGET->hivalue;
      else WIDGET->hivalue=WIDGET->lovalue;
    }
    MN_SOUND(MOTION)
    daterange_redraw_labels(widget);
  }
}

/* Submit.
 */
 
static void daterange_submit(struct gui_widget *widget) {
  if (WIDGET->cb) WIDGET->cb(widget,WIDGET->userdata,WIDGET->lovalue,WIDGET->single?WIDGET->lovalue:WIDGET->hivalue);
}

/* Signal.
 */
 
static void _daterange_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_CANCEL: MN_SOUND(CANCEL) gui_dismiss_modal(widget->gui,widget); break;
    case GUI_SIGID_ACTIVATE: daterange_submit(widget); break;
    case GUI_SIGID_FOCUS: WIDGET->focus=1; break;
    case GUI_SIGID_BLUR: WIDGET->focus=0; break;
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_daterange={
  .name="daterange",
  .objlen=sizeof(struct mn_widget_daterange),
  .del=_daterange_del,
  .init=_daterange_init,
  .measure=_daterange_measure,
  .pack=_daterange_pack,
  .draw=_daterange_draw,
  .motion=_daterange_motion,
  .signal=_daterange_signal,
};

/* Public setup.
 */
 
int mn_widget_daterange_setup(
  struct gui_widget *widget,
  void (*cb)(struct gui_widget *daterange,void *userdata,int lovalue,int hivalue),
  void *userdata,
  int lovalue,int hivalue,
  int lolimit,int hilimit
) {
  if (!widget||(widget->type!=&mn_widget_type_daterange)) return -1;
  WIDGET->cb=cb;
  WIDGET->userdata=userdata;
  WIDGET->lovalue=lovalue;
  WIDGET->hivalue=hivalue;
  WIDGET->lolimit=lolimit;
  WIDGET->hilimit=hilimit;
  WIDGET->single=0;
  daterange_redraw_labels(widget);
  return 0;
}

int mn_widget_daterange_setup_single(
  struct gui_widget *widget,
  void (*cb)(struct gui_widget *daterange,void *userdata,int lovalue,int hivalue),
  void *userdata,
  int value
) {
  if (!widget||(widget->type!=&mn_widget_type_daterange)) return -1;
  WIDGET->cb=cb;
  WIDGET->userdata=userdata;
  WIDGET->lovalue=value;
  WIDGET->hivalue=value;
  WIDGET->lolimit=1970;
  WIDGET->hilimit=9999;
  WIDGET->single=1;
  daterange_redraw_labels(widget);
  return 0;
}
