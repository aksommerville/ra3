#include "../gui_internal.h"
#include <stdarg.h>

/* Object definition.
 */
 
struct gui_widget_confirm {
  struct gui_widget hdr;
  void (*cb)(struct gui_widget *confirm,int p,void *userdata);
  void *userdata;
  struct gui_texture *prompt;
  int margin;
  int focusp;
};

#define WIDGET ((struct gui_widget_confirm*)widget)

/* Cleanup.
 */
 
static void _confirm_del(struct gui_widget *widget) {
  gui_texture_del(WIDGET->prompt);
}

/* Init.
 */
 
static int _confirm_init(struct gui_widget *widget) {
  WIDGET->margin=10;
  return 0;
}

/* Measure.
 */
 
static void _confirm_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  if (WIDGET->prompt) {
    gui_texture_get_size(w,h,WIDGET->prompt);
  } else {
    *w=*h=0;
  }
  int maxchh=0;
  int sumchw=0;
  int i=widget->childc;
  while (i-->0) {
    struct gui_widget *child=widget->childv[i];
    int chw,chh;
    gui_widget_measure(&chw,&chh,child,wlimit,hlimit);
    sumchw+=chw+WIDGET->margin;
    if (chh>maxchh) maxchh=chh;
  }
  if (sumchw>*w) *w=sumchw;
  (*h)+=maxchh;
  (*w)+=WIDGET->margin*2;
  (*h)+=WIDGET->margin*3;
}

/* Pack.
 * Children line up along the bottom.
 */
 
static void _confirm_pack(struct gui_widget *widget) {
  if (widget->childc<1) return;
  int x=WIDGET->margin;
  int i=0;
  // Initially arrange all children flush left with the minimal spacing.
  for (;i<widget->childc;i++) {
    struct gui_widget *child=widget->childv[i];
    int chw,chh;
    gui_widget_measure(&chw,&chh,child,widget->w,widget->h);
    child->x=x;
    child->y=widget->h-WIDGET->margin-chh;
    child->w=chw;
    child->h=chh;
    gui_widget_pack(child);
    x+=chw+WIDGET->margin;
  }
  // If we are short of the right edge, add space between the children.
  if ((x<widget->w)&&(widget->childc>1)) { // (unless there's just one; then leave it at the left)
    int gapc=widget->childc-1;
    int addtotal=widget->w-x;
    int addeach=addtotal/gapc;
    int addmore=addtotal%gapc;
    int accum=0;
    // The first child is already in its ideal position, so start at 1.
    for (i=1;i<widget->childc;i++) {
      struct gui_widget *child=widget->childv[i];
      accum+=addeach;
      child->x+=accum;
      if (addmore) {
        accum++;
        child->x++;
        addmore--;
      }
    }
  }
  // And finally pack them.
  for (i=widget->childc;i-->0;) {
    struct gui_widget *child=widget->childv[i];
    gui_widget_pack(child);
  }
}

/* Draw.
 */
 
static void _confirm_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x400000ff);
  if (WIDGET->prompt) {
    int texw,texh;
    gui_texture_get_size(&texw,&texh,WIDGET->prompt);
    int dstx=x+(widget->w>>1)-(texw>>1);
    int dsty=y+WIDGET->margin;
    gui_draw_texture(widget->gui,dstx,dsty,WIDGET->prompt);
  }
  int i=widget->childc;
  while (i-->0) {
    struct gui_widget *child=widget->childv[i];
    gui_widget_draw(child,x+child->x,y+child->y);
  }
}

/* Signal focussed child.
 */
 
static void confirm_signal_focus(struct gui_widget *widget,int sigid) {
  if (WIDGET->focusp<0) return;
  if (WIDGET->focusp>=widget->childc) return;
  struct gui_widget *focus=widget->childv[WIDGET->focusp];
  if (focus->type->signal) focus->type->signal(focus,sigid);
}

/* Motion.
 */
 
static void _confirm_motion(struct gui_widget *widget,int dx,int dy) {
  if (dx) {
    if (widget->childc<1) return;
    int np=WIDGET->focusp+dx;
    if (np<0) np=widget->childc-1;
    else if (np>=widget->childc) np=0;
    if (np==WIDGET->focusp) return;
    confirm_signal_focus(widget,GUI_SIGID_BLUR);
    WIDGET->focusp=np;
    confirm_signal_focus(widget,GUI_SIGID_FOCUS);
  }
}

/* Signal.
 */
 
static void _confirm_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_ACTIVATE: confirm_signal_focus(widget,GUI_SIGID_ACTIVATE); break;
    case GUI_SIGID_CANCEL: gui_dismiss_modal(widget->gui,widget); break;
  }
}

/* Button callback.
 */
 
static void confirm_cb_button(struct gui_widget *button,void *userdata) {
  struct gui_widget *widget=userdata;
  if (WIDGET->cb) WIDGET->cb(widget,WIDGET->focusp,WIDGET->userdata);
}

/* Type definition.
 */
 
const struct gui_widget_type gui_widget_type_confirm={
  .name="confirm",
  .objlen=sizeof(struct gui_widget_confirm),
  .del=_confirm_del,
  .init=_confirm_init,
  .measure=_confirm_measure,
  .pack=_confirm_pack,
  .draw=_confirm_draw,
  .motion=_confirm_motion,
  .signal=_confirm_signal,
};

/* Setup.
 */
 
int gui_widget_confirm_setup_(
  struct gui_widget *widget,
  const char *prompt,
  void (*cb)(struct gui_widget *confirm,int p,void *userdata),
  void *userdata,
  ... // Button labels, nul-terminated strings.
) {
  if (!widget||(widget->type!=&gui_widget_type_confirm)) return -1;
  WIDGET->cb=cb;
  WIDGET->userdata=userdata;
  
  if (!(WIDGET->prompt=gui_texture_from_text(widget->gui,0,prompt,-1,0xffffff))) return -1;
  
  va_list vargs;
  va_start(vargs,userdata);
  while (1) {
    const char *label=va_arg(vargs,const char*);
    if (!label) break;
    if (!gui_widget_button_spawn(widget,label,-1,0xffffff,confirm_cb_button,widget)) return -1;
  }
  
  WIDGET->focusp=0;
  confirm_signal_focus(widget,GUI_SIGID_FOCUS);
  return 0;
}
