#include "../mn_internal.h"
#include "opt/serial/serial.h"
#include "opt/gui/gui_font.h"

static void lists_cb_new(struct gui_widget *button,void *userdata);
static void lists_cb_toggle(struct gui_widget *button,void *userdata);

/* Object definition.
 */
 
struct mn_widget_lists {
  struct gui_widget hdr;
  void (*cb)(struct gui_widget *lists,int add,const char *name,int namec,void *userdata);
  void *userdata;
  int focusp; // 0..childc-1
  int inc; // how much of (childv) is the "in" list
  int margin; // edges, and separator between columns
  struct gui_font *font; // WEAK
};

#define WIDGET ((struct mn_widget_lists*)widget)

/* Cleanup.
 */
 
static void _lists_del(struct gui_widget *widget) {
}

/* Init.
 */
 
static int _lists_init(struct gui_widget *widget) {
  if (!(gui_widget_button_spawn(widget,"New List...",-1,0x80ff80,lists_cb_new,widget))) return -1;
  if (!(WIDGET->font=gui_font_get_default(widget->gui))) return -1;
  WIDGET->inc=0;
  WIDGET->margin=10;
  WIDGET->focusp=-1;
  return 0;
}

/* Measure.
 */
 
static void _lists_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  
  // Height is the maximum row count we could have on one side, ie the count of children.
  // But don't let that go below say five? For aesthetics.
  (*h)=widget->childc;
  if (*h<5) *h=5;
  (*h)*=WIDGET->font->lineh;
  (*h)+=WIDGET->margin<<1;
  
  // Width is double the widest child, or some minimum.
  // So any child can change column without us needing to resize.
  *w=200;
  int i=widget->childc;
  while (i-->0) {
    struct gui_widget *child=widget->childv[i];
    int chw,chh;
    gui_widget_measure(&chw,&chh,child,wlimit,hlimit);
    if (chw>*w) *w=chw;
  }
  (*w)<<=1;
  (*w)+=WIDGET->margin*3;
}

/* Pack.
 */
 
static void _lists_pack(struct gui_widget *widget) {
  int colw=(widget->w-WIDGET->margin*3)>>1;
  if (colw<1) colw=1;
  int x=WIDGET->margin;
  int y=WIDGET->margin;
  int i=0;
  for (;i<WIDGET->inc;i++) {
    struct gui_widget *child=widget->childv[i];
    int chw,chh;
    gui_widget_measure(&chw,&chh,child,widget->w,widget->h);
    child->x=x;
    child->y=y;
    child->w=colw;
    child->h=chh;
    gui_widget_pack(child);
    y+=chh;
  }
  x=WIDGET->margin+colw+WIDGET->margin;
  y=WIDGET->margin;
  for (;i<widget->childc;i++) {
    struct gui_widget *child=widget->childv[i];
    int chw,chh;
    gui_widget_measure(&chw,&chh,child,widget->w,widget->h);
    child->x=x;
    child->y=y;
    child->w=colw;
    child->h=chh;
    gui_widget_pack(child);
    y+=chh;
  }
}

/* Draw.
 */
 
static void _lists_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x303030ff);
  //TODO Do we need static "Remove:" and "Add:" labels?
  int i=0;
  for (;i<widget->childc;i++) {
    struct gui_widget *child=widget->childv[i];
    gui_widget_draw(child,x+child->x,y+child->y);
  }
}

/* Create a new list.
 */
 
static void lists_cb_new_ready(struct gui_widget *entry,const char *name,int namec,void *userdata) {
  struct gui_widget *widget=userdata;
  if (namec) {
    struct gui_widget *button=gui_widget_button_spawn(widget,name,namec,0xffffff,lists_cb_toggle,widget);
    if (button) {
      gui_widget_insert_child(widget,WIDGET->inc,button);
      if (WIDGET->focusp>=WIDGET->inc) WIDGET->focusp++;
      WIDGET->inc++;
      gui_widget_pack(widget);
      if (WIDGET->cb) WIDGET->cb(widget,-1,name,namec,WIDGET->userdata);
    }
  }
  gui_dismiss_modal(widget->gui,entry);
}
 
static void lists_cb_new(struct gui_widget *button,void *userdata) {
  struct gui_widget *widget=userdata;
  struct gui_widget *modal=gui_push_modal(widget->gui,&gui_widget_type_entry);
  if (!modal) return;
  gui_widget_entry_setup(modal,"",0,lists_cb_new_ready,widget);
}

/* Move this list from in to out or vice-versa.
 */
 
static void lists_cb_toggle(struct gui_widget *button,void *userdata) {
  struct gui_widget *widget=userdata;
  
  // Find it in the child list. (childp) should be the same as (focusp) but let's not make any assumptions.
  int childp=-1,i=widget->childc;
  while (i-->0) {
    if (widget->childv[i]==button) {
      childp=i;
      break;
    }
  }
  if (childp<0) return;
  if (childp==WIDGET->inc) return; // We can't move the "New List..." button!
  
  const char *name=0;
  int namec=gui_widget_button_get_text(&name,button);
  if (childp>WIDGET->inc) {
    if (gui_widget_insert_child(widget,WIDGET->inc,button)<0) return;
    if (WIDGET->focusp==childp) WIDGET->focusp=WIDGET->inc;
    WIDGET->inc++;
    gui_widget_pack(widget);
    if (WIDGET->cb) WIDGET->cb(widget,1,name,namec,WIDGET->userdata);
  } else {
    if (gui_widget_insert_child(widget,widget->childc,button)<0) return;
    if (WIDGET->focusp==childp) WIDGET->focusp=widget->childc-1;
    WIDGET->inc--;
    gui_widget_pack(widget);
    if (WIDGET->cb) WIDGET->cb(widget,0,name,namec,WIDGET->userdata);
  }
}

/* Events to focussed child.
 */
 
static void lists_signal_child(struct gui_widget *widget,int sigid) {
  if (WIDGET->focusp<0) return;
  if (WIDGET->focusp>=widget->childc) return;
  struct gui_widget *child=widget->childv[WIDGET->focusp];
  if (child->type->signal) child->type->signal(child,sigid);
}

static void lists_blur(struct gui_widget *widget) { lists_signal_child(widget,GUI_SIGID_BLUR); }
static void lists_focus(struct gui_widget *widget) { lists_signal_child(widget,GUI_SIGID_FOCUS); }
static void lists_activate(struct gui_widget *widget) { lists_signal_child(widget,GUI_SIGID_ACTIVATE); }

/* Motion.
 */
 
static void _lists_motion(struct gui_widget *widget,int dx,int dy) {
  lists_blur(widget);
  
  if ((WIDGET->focusp<0)||(WIDGET->focusp>=widget->childc)) {
    WIDGET->focusp=0;
  
  } else if (dx) {
    // Switch to the other column, and try to preserve the vertical position.
    int np;
    if (WIDGET->focusp<WIDGET->inc) {
      np=WIDGET->focusp+WIDGET->inc;
      if (np>=widget->childc) np=widget->childc-1;
    } else {
      np=WIDGET->focusp-WIDGET->inc;
      if (np>=WIDGET->inc) np=WIDGET->inc-1;
      else if (np<0) np=0;
    }
    WIDGET->focusp=np;
  
  } else if (dy) {
    // Up and down, and wrap at the (inc) boundary -- remain in the same column.
    if (WIDGET->focusp<WIDGET->inc) { // left side
      WIDGET->focusp+=dy;
      if (WIDGET->focusp<0) WIDGET->focusp=WIDGET->inc-1;
      else if (WIDGET->focusp>=WIDGET->inc) WIDGET->focusp=0;
    } else { // right side
      WIDGET->focusp+=dy;
      if (WIDGET->focusp<WIDGET->inc) WIDGET->focusp=widget->childc-1;
      else if (WIDGET->focusp>=widget->childc) WIDGET->focusp=WIDGET->inc;
    }
  
  }
  lists_focus(widget);
}

/* Signal.
 */
 
static void _lists_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_CANCEL: gui_dismiss_modal(widget->gui,widget); return;
    case GUI_SIGID_ACTIVATE: lists_activate(widget); break;
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_lists={
  .name="lists",
  .objlen=sizeof(struct mn_widget_lists),
  .del=_lists_del,
  .init=_lists_init,
  .measure=_lists_measure,
  .pack=_lists_pack,
  .draw=_lists_draw,
  .motion=_lists_motion,
  .signal=_lists_signal,
};

/* Iterate lists. Accepts plain strings or {id,name}.
 */
 
static int lists_for_each(const char *src,int srcc,int (*cb)(const char *name,int namec,void *userdata),void *userdata) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_array_start(&decoder)<0) return -1;
  while (sr_decode_json_next(0,&decoder)>0) {
    char name[256];
    int namec=0;
  
    if (sr_decode_json_peek(&decoder)=='{') {
      int subctx=sr_decode_json_object_start(&decoder);
      if (subctx<0) return -1;
      const char *k;
      int kc;
      while ((kc=sr_decode_json_next(&k,&decoder))>0) {
        if ((kc==4)&&!memcmp(k,"name",4)) {
          namec=sr_decode_json_string(name,sizeof(name),&decoder);
          if ((namec<0)||(namec>sizeof(name))) return -1;
        } else {
          if (sr_decode_json_skip(&decoder)<0) return -1;
        }
      }
      if (sr_decode_json_end(&decoder,subctx)<0) return -1;
    
    } else {
      namec=sr_decode_json_string(name,sizeof(name),&decoder);
      if ((namec<0)||(namec>sizeof(name))) return -1;
    }
    
    if (!namec) continue; // Lists aren't required to have a name, but we won't display if they don't. (and that would be weird)
    
    int err=cb(name,namec,userdata);
    if (err) return err;
  }
  return 0;
}

/* Setup callback: Add to "out" column.
 */
 
static int lists_add_to_out(const char *name,int namec,void *userdata) {
  struct gui_widget *widget=userdata;
  struct gui_widget *button=gui_widget_button_spawn(widget,name,namec,0xffffff,lists_cb_toggle,widget);
  if (!button) return -1;
  return 0;
}

/* Setup callback: Move existing list to "in" column.
 */
 
static int lists_shuffle_to_in(const char *name,int namec,void *userdata) {
  struct gui_widget *widget=userdata;
  struct gui_widget *button=0;
  int i=WIDGET->inc+1;
  for (;i<widget->childc;i++) {
    struct gui_widget *child=widget->childv[i];
    const char *qname=0;
    int qnamec=gui_widget_button_get_text(&qname,child);
    if (qnamec!=namec) continue;
    if (memcmp(qname,name,namec)) continue;
    button=child;
    break;
  }
  if (!button) return 0;
  if (gui_widget_insert_child(widget,WIDGET->inc,button)<0) return -1;
  WIDGET->inc++;
  return 0;
}

/* Public setup.
 */
 
int mn_widget_lists_setup(
  struct gui_widget *widget,
  const char *joined,int joinedc,
  const char *all,int allc,
  void (*cb)(struct gui_widget *lists,int add,const char *name,int namec,void *userdata),
  void *userdata
) {
  if (!widget||(widget->type!=&mn_widget_type_lists)) return -1;
  if (widget->childc!=1) return -1; // We expect to be setup with one child initially: "New List..."
  WIDGET->cb=cb;
  WIDGET->userdata=userdata;
  
  // Append a button child for everything in (all), straight up. These implicitly land in the "out" list.
  lists_for_each(all,allc,lists_add_to_out,widget);
  
  // Now for everything in (joined), find the button, move it to (inc), and increment (inc).
  // Everything in (joined) must also be in (all), and if not we don't want it.
  lists_for_each(joined,joinedc,lists_shuffle_to_in,widget);
  
  return 0;
}

/* Encode full value per UI state.
 */
 
int mn_widget_lists_encode(char *dst,int dsta,struct gui_widget *widget) {
  if (!dst||(dsta<0)) dsta=0;
  if (!widget||(widget->type!=&mn_widget_type_lists)) return -1;
  struct sr_encoder encoder={0};
  int arrayctx=sr_encode_json_array_start(&encoder,0,0);
  int i=0;
  for (;i<WIDGET->inc;i++) {
    const char *name=0;
    int namec=gui_widget_button_get_text(&name,widget->childv[i]);
    if (namec<=0) continue;
    int subctx=sr_encode_json_object_start(&encoder,0,0);
    sr_encode_json_string(&encoder,"name",4,name,namec);
    sr_encode_json_object_end(&encoder,subctx);
  }
  sr_encode_json_array_end(&encoder,0);
  if (encoder.c<=dsta) {
    memcpy(dst,encoder.v,encoder.c);
  }
  sr_encoder_cleanup(&encoder);
  return encoder.c;
}
