/* gui_widget_entry.c
 * General text-entry widget, intended for popup single-line editors.
 * We create a keyboard, and it is technically a child of this widget, though it will appear to be anchored to the root.
 */

#include "../gui_internal.h"
#include "opt/serial/serial.h"

static void entry_cb_codepoint(struct gui_widget *keyboard,int codepoint,void *userdata);

/* Object definition.
 */
 
struct gui_widget_entry {
  struct gui_widget hdr;
  struct gui_font *font; // WEAK
  struct gui_texture *tex;
  void (*cb)(struct gui_widget *widget,const char *v,int c,void *userdata);
  void *userdata;
  struct sr_encoder v;
};

#define WIDGET ((struct gui_widget_entry*)widget)

/* Cleanup.
 */
 
static void _entry_del(struct gui_widget *widget) {
  gui_texture_del(WIDGET->tex);
  sr_encoder_cleanup(&WIDGET->v);
}

/* Init.
 */
 
static int _entry_init(struct gui_widget *widget) {
  struct gui_widget *keyboard=gui_widget_spawn(widget,&gui_widget_type_keyboard);
  if (!keyboard) return -1;
  gui_widget_keyboard_set_callback(keyboard,entry_cb_codepoint,widget);
  if (!(WIDGET->font=gui_font_get(widget->gui,"lightv40",8))) return -1;
  return 0;
}

/* Measure.
 */
 
static void _entry_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  *h=WIDGET->font->lineh;
  int ww=gui_font_measure_line(WIDGET->font,"W",1);
  *w=ww*20;
  if (widget->childc>=1) {
    struct gui_widget *keyboard=widget->childv[0];
    int chw,chh;
    gui_widget_measure(&chw,&chh,keyboard,wlimit,hlimit);
    if (chw>*w) *w=chw;
    (*h)+=chh;
  }
}

/* Pack.
 */
 
static void _entry_pack(struct gui_widget *widget) {
  if (widget->childc!=1) return;
  struct gui_widget *keyboard=widget->childv[0];
  // Keyboard gets the lower portion, whatever we don't need for the text view.
  keyboard->x=0;
  keyboard->y=WIDGET->font->lineh;
  keyboard->w=widget->w;
  keyboard->h=widget->h-keyboard->y;
  gui_widget_pack(keyboard);
}

/* Draw.
 */
 
static void _entry_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x202020ff);//TODO
  int i=0;
  for (;i<widget->childc;i++) {
    struct gui_widget *child=widget->childv[i];
    gui_widget_draw(child,x+child->x,y+child->y);
  }
  if (WIDGET->tex) {
    //TODO horizontal scrolling and scissor
    gui_draw_texture(widget->gui,x,y,WIDGET->tex);
  }
}

/* Redraw texture.
 */
 
static int entry_redraw_tex(struct gui_widget *widget) {
  gui_texture_del(WIDGET->tex);
  if (!(WIDGET->tex=gui_texture_from_text(widget->gui,WIDGET->font,WIDGET->v.v,WIDGET->v.c,0xffffff))) return -1;
  return 0;
}

/* Append character.
 */
 
static void entry_append(struct gui_widget *widget,int codepoint) {
  sr_encode_utf8(&WIDGET->v,codepoint);
  entry_redraw_tex(widget);
}

/* Backspace.
 */
 
static void entry_backspace(struct gui_widget *widget) {
  if (WIDGET->v.c<1) return;
  WIDGET->v.c--;
  while (WIDGET->v.c&&(WIDGET->v.v[WIDGET->v.c]&0x80)) WIDGET->v.c--;
  entry_redraw_tex(widget);
}

/* Submit.
 */
 
static void entry_submit(struct gui_widget *widget) {
  if (!WIDGET->cb) return;
  WIDGET->cb(widget,WIDGET->v.v,WIDGET->v.c,WIDGET->userdata);
}

/* Events.
 */
 
static void _entry_motion(struct gui_widget *widget,int dx,int dy) {
  if (widget->childc>=1) {
    struct gui_widget *child=widget->childv[0];
    if (child->type->motion) child->type->motion(child,dx,dy);
  }
}
 
static void _entry_signal(struct gui_widget *widget,int sigid) {
  if (widget->childc>=1) {
    struct gui_widget *child=widget->childv[0];
    if (child->type->signal) child->type->signal(child,sigid);
  }
}

static void entry_cb_codepoint(struct gui_widget *keyboard,int codepoint,void *userdata) {
  struct gui_widget *widget=userdata;
  switch (codepoint) {
    case 0x00: gui_dismiss_modal(widget->gui,widget); return;
    case 0x08: entry_backspace(widget); break;
    case 0x0a: entry_submit(widget); break;
    default: entry_append(widget,codepoint); break;
  }
}

/* Type definition.
 */
 
const struct gui_widget_type gui_widget_type_entry={
  .name="entry",
  .objlen=sizeof(struct gui_widget_entry),
  .del=_entry_del,
  .init=_entry_init,
  .measure=_entry_measure,
  .pack=_entry_pack,
  .draw=_entry_draw,
  .motion=_entry_motion,
  .signal=_entry_signal,
};

/* Public accessors.
 */
 
int gui_widget_entry_setup(
  struct gui_widget *widget,
  const char *v,int c,
  void (*cb)(struct gui_widget *widget,const char *v,int c,void *userdata),
  void *userdata
) {
  if (!widget||(widget->type!=&gui_widget_type_entry)) return -1;
  WIDGET->cb=cb;
  WIDGET->userdata=userdata;
  WIDGET->v.c=0;
  if (sr_encode_raw(&WIDGET->v,v,c)<0) return -1;
  if (entry_redraw_tex(widget)<0) return -1;
  return 0;
}
