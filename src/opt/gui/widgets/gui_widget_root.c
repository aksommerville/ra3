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

/* Position modal near its anchor widget.
 * We measure the modal and finalize its bounds but do not pack it.
 */
 
static void root_position_modal_by_anchor(struct gui_widget *widget,struct gui_widget *modal,struct gui_widget *anchor) {

  // Determine anchor's position globally.
  // We may assume that (modal->(x,y)) are in global coordinates.
  int ax=0,ay=0,aw=anchor->w,ah=anchor->h;
  for (;anchor;anchor=anchor->parent) {
    ax+=anchor->x;
    ay+=anchor->y;
  }
  
  // Measure the modal, but lie about the available height -- we want to know if it is vertically truncated, eg for long popup menus.
  int chw=0,chh=0;
  gui_widget_measure(&chw,&chh,modal,widget->w,widget->h+1);
  
  // Most preferred placement is centered (then horz clamp) below anchor. Take that if it fits.
  int available=widget->h-ah-ay;
  if (chh<=available) {
    modal->y=ah+ay;
    modal->x=ax+(aw>>1)-(chw>>1);
    if (modal->x<0) modal->x=0;
    else if (modal->x+chw>widget->w) modal->x=widget->w-chw;
    modal->w=chw;
    modal->h=chh;
    return;
  }
  
  // If it fits similarly above, take that.
  if (chh<=ay) {
    modal->y=ay-chh;
    modal->x=ax+(aw>>1)-(chw>>1);
    if (modal->x<0) modal->x=0;
    else if (modal->x+chw>widget->w) modal->x=widget->w-chw;
    modal->w=chw;
    modal->h=chh;
    return;
  }
  
  // If it fits left or right without truncating, take it. Prefer right and aim to align top edges.
  // This case is why we lied about the parent's height at measure.
  if (chh<=widget->h) {
    available=widget->w-aw-ax;
    if (chw<=available) {
      modal->x=aw+ax;
      modal->y=ay;
      if (modal->y+chh>widget->h) modal->y=widget->h-chh;
      modal->w=chw;
      modal->h=chh;
      return;
    }
    if (chw<=ax) {
      modal->x=ax-chw;
      modal->y=ay;
      if (modal->y+chh>widget->h) modal->y=widget->h-chh;
      modal->w=chw;
      modal->h=chh;
      return;
    }
  }
  
  // Put above or below, whichever is larger, truncate vertically, and center horizontally.
  available=widget->h-ah-ay;
  if (ay>available) { // above
    modal->y=0;
    modal->h=ay;
  } else { // below
    modal->y=ay+ah;
    modal->h=widget->h-ah-ay;
  }
  modal->x=ax+(aw>>1)-(chw>>1);
  if (modal->x<0) modal->x=0;
  else if (modal->x+chw>widget->w) modal->x=widget->w-chw;
  modal->w=chw;
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
   * Unless they have a (modal_anchor), in which case we use that for positioning.
   */
  int i=1;
  for (;i<widget->childc;i++) {
    struct gui_widget *modal=widget->childv[i];
    if (modal->modal_anchor) {
      root_position_modal_by_anchor(widget,modal,modal->modal_anchor);
    } else {
      int chw=0,chh=0;
      gui_widget_measure(&chw,&chh,modal,widget->w,widget->h);
      modal->w=chw;
      modal->h=chh;
      modal->x=(widget->w>>1)-(chw>>1);
      modal->y=(widget->h>>1)-(chh>>1);
    }
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

/* Set anchor for modal.
 */
 
void gui_root_place_modal_near(struct gui_widget *widget,struct gui_widget *modal,struct gui_widget *anchor) {
  if (!widget||(widget->type!=&gui_widget_type_root)) return;
  if (!gui_widget_has_child(widget,modal)) return;
  if (widget->childv[0]==modal) return; // (modal) is actually the page -- invalid
  if (anchor&&(gui_widget_ref(anchor)<0)) return;
  gui_widget_del(modal->modal_anchor);
  modal->modal_anchor=anchor;
  gui_dirty_pack(widget->gui);
}
