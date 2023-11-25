#include "../mn_internal.h"

/* Object definition.
 */
 
struct mn_widget_interface {
  struct gui_widget hdr;
};

#define WIDGET ((struct mn_widget_interface*)widget)

/* Cleanup.
 */
 
static void _interface_del(struct gui_widget *widget) {
}

/* Edit individual fields.
 */
 
static void audio_toggle_show_invalid(struct gui_widget *widget,struct gui_widget *form) {
  mn.show_invalid=mn.show_invalid?0:1;
  struct gui_widget *button=gui_widget_form_get_button_by_key(form,"Show Invalid Games?",19);
  if (!button) return;
  gui_widget_button_set_label(button,mn.show_invalid?"Show":"Hide",4,0xffffff);
  gui_dirty_pack(widget->gui);
  dbs_refresh_search(&mn.dbs);
  //TODO notify main, to persist setting
}

/* Callback from form.
 */
 
static void interface_cb_form(struct gui_widget *form,const char *k,int kc,const char *v,int vc,void *userdata) {
  struct gui_widget *widget=userdata;
  
  if (!v&&(vc==-1)) { // Custom value editors.
    if ((kc==19)&&!memcmp(k,"Show Invalid Games?",19)) { audio_toggle_show_invalid(widget,form); return; }
    return;
  }
  
  // Text entry. Value is ready.
}

/* Init.
 */
 
static int _interface_init(struct gui_widget *widget) {
  struct gui_widget *form=gui_widget_spawn(widget,&gui_widget_type_form);
  if (!form) return -1;
  gui_widget_form_set_callback(form,interface_cb_form,widget);
  
  gui_widget_form_add_string(form,"Show Invalid Games?",19,mn.show_invalid?"Show":"Hide",4,1);
  
  return 0;
}

/* Let form control most ops.
 */
 
static void _interface_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  if (widget->childc!=1) return;
  gui_widget_measure(w,h,widget->childv[0],wlimit,hlimit);
}
 
static void _interface_pack(struct gui_widget *widget) {
  if (widget->childc!=1) return;
  struct gui_widget *form=widget->childv[0];
  form->x=0;
  form->y=0;
  form->w=widget->w;
  form->h=widget->h;
  gui_widget_pack(form);
}
 
static void _interface_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x003000ff);
  int i=0;
  for (;i<widget->childc;i++) {
    struct gui_widget *child=widget->childv[i];
    gui_widget_draw(child,x+child->x,y+child->y);
  }
}
 
static void _interface_motion(struct gui_widget *widget,int dx,int dy) {
  if (widget->childc!=1) return;
  struct gui_widget *form=widget->childv[0];
  if (form->type->motion) form->type->motion(form,dx,dy);
}
 
static void _interface_signal(struct gui_widget *widget,int sigid) {
  if (sigid==GUI_SIGID_CANCEL) {
    gui_dismiss_modal(widget->gui,widget);
  } else {
    if (widget->childc!=1) return;
    struct gui_widget *form=widget->childv[0];
    if (form->type->signal) form->type->signal(form,sigid);
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_interface={
  .name="interface",
  .objlen=sizeof(struct mn_widget_interface),
  .del=_interface_del,
  .init=_interface_init,
  .measure=_interface_measure,
  .pack=_interface_pack,
  .draw=_interface_draw,
  .motion=_interface_motion,
  .signal=_interface_signal,
};
