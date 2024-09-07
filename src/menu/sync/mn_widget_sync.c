#include "sync_internal.h"

static int sync_begin_HOST(struct gui_widget *widget);
static int sync_begin_SETUP(struct gui_widget *widget);
static int sync_begin_RUN(struct gui_widget *widget);
static int sync_begin_DONE(struct gui_widget *widget);
static int sync_begin_FAIL(struct gui_widget *widget);

/* Widget instance definition.
 */
 
struct mn_widget_sync {
  struct gui_widget hdr;
  int stage;
  struct gui_texture *headline;
  int headlinex,headliney;
  struct syncsrv syncsrv;
  int *editing_field; // Points to some int field in syncsrv that we're editing.
  int editing_bias;
};

#define WIDGET ((struct mn_widget_sync*)widget)

/* Delete.
 */
 
static void _sync_del(struct gui_widget *widget) {
  gui_texture_del(WIDGET->headline);
  syncsrv_cleanup(&WIDGET->syncsrv);
}

/* Replace headline text.
 */
 
static int sync_set_headline(struct gui_widget *widget,const char *src) {
  gui_texture_del(WIDGET->headline);
  if (src&&src[0]) {
    if (!(WIDGET->headline=gui_texture_from_text(widget->gui,0,src,-1,0xffffff))) return -1;
    WIDGET->headliney=30;
    struct gui_widget *root=gui_get_root(widget->gui);
    if (root) {
      int texw=0,texh=0;
      gui_texture_get_size(&texw,&texh,WIDGET->headline);
      WIDGET->headlinex=(root->w>>1)-(texw>>1);
    } else {
      WIDGET->headlinex=30;
    }
  } else {
    WIDGET->headline=0;
  }
  return 0;
}

/* Callback from HOST form.
 */
 
static void sync_cb_host(struct gui_widget *form,const char *k,int kc,const char *v,int vc,void *userdata) {
  struct gui_widget *widget=userdata;
  if ((kc==4)&&!memcmp(k,"Host",4)) {
    syncsrv_set_remote_host(&WIDGET->syncsrv,v,vc);
    return;
  }
  if ((kc==4)&&!memcmp(k,"Port",4)) {
    int port=0;
    if ((sr_int_eval(&port,v,vc)>=2)&&(port>0)&&(port<=0xffff)) {
      syncsrv_set_remote_port(&WIDGET->syncsrv,port);
    }
    return;
  }
  if ((kc==4)&&!memcmp(k,"Next",4)) {
    sync_begin_SETUP(widget);
    return;
  }
}

/* Replace all labels in the SETUP form.
 */
 
static void sync_setup_replace_labels(struct gui_widget *widget) {
  if (widget->childc<1) return;
  struct gui_widget *form=widget->childv[0];
  #define SETONE(k,v) { \
    struct gui_widget *btn=gui_widget_form_get_button_by_key(form,k,sizeof(k)-1); \
    gui_widget_button_set_label(btn,v,-1,0xffffff); \
  }
  SETONE("Access",sync_access_repr(WIDGET->syncsrv.access))
  SETONE("Add games",sync_no_ask_yes_repr(WIDGET->syncsrv.add_games))
  SETONE("Delete games",sync_no_ask_yes_repr(WIDGET->syncsrv.delete_games))
  SETONE("Add screencaps",sync_no_ask_yes_repr(WIDGET->syncsrv.add_screencaps))
  SETONE("Add plays",sync_no_ask_yes_repr(WIDGET->syncsrv.add_plays))
  SETONE("Add launchers",sync_no_ask_yes_repr(WIDGET->syncsrv.add_launchers))
  #undef SETONE
}

/* Callback from SETUP pickones.
 */
 
static void sync_cb_setup_pickone(struct gui_widget *pickone,int p,void *userdata) {
  struct gui_widget *widget=userdata;
  if (!WIDGET->editing_field) return;
  *(WIDGET->editing_field)=p+WIDGET->editing_bias;
  gui_dismiss_modal(pickone->gui,pickone);
  sync_setup_replace_labels(widget);
}

/* Callback from SETUP form.
 */
 
static void sync_cb_setup(struct gui_widget *form,const char *k,int kc,const char *v,int vc,void *userdata) {
  struct gui_widget *widget=userdata;
  const char **popops=0;
  int popopc=0;
  const char *no_yes_ask[]={"No","Ask","Yes"};
  const char *access_options[]={"Read Only","Write Only","Read/Write"};
  
  if ((kc==6)&&!memcmp(k,"Access",6)) {
    popops=access_options;
    popopc=3;
    WIDGET->editing_field=&WIDGET->syncsrv.access;
    WIDGET->editing_bias=0;
  } else if ((kc==9)&&!memcmp(k,"Add games",9)) {
    popops=no_yes_ask;
    popopc=3;
    WIDGET->editing_field=&WIDGET->syncsrv.add_games;
    WIDGET->editing_bias=-1;
  } else if ((kc==12)&&!memcmp(k,"Delete games",12)) {
    popops=no_yes_ask;
    popopc=3;
    WIDGET->editing_field=&WIDGET->syncsrv.delete_games;
    WIDGET->editing_bias=-1;
  } else if ((kc==14)&&!memcmp(k,"Add screencaps",14)) {
    popops=no_yes_ask;
    popopc=3;
    WIDGET->editing_field=&WIDGET->syncsrv.add_screencaps;
    WIDGET->editing_bias=-1;
  } else if ((kc==9)&&!memcmp(k,"Add plays",9)) {
    popops=no_yes_ask;
    popopc=3;
    WIDGET->editing_field=&WIDGET->syncsrv.add_plays;
    WIDGET->editing_bias=-1;
  } else if ((kc==13)&&!memcmp(k,"Add launchers",13)) {
    popops=no_yes_ask;
    popopc=3;
    WIDGET->editing_field=&WIDGET->syncsrv.add_launchers;
    WIDGET->editing_bias=-1;
  }
  if (popopc>0) {
    struct gui_widget *pickone=gui_push_modal(widget->gui,&gui_widget_type_pickone);
    if (!pickone) return;
    int i=0; for (;i<popopc;i++) {
      gui_widget_pickone_add_option(pickone,popops[i],-1);
    }
    gui_modal_place_near(pickone,gui_widget_form_get_button_by_key(form,k,kc));
    gui_widget_pickone_set_callback(pickone,sync_cb_setup_pickone,widget);
    return;
  }
  
  if ((kc==4)&&!memcmp(k,"Next",4)) {
    sync_begin_RUN(widget);
    return;
  }
}

/* Enter HOST stage.
 */
 
static int sync_begin_HOST(struct gui_widget *widget) {
  WIDGET->stage=SYNC_STAGE_HOST;
  if (sync_set_headline(widget,"Sync: Select remote host.")<0) return -1;
  gui_widget_remove_all_children(widget);
  
  struct gui_widget *form=gui_widget_spawn(widget,&gui_widget_type_form);
  if (!form) return -1;
  gui_widget_form_add_string(form,"Host",4,syncsrv_get_remote_host(&WIDGET->syncsrv),-1,0);
  gui_widget_form_add_int(form,"Port",4,syncsrv_get_remote_port(&WIDGET->syncsrv),0);
  gui_widget_form_add_string(form,"Next",4,"OK",2,1);
  gui_widget_form_set_callback(form,sync_cb_host,widget);
  
  gui_dirty_pack(widget->gui);
  if (syncsrv_collect_local(&WIDGET->syncsrv)<0) return -1;
  return 0;
}

/* Enter SETUP stage.
 */
 
static int sync_begin_SETUP(struct gui_widget *widget) {
  WIDGET->stage=SYNC_STAGE_SETUP;
  if (sync_set_headline(widget,"Sync: Set parameters.")<0) return -1;
  gui_widget_remove_all_children(widget);
  
  struct gui_widget *form=gui_widget_spawn(widget,&gui_widget_type_form);
  if (!form) return -1;
  gui_widget_form_add_string(form,"Access",6,"",0,1);
  gui_widget_form_add_string(form,"Add games",9,"",0,1);
  gui_widget_form_add_string(form,"Delete games",12,"",0,1);
  gui_widget_form_add_string(form,"Add screencaps",14,"",0,1);
  gui_widget_form_add_string(form,"Add plays",9,"",0,1);
  gui_widget_form_add_string(form,"Add launchers",13,"",0,1);
  gui_widget_form_add_string(form,"Next",4,"OK",2,1);
  gui_widget_form_set_callback(form,sync_cb_setup,widget);
  sync_setup_replace_labels(widget);
  
  gui_dirty_pack(widget->gui);
  if (syncsrv_connect_remote(&WIDGET->syncsrv)<0) return -1;
  if (syncsrv_collect_remote(&WIDGET->syncsrv)<0) return -1;
  return 0;
}

/* Enter RUN stage.
 */
 
static int sync_begin_RUN(struct gui_widget *widget) {
  WIDGET->stage=SYNC_STAGE_RUN;
  if (sync_set_headline(widget,"Sync: Running...")<0) return -1;
  gui_widget_remove_all_children(widget);
  gui_dirty_pack(widget->gui);
  //TODO Start doing things.
  //TODO Progress indicators.
  //TODO Interrupt where input required.
  return 0;
}

/* Enter DONE stage.
 */
 
static int sync_begin_DONE(struct gui_widget *widget) {
  WIDGET->stage=SYNC_STAGE_DONE;
  if (sync_set_headline(widget,"Sync: Complete.")<0) return -1;
  gui_widget_remove_all_children(widget);
  gui_dirty_pack(widget->gui);
  //TODO Report.
  return 0;
}

/* Enter FAIL stage.
 */
 
static int sync_begin_FAIL(struct gui_widget *widget) {
  WIDGET->stage=SYNC_STAGE_FAIL;
  if (sync_set_headline(widget,"Sync: Error.")<0) return -1;
  gui_widget_remove_all_children(widget);
  gui_dirty_pack(widget->gui);
  //TODO Report.
  return 0;
}

/* Init.
 */
 
static int _sync_init(struct gui_widget *widget) {
  widget->opaque=1;
  if (syncsrv_init(&WIDGET->syncsrv)<0) return -1;
  if (sync_begin_HOST(widget)<0) return -1;
  return 0;
}

/* Measure.
 * Nothing to do here; we want the whole screen.
 */
 
static void _sync_measure(int *w,int *h,struct gui_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
}

/* Pack.
 * All children pack in one column, centered.
 * Account for the headline if present.
 */
 
static void _sync_pack(struct gui_widget *widget) {
  if (widget->childc<1) return;
  int x0=0,y0=0,w=widget->w,h=widget->h;
  if (WIDGET->headline) {
    int hw=0,hh=0;
    gui_texture_get_size(&hw,&hh,WIDGET->headline);
    y0=WIDGET->headliney+hh;
    h-=y0+WIDGET->headliney;
  }
  int spacing=10;
  int combinedh=0,i;
  for (i=widget->childc;i-->0;) {
    struct gui_widget *child=widget->childv[i];
    int cw=0,ch=0;
    gui_widget_measure(&cw,&ch,child,widget->w,h-combinedh);
    combinedh+=ch;
    child->w=cw;
    child->x=x0+(w>>1)-(cw>>1);
    child->h=ch;
  }
  combinedh+=(widget->childc-1)*spacing;
  int y=y0+(h>>1)-(combinedh>>1);
  for (i=0;i<widget->childc;i++) {
    struct gui_widget *child=widget->childv[i];
    child->y=y;
    gui_widget_pack(child);
    y+=child->h;
    y+=spacing;
  }
}

/* Draw.
 */
 
static void _sync_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_gradient(widget->gui,x,y,widget->w,widget->h,0x201040ff,0x180c30ff,0x180c30ff,0x100820ff);
  int i=0;
  for (;i<widget->childc;i++) {
    struct gui_widget *child=widget->childv[i];
    gui_widget_draw(child,x+child->x,y+child->y);
  }
  if (WIDGET->headline) {
    gui_draw_texture(widget->gui,x+WIDGET->headlinex,y+WIDGET->headliney,WIDGET->headline);
  }
}

/* Update.
 */
 
static void _sync_update(struct gui_widget *widget) {
  if (WIDGET->stage==SYNC_STAGE_FAIL) {
  } else if (WIDGET->stage==SYNC_STAGE_DONE) {
  } else {
    if (syncsrv_update(&WIDGET->syncsrv)<0) {
      sync_begin_FAIL(widget);
    }
  }
}

/* Input.
 */
 
static void _sync_input_changed(struct gui_widget *widget,uint16_t state) {
  fprintf(stderr,"%s %#.4x\n",__func__,state);
}

/* Motion.
 */
 
static void _sync_motion(struct gui_widget *widget,int dx,int dy) {
  if (widget->childc>0) {
    struct gui_widget *child=widget->childv[0];
    if (child->type->motion) child->type->motion(child,dx,dy);
  }
}

/* Signal.
 */
 
static void _sync_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_CANCEL: gui_dismiss_modal(widget->gui,widget); return;
    case GUI_SIGID_REMOVE: return;
    case GUI_SIGID_AUX: switch (WIDGET->stage) {
        case SYNC_STAGE_HOST: sync_cb_host(0,"Next",4,0,-1,widget); return;
        case SYNC_STAGE_SETUP: return;//TODO
        case SYNC_STAGE_RUN: return;
        case SYNC_STAGE_DONE: gui_dismiss_modal(widget->gui,widget); return;
        case SYNC_STAGE_FAIL: gui_dismiss_modal(widget->gui,widget); return;
      } break;
  }
  if (widget->childc>0) {
    struct gui_widget *child=widget->childv[0];
    if (child->type->signal) child->type->signal(child,sigid);
  }
}

/* Widget type definition.
 */
 
const struct gui_widget_type mn_widget_type_sync={
  .name="sync",
  .objlen=sizeof(struct mn_widget_sync),
  .del=_sync_del,
  .init=_sync_init,
  .measure=_sync_measure,
  .pack=_sync_pack,
  .draw=_sync_draw,
  .update=_sync_update,
  .input_changed=_sync_input_changed,
  .motion=_sync_motion,
  .signal=_sync_signal,
};
