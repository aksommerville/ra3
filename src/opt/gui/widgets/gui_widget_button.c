#include "opt/gui/gui_internal.h"
#include <math.h>

#define BUTTON_HIGHLIGHT_SPEED 0.100f /* radians/frame */

/* Object definition.
 */
 
struct gui_widget_button {
  struct gui_widget hdr;
  int focus;
  float highlight_time; // -pi..pi, start at zero
  void (*cb)(struct gui_widget *button,void *userdata);
  void *userdata;
  struct gui_texture *texture;
  int enable;
  char *text; // not operationally necessary, but can be very convenient for our owner
  int textc;
};

#define WIDGET ((struct gui_widget_button*)widget)

/* Cleanup.
 */
 
static void _button_del(struct gui_widget *widget) {
  gui_texture_del(WIDGET->texture);
  if (WIDGET->text) free(WIDGET->text);
}

/* Init.
 */
 
static int _button_init(struct gui_widget *widget) {
  WIDGET->enable=1;
  return 0;
}

/* Measure.
 */
 
static void _button_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  if (WIDGET->texture) {
    gui_texture_get_size(w,h,WIDGET->texture);
  } else {
    *w=10;
    struct gui_font *font=gui_font_get_default(widget->gui);
    if (font) *h=font->lineh;
    else *h=10;
  }
}

/* Pack.
 */
 
static void _button_pack(struct gui_widget *widget) {
}

/* Draw.
 */
 
static void _button_draw(struct gui_widget *widget,int x,int y) {
  if (WIDGET->texture) {
    gui_draw_texture(widget->gui,x,y,WIDGET->texture);
  }
  if (WIDGET->focus&&WIDGET->enable) {
    WIDGET->highlight_time+=BUTTON_HIGHLIGHT_SPEED;
    if (WIDGET->highlight_time>=M_PI) WIDGET->highlight_time-=M_PI*2.0f;
    uint32_t rgba=0xffff0000;
    rgba|=(int)((cosf(WIDGET->highlight_time)+1.0f)*128.0f);
    gui_frame_rect(widget->gui,x,y,widget->w,widget->h,rgba);
  }
}

/* Activate.
 */
 
static void button_activate(struct gui_widget *widget) {
  if (!WIDGET->enable) return;
  if (WIDGET->cb) {
    WIDGET->cb(widget,WIDGET->userdata);
  }
}

/* Gain or lose focus.
 */
 
static void button_focus(struct gui_widget *widget) {
  WIDGET->focus=1;
  WIDGET->highlight_time=0.0f;
}

static void button_blur(struct gui_widget *widget) {
  WIDGET->focus=0;
}

/* Signal.
 */
 
static void _button_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_ACTIVATE: button_activate(widget); break;
    case GUI_SIGID_FOCUS: button_focus(widget); break;
    case GUI_SIGID_BLUR: button_blur(widget); break;
  }
}

/* Type definition.
 */
 
const struct gui_widget_type gui_widget_type_button={
  .name="button",
  .objlen=sizeof(struct gui_widget_button),
  .del=_button_del,
  .init=_button_init,
  .measure=_button_measure,
  .pack=_button_pack,
  .draw=_button_draw,
  .signal=_button_signal,
};

/* Set label.
 */
 
int gui_widget_button_set_label(struct gui_widget *widget,const char *src,int srcc,int rgb) {
  if (!widget||(widget->type!=&gui_widget_type_button)) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  struct gui_texture *ntexture=0;
  if (srcc) {
    if (!(ntexture=gui_texture_from_text(widget->gui,0,src,srcc,rgb))) return -1;
  }
  gui_texture_del(WIDGET->texture);
  WIDGET->texture=ntexture;
  
  char *ntext=0;
  if (srcc) {
    if (!(ntext=malloc(srcc+1))) return -1;
    memcpy(ntext,src,srcc);
    ntext[srcc]=0;
  }
  if (WIDGET->text) free(WIDGET->text);
  WIDGET->text=ntext;
  WIDGET->textc=srcc;
  
  return 0;
}

/* Spawn button.
 */
 
struct gui_widget *gui_widget_button_spawn(
  struct gui_widget *parent,
  const char *label,int labelc,
  int rgb,
  void (*cb)(struct gui_widget *button,void *userdata),
  void *userdata
) {
  struct gui_widget *widget=gui_widget_spawn(parent,&gui_widget_type_button);
  if (!widget) return 0;
  if (gui_widget_button_set_label(widget,label,labelc,rgb)<0) {
    gui_widget_remove_child(parent,widget);
    return 0;
  }
  WIDGET->cb=cb;
  WIDGET->userdata=userdata;
  return widget;
}

/* Trivial public accessors.
 */
 
void gui_widget_button_enable(struct gui_widget *widget,int enable) {
  if (!widget||(widget->type!=&gui_widget_type_button)) return;
  WIDGET->enable=enable?1:0;
}

int gui_widget_button_get_enable(const struct gui_widget *widget) {
  if (!widget||(widget->type!=&gui_widget_type_button)) return 0;
  return WIDGET->enable;
}

int gui_widget_button_get_text(void *dstpp,const struct gui_widget *widget) {
  if (!widget||(widget->type!=&gui_widget_type_button)) return 0;
  if (dstpp) *(void**)dstpp=WIDGET->text;
  return WIDGET->textc;
}
