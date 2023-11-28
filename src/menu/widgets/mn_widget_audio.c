#include "../mn_internal.h"
#include "lib/emuhost.h"
#include "lib/eh_driver.h"
#include "opt/serial/serial.h"

/* Object definition.
 */
 
struct mn_widget_audio {
  struct gui_widget hdr;
};

#define WIDGET ((struct mn_widget_audio*)widget)

/* Cleanup.
 */
 
static void _audio_del(struct gui_widget *widget) {
}

/* Read full UI state, validate, and try carefully to replace emuhost's audio driver.
 */
 
static int audio_read_form_string(void *dstpp,struct gui_widget *form,const char *k,int kc) {
  struct gui_widget *button=gui_widget_form_get_button_by_key(form,k,kc);
  if (!button) return -1;
  return gui_widget_button_get_text(dstpp,button);
}

static int audio_read_form_uint(struct gui_widget *form,const char *k,int kc) {
  const char *tmp=0;
  int tmpc=audio_read_form_string(&tmp,form,k,kc);
  if (tmpc<1) return -1;
  int v=-1;
  if (sr_int_eval(&v,tmp,tmpc)<2) return -1;
  return v;
}
 
static void audio_replace_driver(struct gui_widget *widget) {
  if (widget->childc!=1) return;
  struct gui_widget *form=widget->childv[0];
  const char *drivername=0,*device=0;
  int drivernamec=audio_read_form_string(&drivername,form,"Driver",6);
  int devicec=audio_read_form_string(&device,form,"Device",6);
  int rate=audio_read_form_uint(form,"Rate",4);
  int chanc=audio_read_form_uint(form,"Channels",8);
  char zdevice[256];
  if (
    (drivernamec<0)||
    (devicec<0)||(devicec>=sizeof(zdevice))||
    (rate<200)||(rate>200000)||
    (chanc<1)||(chanc>8)
  ) {
    fprintf(stderr,"%s: Form invalid (%d,%d,%d,%d)\n",__func__,drivernamec,devicec,rate,chanc);
    return;
  }
  const struct eh_audio_type *type=eh_audio_type_by_name(drivername,drivernamec);
  if (!type) {
    fprintf(stderr,"%s: No such driver '%.*s'. How did you pick it? Something is broken here.\n",__func__,drivernamec,drivername);
    return;
  }
  memcpy(zdevice,device,devicec);
  zdevice[devicec]=0;
  struct eh_audio_setup setup={
    .device=zdevice,
    .rate=rate,
    .chanc=chanc,
    .format=EH_AUDIO_FORMAT_S16N,
    .buffersize=0, // Emuhost overwrites, at least for now.
  };
  struct eh_audio_driver *driver=eh_audio_reinit(type,&setup);
  if (!driver) return;
  cheapsynth_del(mn.cheapsynth);
  mn.cheapsynth=cheapsynth_new(driver->rate,driver->chanc);
  if (driver->type->play) driver->type->play(driver,1);
}

/* Commit changed values.
 */
 
static void audio_set_driver(struct gui_widget *pickone,int p,void *userdata) {
  struct gui_widget *widget=userdata;
  gui_dismiss_modal(widget->gui,pickone);
  const struct eh_audio_type *type=eh_audio_type_by_index(p);
  if (!type) return;
  MN_SOUND(ACTIVATE)
  gui_widget_button_set_label(gui_widget_form_get_button_by_key(widget->childv[0],"Driver",6),type->name,-1,0xffffff);
  gui_dirty_pack(widget->gui);
  audio_replace_driver(widget);
}
 
static void audio_set_device(struct gui_widget *widget,const char *v,int vc) {
  MN_SOUND(ACTIVATE)
  audio_replace_driver(widget);
}

static void audio_set_rate(struct gui_widget *widget,const char *v,int vc) {
  MN_SOUND(ACTIVATE)
  audio_replace_driver(widget);
}

static void audio_set_chanc(struct gui_widget *widget,const char *v,int vc) {
  MN_SOUND(ACTIVATE)
  audio_replace_driver(widget);
}

/* Launch sub-editors.
 */
 
static void audio_edit_driver(struct gui_widget *widget,struct gui_widget *form) {
  struct eh_audio_driver *driver=eh_get_audio_driver();
  struct gui_widget *pickone=gui_push_modal(widget->gui,&gui_widget_type_pickone);
  if (!pickone) return;
  MN_SOUND(ACTIVATE)
  gui_modal_place_near(pickone,gui_widget_form_get_button_by_key(form,"Driver",6));
  gui_widget_pickone_set_callback(pickone,audio_set_driver,widget);
  int i=0;
  for (;;i++) {
    const struct eh_audio_type *type=eh_audio_type_by_index(i);
    if (!type) break;
    struct gui_widget *option=gui_widget_pickone_add_option(pickone,type->name,-1);
    if (!option) return;
    if (driver&&(type==driver->type)) {
      gui_widget_pickone_focus(pickone,option);
    }
  }
}

/* Callback from form.
 */
 
static void audio_cb_form(struct gui_widget *form,const char *k,int kc,const char *v,int vc,void *userdata) {
  struct gui_widget *widget=userdata;
  
  if (!v&&(vc==-1)) { // Custom value editors.
    if ((kc==6)&&!memcmp(k,"Driver",6)) { audio_edit_driver(widget,form); return; }
    return;
  }
  
  // Text entry. Value is ready.
  if ((kc==6)&&!memcmp(k,"Device",6)) { audio_set_device(widget,v,vc); return; }
  if ((kc==4)&&!memcmp(k,"Rate",4)) { audio_set_rate(widget,v,vc); return; }
  if ((kc==8)&&!memcmp(k,"Channels",8)) { audio_set_chanc(widget,v,vc); return; }
}

/* Init.
 */
 
static int _audio_init(struct gui_widget *widget) {
  struct gui_widget *form=gui_widget_spawn(widget,&gui_widget_type_form);
  if (!form) return -1;
  gui_widget_form_set_callback(form,audio_cb_form,widget);
  struct eh_audio_driver *driver=eh_get_audio_driver();
  if (!driver) return -1;
  
  gui_widget_form_add_string(form,"Driver",6,driver->type->name,-1,1);
  gui_widget_form_add_string(form,"Device",6,"",-1,0);//TODO eh doesn't deal with device names yet -- it needs to
  gui_widget_form_add_int(form,"Rate",4,driver->rate,0); // Integer input would make sense, but meh, strings are easier.
  gui_widget_form_add_int(form,"Channels",8,driver->chanc,0);
  
  return 0;
}

/* Let form control most ops.
 */
 
static void _audio_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  if (widget->childc!=1) return;
  gui_widget_measure(w,h,widget->childv[0],wlimit,hlimit);
}
 
static void _audio_pack(struct gui_widget *widget) {
  if (widget->childc!=1) return;
  struct gui_widget *form=widget->childv[0];
  form->x=0;
  form->y=0;
  form->w=widget->w;
  form->h=widget->h;
  gui_widget_pack(form);
}
 
static void _audio_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x000040ff);
  int i=0;
  for (;i<widget->childc;i++) {
    struct gui_widget *child=widget->childv[i];
    gui_widget_draw(child,x+child->x,y+child->y);
  }
}
 
static void _audio_motion(struct gui_widget *widget,int dx,int dy) {
  if (widget->childc!=1) return;
  struct gui_widget *form=widget->childv[0];
  if (form->type->motion) form->type->motion(form,dx,dy);
}
 
static void _audio_signal(struct gui_widget *widget,int sigid) {
  if (sigid==GUI_SIGID_CANCEL) {
    MN_SOUND(CANCEL)
    gui_dismiss_modal(widget->gui,widget);
  } else {
    if (widget->childc!=1) return;
    struct gui_widget *form=widget->childv[0];
    if (form->type->signal) form->type->signal(form,sigid);
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_audio={
  .name="audio",
  .objlen=sizeof(struct mn_widget_audio),
  .del=_audio_del,
  .init=_audio_init,
  .measure=_audio_measure,
  .pack=_audio_pack,
  .draw=_audio_draw,
  .motion=_audio_motion,
  .signal=_audio_signal,
};
