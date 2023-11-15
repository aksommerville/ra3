#include "../gui_internal.h"
#include "lib/emuhost.h" // for button names

#define GUI_MOTION_REPEAT_TIME_INITIAL    30
#define GUI_MOTION_REPEAT_TIME_ADDITIONAL 15

/* Object definition.
 */

struct gui_widget_root {
  struct gui_widget hdr;
  uint16_t pvinput;
  int motion_repeat_clock;
};

#define WIDGET ((struct gui_widget_root*)widget)

/* Delete.
 */
 
static void _root_del(struct gui_widget *widget) {
}

/* Init.
 */
 
static int _root_init(struct gui_widget *widget) {

  /* We are required to have a "page" widget, child zero.
   * Use a blank label initially, and the gui's owner is expected to replace it.
   */
  struct gui_widget *page=gui_widget_spawn(widget,&gui_widget_type_label);
  if (!page) return -1;

  return 0;
}

/* Measure.
 * This won't actually get called for root, but for form's sake, implement and demand the whole area.
 */
 
static void _root_measure(int *w,int *h,struct gui_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
}

/* Pack.
 */
 
static void _root_pack(struct gui_widget *widget) {

  /* Page gets the full size.
   */
  if (widget->childc>=1) {
    struct gui_widget *page=widget->childv[0];
    page->x=0;
    page->y=0;
    page->w=widget->w;
    page->h=widget->h;
    gui_widget_pack(page);
  }
  
  /* Modals get whatever they ask for, and we center each.
   */
  int i=widget->childc;
  while (i-->1) {
    struct gui_widget *modal=widget->childv[i];
    int chw=0,chh=0;
    gui_widget_measure(&chw,&chh,modal,widget->w,widget->h);
    modal->x=(widget->w>>1)-(chw>>1);
    modal->y=(widget->h>>1)-(chh>>1);
    modal->w=chw;
    modal->h=chh;
    gui_widget_pack(modal);
  }
}

/* Draw.
 */
 
static void _root_draw(struct gui_widget *widget,int x,int y) {
  if (widget->childc<1) { // illegal! but we can play it sensibly
    gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x000000ff);
    return;
  }
  struct gui_widget *page=widget->childv[0];
  if (!page->opaque) gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x000000ff);
  gui_widget_draw(page,x,y);
  if (widget->childc>1) {
    // Draw all but the last modal.
    int i=1,lastp=widget->childc-1;
    for (;i<lastp;i++) {
      struct gui_widget *modal=widget->childv[i];
      gui_widget_draw(modal,x+modal->x,y+modal->y);
    }
    // Then draw a blotter.
    gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x00000080);
    // Then the top modal.
    struct gui_widget *modal=widget->childv[lastp];
    gui_widget_draw(modal,x+modal->x,y+modal->y);
  }
}

/* Dpad motion, state changes only.
 */
 
static void _root_motion(struct gui_widget *widget,int dx,int dy) {
  if (widget->childc>0) {
    struct gui_widget *child=widget->childv[widget->childc-1];
    if (child->type->motion) child->type->motion(child,dx,dy);
  }
}

/* Signals.
 */
 
static void _root_signal(struct gui_widget *widget,int sigid) {
  if (widget->childc>0) {
    struct gui_widget *child=widget->childv[widget->childc-1];
    if (child->type->signal) child->type->signal(child,sigid);
  }
}

/* Input state.
 */
 
static void _root_input_changed(struct gui_widget *widget,uint16_t input) {
  if (input==WIDGET->pvinput) return;
  WIDGET->motion_repeat_clock=GUI_MOTION_REPEAT_TIME_INITIAL;
  #define ON(tag) ((input&EH_BTN_##tag)&&!(WIDGET->pvinput&EH_BTN_##tag))
  if (ON(LEFT)) _root_motion(widget,-1,0);
  if (ON(RIGHT)) _root_motion(widget,1,0);
  if (ON(UP)) _root_motion(widget,0,-1);
  if (ON(DOWN)) _root_motion(widget,0,1);
  if (ON(SOUTH)) _root_signal(widget,GUI_SIGID_ACTIVATE);
  if (ON(WEST)) _root_signal(widget,GUI_SIGID_CANCEL);
  if (ON(EAST)) _root_signal(widget,GUI_SIGID_AUX);
  #undef ON
  WIDGET->pvinput=input;
}

/* Update.
 */
 
static void _root_update(struct gui_widget *widget) {

  /* Poke dpad motion if held.
   */
  if (WIDGET->pvinput&EH_BTN_DPAD) {
    if (WIDGET->motion_repeat_clock>0) {
      WIDGET->motion_repeat_clock--;
    } else {
      WIDGET->motion_repeat_clock=GUI_MOTION_REPEAT_TIME_ADDITIONAL;
      switch (WIDGET->pvinput&EH_BTN_HORZ) {
        case EH_BTN_LEFT: _root_motion(widget,-1,0); break;
        case EH_BTN_RIGHT: _root_motion(widget,1,0); break;
      }
      switch (WIDGET->pvinput&EH_BTN_VERT) {
        case EH_BTN_UP: _root_motion(widget,0,-1); break;
        case EH_BTN_DOWN: _root_motion(widget,0,1); break;
      }
    }
  }
  
  /* Update the topmost child only.
   */
  if (widget->childc>0) {
    struct gui_widget *child=widget->childv[widget->childc-1];
    if (child->type->update) child->type->update(child);
  }
}

/* Type definition.
 */
 
const struct gui_widget_type gui_widget_type_root={
  .name="root",
  .objlen=sizeof(struct gui_widget_root),
  .del=_root_del,
  .init=_root_init,
  .measure=_root_measure,
  .pack=_root_pack,
  .draw=_root_draw,
  .update=_root_update,
  .input_changed=_root_input_changed,
  .motion=_root_motion,
  .signal=_root_signal,
};
