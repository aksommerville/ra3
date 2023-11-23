#include "../gui_internal.h"
#include "opt/serial/serial.h"

#define GUI_FORM_K_LIMIT 16
#define FORM_KEY_COLOR 0xa0a0a0
#define FORM_VALUE_COLOR_ENABLE 0xffffff
#define FORM_VALUE_COLOR_DISABLE 0xa0a0a0

/* Object definition.
 */
 
struct gui_widget_form {
  struct gui_widget hdr;
  struct gui_form_row {
    char k[GUI_FORM_K_LIMIT];
    int kc;
    struct gui_texture *ktex;
    int kx,ky;
    struct gui_widget *v; // WEAK; must be in childv too
    int custom;
  } *rowv;
  int rowc,rowa;
  int margin; // left, right, and center
  int focusp;
  void (*cb)(struct gui_widget *widget,const char *k,int kc,const char *v,int vc,void *userdata);
  void *userdata;
};

#define WIDGET ((struct gui_widget_form*)widget)

/* Cleanup.
 */
 
static void gui_form_row_cleanup(struct gui_form_row *row) {
  gui_texture_del(row->ktex);
}
 
static void _form_del(struct gui_widget *widget) {
  if (WIDGET->rowv) {
    while (WIDGET->rowc-->0) gui_form_row_cleanup(WIDGET->rowv+WIDGET->rowc);
    free(WIDGET->rowv);
  }
}

/* Init.
 */
 
static int _form_init(struct gui_widget *widget) {
  WIDGET->margin=20;
  WIDGET->focusp=-1;
  return 0;
}

/* Measure.
 */
 
static void _form_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  *w=*h=0;
  int kw=0,vw=0;
  struct gui_form_row *row=WIDGET->rowv;
  int i=WIDGET->rowc;
  for (;i-->0;row++) {
    int rh=0;
    if (row->ktex) {
      int tw,th;
      gui_texture_get_size(&tw,&th,row->ktex);
      if (tw>kw) kw=tw;
      rh=th;
    }
    if (row->v) {
      int cw,ch;
      gui_widget_measure(&cw,&ch,row->v,wlimit,hlimit);
      if (cw>vw) vw=cw;
      if (ch>rh) rh=ch;
    }
    (*h)+=rh;
  }
  (*w)=kw+vw+WIDGET->margin*3;
}

/* Pack.
 */
 
static void _form_pack(struct gui_widget *widget) {
  int maxkw=0;
  int y=0,i=WIDGET->rowc;
  struct gui_form_row *row=WIDGET->rowv;
  for (;i-->0;row++) {
    int kw=0,kh=0,vw=0,vh=0;
    if (row->ktex) {
      gui_texture_get_size(&kw,&kh,row->ktex);
      row->kx=kw; // for safekeeping temporarily
      row->ky=y; // final
    }
    if (row->v) {
      gui_widget_measure(&vw,&vh,row->v,widget->w-kw,widget->h);
      // (x,w) will be finalized after we've seen every row.
      row->v->y=y;
      row->v->w=vw;
      row->v->h=vh;
    }
    y+=(kh>vh)?kh:vh;
    if (kw>maxkw) maxkw=kw;
  }
  // Add a tasteful separator between key and value columns, and on left and right edges.
  maxkw+=WIDGET->margin*2;
  // Now (maxkw) is final, run thru again and set final horizontals.
  for (i=WIDGET->rowc,row=WIDGET->rowv;i-->0;row++) {
    row->kx=maxkw-WIDGET->margin-row->kx;
    if (row->v) {
      row->v->x=maxkw;
      row->v->w=widget->w-maxkw-WIDGET->margin;
      gui_widget_pack(row->v);
    }
  }
}

/* Draw.
 */
 
static void _form_draw(struct gui_widget *widget,int x,int y) {
  //TODO scroll and scissor
  struct gui_form_row *row=WIDGET->rowv;
  int i=WIDGET->rowc;
  for (;i-->0;row++) {
    if (row->ktex) {
      gui_draw_texture(widget->gui,x+row->kx,y+row->ky,row->ktex);
    }
    if (row->v) {
      gui_widget_draw(row->v,x+row->v->x,y+row->v->y);
    }
  }
}

/* Events to child.
 */
 
static void form_signal_child(struct gui_widget *widget,int sigid) {
  if ((WIDGET->focusp<0)||(WIDGET->focusp>=WIDGET->rowc)) return;
  struct gui_form_row *row=WIDGET->rowv+WIDGET->focusp;
  if (!row->v) return;
  if (!row->v->type->signal) return;
  if (row->v->type==&gui_widget_type_button) {
    if (!gui_widget_button_get_enable(row->v)) return;
  }
  row->v->type->signal(row->v,sigid);
}
 
static void form_blur(struct gui_widget *widget) { form_signal_child(widget,GUI_SIGID_BLUR); }
static void form_focus(struct gui_widget *widget) { form_signal_child(widget,GUI_SIGID_FOCUS); }
static void form_activate(struct gui_widget *widget) { form_signal_child(widget,GUI_SIGID_ACTIVATE); }

/* Motion.
 */
 
static void _form_motion(struct gui_widget *widget,int dx,int dy) {
  if (dy) {
    if (WIDGET->rowc<2) return;
    int np=WIDGET->focusp;
    int panic=WIDGET->rowc;
    while (panic-->0) {
      np+=dy;
      if (np<0) np=WIDGET->rowc-1;
      else if (np>=WIDGET->rowc) np=0;
      struct gui_widget *child=WIDGET->rowv[np].v;
      if (!child) continue;
      if (child->type==&gui_widget_type_button) {
        if (gui_widget_button_get_enable(WIDGET->rowv[np].v)) break;
      } else if (child->type->signal) {
        break; // assume that anything we can signal, can accept focus
      }
    }
    if (np==WIDGET->focusp) return;
    form_blur(widget);
    WIDGET->focusp=np;
    form_focus(widget);
    return;
  }
  if (dx) {
    if ((WIDGET->focusp>=0)&&(WIDGET->focusp<WIDGET->rowc)) {
      struct gui_widget *child=WIDGET->rowv[WIDGET->focusp].v;
      if (child&&child->type->motion) child->type->motion(child,dx,0);
    }
  }
}

/* Signal.
 */
 
static void _form_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_ACTIVATE: form_activate(widget); break;
    case GUI_SIGID_FOCUS: form_focus(widget); break;
    case GUI_SIGID_BLUR: form_blur(widget); break;
  }
}

/* Edit complete.
 */
 
static void form_cb_edited(struct gui_widget *entry,const char *v,int c,void *userdata) {
  struct gui_widget *widget=userdata;
  // It's a little risky, but we have to assume that (focusp) is the row being edited.
  if ((WIDGET->focusp>=0)&&(WIDGET->focusp<WIDGET->rowc)) {
    struct gui_form_row *row=WIDGET->rowv+WIDGET->focusp;
    gui_widget_button_set_label(row->v,v,c,FORM_VALUE_COLOR_ENABLE);
    gui_dirty_pack(widget->gui);
    if (WIDGET->cb) WIDGET->cb(widget,row->k,row->kc,v,c,WIDGET->userdata);
  }
  gui_dismiss_modal(widget->gui,entry);
}

/* Button actuated.
 */
 
static void form_cb_button(struct gui_widget *button,void *userdata) {
  struct gui_widget *widget=userdata;
  struct gui_form_row *row=WIDGET->rowv;
  int i=WIDGET->rowc;
  for (;i-->0;row++) {
    if (row->v==button) {
      if (row->custom) {
        if (WIDGET->cb) WIDGET->cb(widget,row->k,row->kc,0,-1,WIDGET->userdata);
      } else {
        struct gui_widget *modal=gui_push_modal(widget->gui,&gui_widget_type_entry);
        if (!modal) return;
        const char *text=0;
        int textc=gui_widget_button_get_text(&text,row->v);
        gui_widget_entry_setup(modal,text,textc,form_cb_edited,widget);
        gui_modal_place_near(modal,button);
      }
      return;
    }
  }
}

/* Type definition.
 */
 
const struct gui_widget_type gui_widget_type_form={
  .name="form",
  .objlen=sizeof(struct gui_widget_form),
  .del=_form_del,
  .init=_form_init,
  .measure=_form_measure,
  .pack=_form_pack,
  .draw=_form_draw,
  .motion=_form_motion,
  .signal=_form_signal,
};

/* Add row, internal.
 */
 
static struct gui_form_row *gui_widget_form_add_row(struct gui_widget *widget,const char *k,int kc) {
  if (!widget||(widget->type!=&gui_widget_type_form)) return 0;
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (kc>GUI_FORM_K_LIMIT) kc=GUI_FORM_K_LIMIT;
  if (WIDGET->rowc>=WIDGET->rowa) {
    int na=WIDGET->rowa+16;
    if (na>INT_MAX/sizeof(struct gui_form_row)) return 0;
    void *nv=realloc(WIDGET->rowv,sizeof(struct gui_form_row)*na);
    if (!nv) return 0;
    WIDGET->rowv=nv;
    WIDGET->rowa=na;
  }
  struct gui_form_row *row=WIDGET->rowv+WIDGET->rowc++;
  memset(row,0,sizeof(struct gui_form_row));
  memcpy(row->k,k,kc);
  row->kc=kc;
  if (!(row->ktex=gui_texture_from_text(widget->gui,0,k,kc,FORM_KEY_COLOR))) return 0;
  return row;
}

/* Add row, public conveniences.
 */
 
struct gui_widget *gui_widget_form_add_string(struct gui_widget *widget,const char *k,int kc,const char *v,int vc,int custom) {
  struct gui_form_row *row=gui_widget_form_add_row(widget,k,kc);
  if (!row) return 0;
  row->custom=custom;
  if (!(row->v=gui_widget_button_spawn(widget,v,vc,FORM_VALUE_COLOR_ENABLE,form_cb_button,widget))) return 0;
  if (WIDGET->focusp<0) {
    WIDGET->focusp=WIDGET->rowc-1;
    form_focus(widget);
  }
  return row->v;
}

struct gui_widget *gui_widget_form_add_int(struct gui_widget *widget,const char *k,int kc,int v,int custom) {
  struct gui_form_row *row=gui_widget_form_add_row(widget,k,kc);
  if (!row) return 0;
  row->custom=custom;
  char tmp[16];
  int tmpc=snprintf(tmp,sizeof(tmp),"%d",v);
  if ((tmpc<1)||(tmpc>=sizeof(tmp))) return 0;
  if (!(row->v=gui_widget_button_spawn(widget,tmp,tmpc,FORM_VALUE_COLOR_ENABLE,form_cb_button,widget))) return 0;
  if (WIDGET->focusp<0) {
    WIDGET->focusp=WIDGET->rowc-1;
    form_focus(widget);
  }
  return row->v;
}

struct gui_widget *gui_widget_form_add_readonly_string(struct gui_widget *widget,const char *k,int kc,const char *v,int vc) {
  struct gui_form_row *row=gui_widget_form_add_row(widget,k,kc);
  if (!row) return 0;
  if (!(row->v=gui_widget_button_spawn(widget,v,vc,FORM_VALUE_COLOR_DISABLE,form_cb_button,widget))) return 0;
  gui_widget_button_enable(row->v,0);
  return row->v;
}

struct gui_widget *gui_widget_form_add_readonly_int(struct gui_widget *widget,const char *k,int kc,int v) {
  struct gui_form_row *row=gui_widget_form_add_row(widget,k,kc);
  if (!row) return 0;
  char tmp[16];
  int tmpc=snprintf(tmp,sizeof(tmp),"%d",v);
  if ((tmpc<1)||(tmpc>=sizeof(tmp))) return 0;
  if (!(row->v=gui_widget_button_spawn(widget,tmp,tmpc,FORM_VALUE_COLOR_DISABLE,form_cb_button,widget))) return 0;
  gui_widget_button_enable(row->v,0);
  return row->v;
}
 
struct gui_widget *gui_widget_form_add_string_json(struct gui_widget *widget,const char *k,int kc,const char *v,int vc,int custom) {
  char tmp[256];
  int tmpc=sr_string_eval(tmp,sizeof(tmp),v,vc);
  if ((tmpc>=0)&&(tmpc<=sizeof(tmp))) {
    v=tmp;
    vc=tmpc;
  }
  struct gui_form_row *row=gui_widget_form_add_row(widget,k,kc);
  if (!row) return 0;
  row->custom=custom;
  if (!(row->v=gui_widget_button_spawn(widget,v,vc,FORM_VALUE_COLOR_ENABLE,form_cb_button,widget))) return 0;
  if (WIDGET->focusp<0) {
    WIDGET->focusp=WIDGET->rowc-1;
    form_focus(widget);
  }
  return row->v;
}

struct gui_widget *gui_widget_form_add_readonly_string_json(struct gui_widget *widget,const char *k,int kc,const char *v,int vc) {
  char tmp[256];
  int tmpc=sr_string_eval(tmp,sizeof(tmp),v,vc);
  if ((tmpc>=0)&&(tmpc<=sizeof(tmp))) {
    v=tmp;
    vc=tmpc;
  }
  struct gui_form_row *row=gui_widget_form_add_row(widget,k,kc);
  if (!row) return 0;
  if (!(row->v=gui_widget_button_spawn(widget,v,vc,FORM_VALUE_COLOR_DISABLE,form_cb_button,widget))) return 0;
  gui_widget_button_enable(row->v,0);
  return row->v;
}

struct gui_widget *gui_widget_form_add_custom(struct gui_widget *widget,const char *k,int kc,const struct gui_widget_type *type) {
  struct gui_form_row *row=gui_widget_form_add_row(widget,k,kc);
  if (!row) return 0;
  struct gui_widget *child=gui_widget_spawn(widget,type);
  if (!child) return 0;
  row->v=child;
  return child;
}

/* Trivial accessors.
 */

void gui_widget_form_set_callback(
  struct gui_widget *widget,
  void (*cb)(struct gui_widget *widget,const char *k,int kc,const char *v,int vc,void *userdata),
  void *userdata
) {
  if (!widget||(widget->type!=&gui_widget_type_form)) return;
  WIDGET->cb=cb;
  WIDGET->userdata=userdata;
}

struct gui_widget *gui_widget_form_get_button_by_key(struct gui_widget *widget,const char *k,int kc) {
  if (!widget||(widget->type!=&gui_widget_type_form)) return 0;
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (kc>GUI_FORM_K_LIMIT) kc=GUI_FORM_K_LIMIT;
  struct gui_form_row *row=WIDGET->rowv;
  int i=WIDGET->rowc;
  for (;i-->0;row++) {
    if (row->kc!=kc) continue;
    if (memcmp(row->k,k,kc)) continue;
    return row->v;
  }
  return 0;
}
