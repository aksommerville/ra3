#include "../mn_internal.h"
#include "opt/serial/serial.h"

static void edit_cb_text(struct gui_widget *form,const char *k,int kc,const char *v,int vc,void *userdata);

/* Object definition.
 */
 
struct mn_widget_edit {
  struct gui_widget hdr;
  int gameid;
};

#define WIDGET ((struct mn_widget_edit*)widget)

/* Cleanup.
 */
 
static void _edit_del(struct gui_widget *widget) {
}

/* Init.
 */
 
static int _edit_init(struct gui_widget *widget) {

  struct gui_widget *form=gui_widget_spawn(widget,&gui_widget_type_form);
  if (!form) return -1;
  gui_widget_form_set_callback(form,edit_cb_text,widget);
  // Building the form is deferred to setup, so we have the game details in hand.

  return 0;
}

/* Measure, pack, draw, motion: Defer to form.
 */
 
static void _edit_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  if (widget->childc<1) return;
  struct gui_widget *form=widget->childv[0];
  gui_widget_measure(w,h,form,wlimit,hlimit);
}
 
static void _edit_pack(struct gui_widget *widget) {
  if (widget->childc<1) return;
  struct gui_widget *form=widget->childv[0];
  form->x=0;
  form->y=0;
  form->w=widget->w;
  form->h=widget->h;
  gui_widget_pack(form);
}
 
static void _edit_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x304030ff);
  int i=0;
  for (;i<widget->childc;i++) {
    struct gui_widget *child=widget->childv[i];
    gui_widget_draw(child,x+child->x,y+child->y);
  }
}
 
static void _edit_motion(struct gui_widget *widget,int dx,int dy) {
  if (widget->childc<1) return;
  struct gui_widget *form=widget->childv[0];
  if (form->type->motion) form->type->motion(form,dx,dy);
}

/* Signal.
 */
 
static void _edit_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_CANCEL: gui_dismiss_modal(widget->gui,widget); break;
    default: {
        if (widget->childc<1) return;
        struct gui_widget *form=widget->childv[0];
        if (form->type->signal) form->type->signal(form,sigid);
      } break;
  }
}

/* From form: Text field edited.
 */

static void edit_cb_text(struct gui_widget *form,const char *k,int kc,const char *v,int vc,void *userdata) {
  struct gui_widget *widget=userdata;
  fprintf(stderr,"%s '%.*s' = '%.*s'\n",__func__,kc,k,vc,v);
  if ((kc==4)&&!memcmp(k,"Name",4)) { dbs_replace_game_field_string(&mn.dbs,WIDGET->gameid,"name",4,v,vc); return; }
  if ((kc==6)&&!memcmp(k,"Author",6)) { dbs_replace_game_field_string(&mn.dbs,WIDGET->gameid,"author",6,v,vc); return; }
  if ((kc==5)&&!memcmp(k,"Genre",5)) { dbs_replace_game_field_string(&mn.dbs,WIDGET->gameid,"genre",5,v,vc); return; }
  if ((kc==6)&&!memcmp(k,"Rating",6)) { dbs_replace_game_field_string(&mn.dbs,WIDGET->gameid,"rating",6,v,vc); return; }//TODO shouldn't be string
  if ((kc==12)&&!memcmp(k,"Release Year",12)) { dbs_replace_game_field_string(&mn.dbs,WIDGET->gameid,"pubtime",7,v,vc); return; }//TODO shouldn't be string
  //TODO flags
  //TODO comments
  //TODO plays
  //TODO blobs
  fprintf(stderr,"!!! %s: Unexpected field: '%.*s' = '%.*s'\n",__func__,kc,k,vc,v);
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_edit={
  .name="edit",
  .objlen=sizeof(struct mn_widget_edit),
  .del=_edit_del,
  .init=_edit_init,
  .measure=_edit_measure,
  .pack=_edit_pack,
  .draw=_edit_draw,
  .motion=_edit_motion,
  .signal=_edit_signal,
};

/* Setup.
 */

int mn_widget_edit_setup(
  struct gui_widget *widget,
  int gameid
) {
  if (!widget||(widget->type!=&mn_widget_type_edit)) return -1;
  if (widget->childc<1) return -1;
  struct gui_widget *form=widget->childv[0];
  WIDGET->gameid=gameid;
  
  struct dbs_game game={0};
  if (dbs_game_get(&game,&mn.dbs,gameid)<0) return -1;
  
  gui_widget_form_add_readonly_int(form,"ID",2,gameid);
  gui_widget_form_add_readonly_string_json(form,"Path",4,game.path,game.pathc);
  gui_widget_form_add_readonly_string_json(form,"Platform",8,game.platform,game.platformc);
  gui_widget_form_add_string_json(form,"Name",4,game.name,game.namec);
  gui_widget_form_add_string_json(form,"Author",6,game.author,game.authorc);
  gui_widget_form_add_string_json(form,"Genre",5,game.genre,game.genrec);
  gui_widget_form_add_int(form,"Rating",6,game.rating);
  gui_widget_form_add_int(form,"Release Year",12,game.pubtime);
  gui_widget_form_add_readonly_string(form,"Flags",5,"TODO",4);
  gui_widget_form_add_readonly_string(form,"Comments",8,"TODO",4);
  gui_widget_form_add_readonly_string(form,"Lists",5,"TODO",4);
  gui_widget_form_add_readonly_string(form,"Plays",5,"TODO?",5);
  gui_widget_form_add_readonly_string(form,"Blobs",5,"TODO?",5);

  return 0;
}
