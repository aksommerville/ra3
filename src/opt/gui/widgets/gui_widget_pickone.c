#include "../gui_internal.h"
#include "lib/emuhost.h"

static void pickone_set_scroll_target(struct gui_widget *widget);

/* Object definition.
 */
 
struct gui_widget_pickone {
  struct gui_widget hdr;
  void (*cb)(struct gui_widget *widget,int p,void *userdata);
  void *userdata;
  int padding,spacing;
  int focusp;
  int scroll,scroll_limit;
  int scroll_target;
  int pvh; // our height, at the last pack or init. So we can determine whether to suppress animation.
};

#define WIDGET ((struct gui_widget_pickone*)widget)

/* Delete.
 */
 
static void _pickone_del(struct gui_widget *widget) {
}

/* Init.
 */
 
static int _pickone_init(struct gui_widget *widget) {
  WIDGET->padding=10;
  WIDGET->spacing=0; // for typical text labels, spacing should not be necessary
  WIDGET->focusp=-1;
  return 0;
}

/* Measure.
 */
 
static void _pickone_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  *w=*h=0;
  int i=widget->childc;
  while (i-->0) {
    struct gui_widget *child=widget->childv[i];
    int chw,chh;
    gui_widget_measure(&chw,&chh,child,wlimit,hlimit);
    if (chw>*w) *w=chw;
    (*h)+=chh;
  }
  if (widget->childc>1) (*h)+=WIDGET->spacing*(widget->childc-1);
  (*h)+=(WIDGET->padding<<1);
  (*w)+=(WIDGET->padding<<1);
}

/* Pack.
 */
 
static void _pickone_pack(struct gui_widget *widget) {
  int from_zero_height=!WIDGET->pvh;
  WIDGET->pvh=widget->h;
  int i=0,y=WIDGET->padding;
  int wall=widget->w-(WIDGET->padding<<1);
  if (wall<0) wall=0;
  for (;i<widget->childc;i++) {
    struct gui_widget *child=widget->childv[i];
    int chw,chh;
    gui_widget_measure(&chw,&chh,child,widget->w,widget->h);
    child->x=WIDGET->padding;
    child->y=y;
    child->w=wall;
    child->h=chh;
    y+=chh;
  }
  y+=WIDGET->padding;
  WIDGET->scroll_limit=y-widget->h;
  if (WIDGET->scroll_limit<=0) {
    WIDGET->scroll_limit=0;
    WIDGET->scroll=0;
  } else {
    if (WIDGET->scroll>WIDGET->scroll_limit) WIDGET->scroll=WIDGET->scroll_limit;
  }
  pickone_set_scroll_target(widget);
  if (from_zero_height) {
    WIDGET->scroll=WIDGET->scroll_target;
  }
}

/* Draw.
 */
 
static void _pickone_draw(struct gui_widget *widget,int x,int y) {
  if (WIDGET->scroll_limit) {
    int screenw,screenh;
    gui_get_screen_size(&screenw,&screenh,widget->gui);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x,screenh-widget->h-y,widget->w,widget->h);
  }
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x202020ff);
  y-=WIDGET->scroll;
  int i=0;
  for (;i<widget->childc;i++) {
    struct gui_widget *child=widget->childv[i];
    gui_widget_draw(child,x+child->x,y+child->y);
  }
  if (WIDGET->scroll_limit) {
    glDisable(GL_SCISSOR_TEST);
  }
}

/* Update.
 */
 
static void _pickone_update(struct gui_widget *widget) {

  if (WIDGET->scroll!=WIDGET->scroll_target) {
    int dpx=WIDGET->scroll_target-WIDGET->scroll;
    int move=dpx/10;
    if (dpx>0) move++;
    WIDGET->scroll+=move;
  }
}

/* Set scroll_target so the focus is visible.
 */
 
static void pickone_set_scroll_target(struct gui_widget *widget) {
  int target=WIDGET->scroll;
  if ((WIDGET->focusp<0)||(WIDGET->focusp>=widget->childc)||(WIDGET->scroll_limit<=0)) {
    target=0;
  } else {
    struct gui_widget *focus=widget->childv[WIDGET->focusp];
    target=focus->y+(focus->h>>1)-(widget->h>>1);
    if (target<0) target=0;
    else if (target>WIDGET->scroll_limit) target=WIDGET->scroll_limit;
  }
  WIDGET->scroll_target=target;
  if (!widget->h) { // a strong clue that we haven't been packed yet -- don't animate
    WIDGET->scroll=WIDGET->scroll_target;
  }
}

/* Activate.
 */
 
static void pickone_activate(struct gui_widget *widget) {
  if ((WIDGET->focusp<0)||(WIDGET->focusp>=widget->childc)) return;
  struct gui_widget *focus=widget->childv[WIDGET->focusp];
  if (focus->type->signal) focus->type->signal(focus,GUI_SIGID_ACTIVATE);
  else if (WIDGET->cb) WIDGET->cb(widget,WIDGET->focusp,WIDGET->userdata);
}

/* Signal focus.
 */
 
static void pickone_blur(struct gui_widget *widget) {
  if ((WIDGET->focusp<0)||(WIDGET->focusp>=widget->childc)) return;
  struct gui_widget *focus=widget->childv[WIDGET->focusp];
  if (focus->type->signal) focus->type->signal(focus,GUI_SIGID_BLUR);
}
 
static void pickone_focus(struct gui_widget *widget) {
  if ((WIDGET->focusp<0)||(WIDGET->focusp>=widget->childc)) return;
  struct gui_widget *focus=widget->childv[WIDGET->focusp];
  if (focus->type->signal) focus->type->signal(focus,GUI_SIGID_FOCUS);
}

/* Motion.
 */
 
static void _pickone_motion(struct gui_widget *widget,int dx,int dy) {
  if (!dy) return;
  if (widget->childc<1) return;
  if (widget->gui->pvinput&EH_BTN_EAST) dy*=10; // hold EAST for high speed
  int np=WIDGET->focusp+dy;
  if (np<0) np=widget->childc-1;
  else if (np>=widget->childc) np=0;
  if (np==WIDGET->focusp) return;
  pickone_blur(widget);
  WIDGET->focusp=np;
  pickone_focus(widget);
  pickone_set_scroll_target(widget);
}

/* Signal.
 */
 
static void _pickone_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_CANCEL: gui_dismiss_modal(widget->gui,widget); break;
    case GUI_SIGID_ACTIVATE: pickone_activate(widget); break;
  }
}

/* Button callback.
 */
 
static void pickone_cb_pick(struct gui_widget *button,void *userdata) {
  struct gui_widget *widget=userdata;
  if (!WIDGET->cb) return;
  int p=-1,i=widget->childc;
  while (i-->0) {
    if (widget->childv[i]==button) {
      p=i;
      break;
    }
  }
  if (p<0) return;
  WIDGET->cb(widget,p,WIDGET->userdata);
}

/* Type definition.
 */
 
const struct gui_widget_type gui_widget_type_pickone={
  .name="pickone",
  .objlen=sizeof(struct gui_widget_pickone),
  .del=_pickone_del,
  .init=_pickone_init,
  .measure=_pickone_measure,
  .pack=_pickone_pack,
  .draw=_pickone_draw,
  .update=_pickone_update,
  .motion=_pickone_motion,
  .signal=_pickone_signal,
};

/* Public accessors.
 */
 
void gui_widget_pickone_set_callback(struct gui_widget *widget,void (*cb)(struct gui_widget *pickone,int p,void *userdata),void *userdata) {
  if (!widget||(widget->type!=&gui_widget_type_pickone)) return;
  WIDGET->cb=cb;
  WIDGET->userdata=userdata;
}

struct gui_widget *gui_widget_pickone_add_option(struct gui_widget *widget,const char *label,int labelc) {
  if (!widget||(widget->type!=&gui_widget_type_pickone)) return 0;
  struct gui_widget *button=gui_widget_button_spawn(widget,label,labelc,0xffffffff,pickone_cb_pick,widget);
  if (!button) return 0;
  return button;
}

void gui_widget_pickone_focus(struct gui_widget *widget,struct gui_widget *option) {
  if (!widget||(widget->type!=&gui_widget_type_pickone)) return;
  if (!option) {
    pickone_blur(widget);
    WIDGET->focusp=-1;
    return;
  }
  int i=widget->childc;
  while (i-->0) {
    if (widget->childv[i]==option) {
      pickone_blur(widget);
      WIDGET->focusp=i;
      pickone_focus(widget);
      pickone_set_scroll_target(widget);
      return;
    }
  }
}
