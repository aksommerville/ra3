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
    case GUI_SIGID_CANCEL: MN_SOUND(CANCEL) gui_dismiss_modal(widget->gui,widget); break;
    default: {
        if (widget->childc<1) return;
        struct gui_widget *form=widget->childv[0];
        if (form->type->signal) form->type->signal(form,sigid);
      } break;
  }
}

/* Populate a pickone with strings from a JSON array, as the metadata calls return them.
 */
 
static int edit_populate_pickone_from_json(struct gui_widget *pickone,const char *src,int srcc,const char *select,int selectc) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_array_start(&decoder)<0) return -1;
  while (sr_decode_json_next(0,&decoder)>0) {
    char v[64];
    int c=sr_decode_json_string(v,sizeof(v),&decoder);
    if ((c<0)||(c>sizeof(v))) {
      if (sr_decode_json_skip(&decoder)<0) return -1;
    } else if (!c) {
      // Empty strings are perfectly normal in an incoming list (eg genre "" means games with genre unset). Normal, but useless to our user IMHO. So skip.
    } else {
      struct gui_widget *button=gui_widget_pickone_add_option(pickone,v,c);
      if (!button) return -1;
      if ((c==selectc)&&!memcmp(v,select,c)) {
        gui_widget_pickone_focus(pickone,button);
      }
    }
  }
  return 0;
}

// Only returns 0..dsta-1; doesn't necessarily terminate. Skips blanks, to keep in sync with menubar_populate_pickone_from_json.
static int edit_json_member_by_index(char *dst,int dsta,const char *src,int srcc,int p) {
  if (p<0) return 0;
  if (dsta<1) return 0;
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_array_start(&decoder)<0) return 0;
  while (sr_decode_json_next(0,&decoder)>0) {
    int c=sr_decode_json_string(dst,dsta,&decoder);
    if ((c<0)||(c>dsta)) {
      if (sr_decode_json_skip(&decoder)<0) return 0;
      c=0;
    } else if (!c) {
      continue; // skip blanks
    }
    if (!p--) return c;
  }
  return 0;
}

/* Summarize comments.
 */
#if 0 // XXX After some difficulty imagining how this UI could work, I've removed comments from the editor. Maybe revisit this later.
// No UI was written for it, beyond this point.
static int edit_summarize_comments(char *dst,int dsta,const struct dbs_game *game) {
  // If there is at least one comment, return the first few bytes of its content.
  int totalc=0;
  struct sr_decoder decoder={.v=game->comments,.c=game->commentsc};
  if (sr_decode_json_array_start(&decoder)>=0) {
    if (sr_decode_json_next(0,&decoder)>0) {
      if (sr_decode_json_object_start(&decoder)>=0) {
        const char *k;
        int kc;
        while ((kc=sr_decode_json_next(&k,&decoder))>0) {
          if ((kc==1)&&(k[0]=='v')) {
            char tmp[1024];
            int tmpc=sr_decode_json_string(tmp,sizeof(tmp),&decoder);
            if ((tmpc>=0)&&(tmpc<=sizeof(tmp))) {
              if (tmpc>dsta) {
                memcpy(dst,tmp,dsta);
                if (dsta>=3) memset(dst+dsta-3,'.',3);
                return dsta;
              } else {
                memcpy(dst,tmp,tmpc);
                return tmpc;
              }
            }
          }
          if (sr_decode_json_skip(&decoder)<0) break;
        }
      }
    }
  }
  return 0; // Otherwise, I think empty is appropriate.
}
#endif

/* Summarize lists.
 */
 
static int edit_summarize_lists(char *dst,int dsta,const struct dbs_game *game) {
  // Join list names with ", " and if we approach the end, add "+N" with the remaining count.
  int dstc=0,addlc=0;
  struct sr_decoder decoder={.v=game->lists,.c=game->listsc};
  if (sr_decode_json_array_start(&decoder)>=0) {
    while (sr_decode_json_next(0,&decoder)>0) {
      if (addlc) { // we've already breached the limit, just count now
        addlc++;
        if (sr_decode_json_skip(&decoder)<0) break;
        continue;
      }
      int subctx=sr_decode_json_object_start(&decoder);
      if (subctx<0) break;
      char name[256];
      int namec=0;
      const char *k;
      int kc,id=0;
      while ((kc=sr_decode_json_next(&k,&decoder))>0) {
        if ((kc==4)&&!memcmp(k,"name",4)) {
          namec=sr_decode_json_string(name,sizeof(name),&decoder);
          if ((namec<0)||(namec>sizeof(name))) break;
        } else if ((kc==6)&&!memcmp(k,"listid",6)) {
          if (sr_decode_json_int(&id,&decoder)<0) break;
        } else {
          if (sr_decode_json_skip(&decoder)<0) break;
        }
      }
      if (sr_decode_json_end(&decoder,subctx)<0) break;
      if (!namec) {
        namec=snprintf(name,sizeof(name),"%d",id);
        if ((namec<1)||(namec>=sizeof(name))) break;
      }
      if (dstc+namec>=dsta-5) { // -5 is arbitrary, leave enough room for ", +N"
        addlc++;
      } else {
        if (dstc) {
          if (dstc<=dsta-2) memcpy(dst+dstc,", ",2);
          dstc+=2;
        }
        if (dstc<=dsta-namec) memcpy(dst+dstc,name,namec);
        dstc+=namec;
      }
    }
  }
  if (addlc) {
    if (dstc<dsta-3) memcpy(dst+dstc,", +",3);
    dstc+=3;
    if (dstc<dsta) {
      int err=snprintf(dst+dstc,dsta-dstc,"%d",addlc);
      dstc+=err;
    }
  }
  return dstc;
}

/* Callbacks when custom editors produce a value.
 */
 
static void edit_cb_author(struct gui_widget *pickone,int p,void *userdata) {
  struct gui_widget *widget=userdata;
  char v[256];
  int vc=p?edit_json_member_by_index(v,sizeof(v),mn.dbs.authors,mn.dbs.authorsc,p-1):0;
  gui_widget_button_set_label(gui_widget_form_get_button_by_key(widget->childv[0],"Author",6),v,vc,0xffffff);
  dbs_replace_game_field_string(&mn.dbs,WIDGET->gameid,"author",6,v,vc);
  gui_dismiss_modal(widget->gui,pickone);
  MN_SOUND(ACTIVATE)
}
 
static void edit_cb_genre(struct gui_widget *pickone,int p,void *userdata) {
  struct gui_widget *widget=userdata;
  char v[256];
  int vc=p?edit_json_member_by_index(v,sizeof(v),mn.dbs.genres,mn.dbs.genresc,p-1):0;
  gui_widget_button_set_label(gui_widget_form_get_button_by_key(widget->childv[0],"Genre",5),v,vc,0xffffff);
  dbs_replace_game_field_string(&mn.dbs,WIDGET->gameid,"genre",5,v,vc);
  gui_dismiss_modal(widget->gui,pickone);
  MN_SOUND(ACTIVATE)
}

static void edit_cb_rating(struct gui_widget *rating,int v,void *userdata) {
  struct gui_widget *widget=userdata;
  char tmp[32];
  int tmpc=snprintf(tmp,sizeof(tmp),"%d",v);
  if ((tmpc<0)||(tmpc>=sizeof(tmp))) tmpc=0;
  gui_widget_button_set_label(gui_widget_form_get_button_by_key(widget->childv[0],"Rating",6),tmp,tmpc,0xffffff);
  dbs_replace_game_field_int(&mn.dbs,WIDGET->gameid,"rating",6,v);
  gui_dismiss_modal(widget->gui,rating);
  MN_SOUND(ACTIVATE)
}

static void edit_cb_pubtime(struct gui_widget *daterange,void *userdata,int v,int dummy) {
  struct gui_widget *widget=userdata;
  char tmp[16];
  int tmpc=snprintf(tmp,sizeof(tmp),"%d",v);
  if ((tmpc<0)||(tmpc>=sizeof(tmp))) tmpc=0;
  gui_widget_button_set_label(gui_widget_form_get_button_by_key(widget->childv[0],"Release Year",12),tmp,tmpc,0xffffff);
  dbs_replace_game_field_int(&mn.dbs,WIDGET->gameid,"pubtime",7,v);
  gui_dismiss_modal(widget->gui,daterange);
  MN_SOUND(ACTIVATE)
}

static void edit_cb_flags(struct gui_widget *flags,const char *v,int c,void *userdata) {
  struct gui_widget *widget=userdata;
  dbs_replace_game_field_string(&mn.dbs,WIDGET->gameid,"flags",5,v,c);
  MN_SOUND(ACTIVATE)
}

static void edit_cb_lists(struct gui_widget *lists,int add,const char *name,int namec,void *userdata) {
  struct gui_widget *widget=userdata;
  if (add<0) {
    dbs_create_list(&mn.dbs,name,namec);
    dbs_add_to_list(&mn.dbs,WIDGET->gameid,name,namec);
  } else if (add) {
    dbs_add_to_list(&mn.dbs,WIDGET->gameid,name,namec);
  } else {
    dbs_remove_from_list(&mn.dbs,WIDGET->gameid,name,namec);
  }
  // Updating the label is a trick...
  char tmp[1024];
  int tmpc=mn_widget_lists_encode(tmp,sizeof(tmp),lists);
  if ((tmpc>=0)&&(tmpc<=sizeof(tmp))) {
    struct dbs_game game={.lists=tmp,.listsc=tmpc};
    char tmper[32];
    int tmperc=edit_summarize_lists(tmper,sizeof(tmper),&game);
    if ((tmperc>=0)&&(tmperc<=sizeof(tmper))) {
      gui_widget_button_set_label(gui_widget_form_get_button_by_key(widget->childv[0],"Lists",5),tmper,tmperc,0xffffff);
    }
  }
}

/* Begin editing a non-plain-text field.
 */
 
static void edit_begin_author(struct gui_widget *widget) {
  struct gui_widget *modal=gui_push_modal(widget->gui,&gui_widget_type_pickone);
  if (!modal) return;
  MN_SOUND(ACTIVATE)
  gui_modal_place_near(modal,gui_widget_form_get_button_by_key(widget->childv[0],"Author",6));
  gui_widget_pickone_set_callback(modal,edit_cb_author,widget);
  struct gui_widget *option0=gui_widget_pickone_add_option(modal,"",0);
  struct dbs_game game={0};
  dbs_game_get(&game,&mn.dbs,WIDGET->gameid);
  char v[256];
  int vc=sr_string_eval(v,sizeof(v),game.author,game.authorc);
  if ((vc<0)||(vc>sizeof(v))) vc=0;
  if (!vc) gui_widget_pickone_focus(modal,option0);
  edit_populate_pickone_from_json(modal,mn.dbs.authors,mn.dbs.authorsc,v,vc);
}
 
static void edit_begin_genre(struct gui_widget *widget) {
  struct gui_widget *modal=gui_push_modal(widget->gui,&gui_widget_type_pickone);
  if (!modal) return;
  MN_SOUND(ACTIVATE)
  gui_modal_place_near(modal,gui_widget_form_get_button_by_key(widget->childv[0],"Genre",5));
  gui_widget_pickone_set_callback(modal,edit_cb_genre,widget);
  struct gui_widget *option0=gui_widget_pickone_add_option(modal,"",0);
  struct dbs_game game={0};
  dbs_game_get(&game,&mn.dbs,WIDGET->gameid);
  char v[256];
  int vc=sr_string_eval(v,sizeof(v),game.genre,game.genrec);
  if ((vc<0)||(vc>sizeof(v))) vc=0;
  if (!vc) gui_widget_pickone_focus(modal,option0);
  edit_populate_pickone_from_json(modal,mn.dbs.genres,mn.dbs.genresc,v,vc);
}
 
static void edit_begin_rating(struct gui_widget *widget) {
  struct gui_widget *modal=gui_push_modal(widget->gui,&mn_widget_type_rating);
  if (!modal) return;
  MN_SOUND(ACTIVATE)
  gui_modal_place_near(modal,gui_widget_form_get_button_by_key(widget->childv[0],"Rating",6));
  struct dbs_game game={0};
  dbs_game_get(&game,&mn.dbs,WIDGET->gameid);
  mn_widget_rating_setup(modal,edit_cb_rating,widget,game.rating);
}
 
static void edit_begin_pubtime(struct gui_widget *widget) {
  struct gui_widget *modal=gui_push_modal(widget->gui,&mn_widget_type_daterange);
  if (!modal) return;
  MN_SOUND(ACTIVATE)
  gui_modal_place_near(modal,gui_widget_form_get_button_by_key(widget->childv[0],"Release Year",12));
  struct dbs_game game={0};
  dbs_game_get(&game,&mn.dbs,WIDGET->gameid);
  mn_widget_daterange_setup_single(modal,edit_cb_pubtime,widget,game.pubtime);
}

static void edit_begin_lists(struct gui_widget *widget) {
  struct dbs_game game={0};
  dbs_game_get(&game,&mn.dbs,WIDGET->gameid);
  struct gui_widget *modal=gui_push_modal(widget->gui,&mn_widget_type_lists);
  if (!modal) return;
  MN_SOUND(ACTIVATE)
  mn_widget_lists_setup(modal,game.lists,game.listsc,mn.dbs.lists,mn.dbs.listsc,edit_cb_lists,widget);
}

/* From form: Text field edited.
 */

static void edit_cb_text(struct gui_widget *form,const char *k,int kc,const char *v,int vc,void *userdata) {
  struct gui_widget *widget=userdata;
  
  if (!v&&(vc<0)) { // begin edit, for non-plain-text fields that we indicated at setup
    if ((kc==6)&&!memcmp(k,"Author",6)) { edit_begin_author(widget); return; }
    if ((kc==5)&&!memcmp(k,"Genre",5)) { edit_begin_genre(widget); return; }
    if ((kc==6)&&!memcmp(k,"Rating",6)) { edit_begin_rating(widget); return; }
    if ((kc==12)&&!memcmp(k,"Release Year",12)) { edit_begin_pubtime(widget); return; }
    if ((kc==5)&&!memcmp(k,"Lists",5)) { edit_begin_lists(widget); return; }
    fprintf(stderr,"TODO:%s: Edit field '%.*s'\n",__func__,kc,k);
    return;
  }
  
  // Heh, turns out there's only one "standard" field.
  if ((kc==4)&&!memcmp(k,"Name",4)) {
    MN_SOUND(ACTIVATE)
    dbs_replace_game_field_string(&mn.dbs,WIDGET->gameid,"name",4,v,vc);
    return;
  }
  
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
  gui_widget_form_add_string_json(form,"Name",4,game.name,game.namec,0);
  gui_widget_form_add_int(form,"Rating",6,game.rating,1);
  gui_widget_form_add_int(form,"Release Year",12,game.pubtime,1);
  
  /* For author and genre, plain text is the rightest thing for the data model.
   * But that's inconvenient to the user.
   * So we only allow them to operate as pickones, from existing values elsewhere in the DB.
   * That would definitely not fly as the only option.
   * But I think menu will only be used for data touch-up, not primary data editing.
   */
  gui_widget_form_add_string_json(form,"Author",6,game.author,game.authorc,1);
  gui_widget_form_add_string_json(form,"Genre",5,game.genre,game.genrec,1);
  
  /* Flags has custom inline UI, it's the only one that does.
   */
  struct gui_widget *flags=gui_widget_form_add_custom(form,"Flags",5,&mn_widget_type_flags);
  if (mn_widget_flags_setup(flags,edit_cb_flags,widget,game.flags,game.flagsc)<0) return -1;
  
  /* Lists: Big custom modals on actuation, and a custom summary label.
   */
  char summary[32];
  int summaryc;
  //summaryc=edit_summarize_comments(summary,sizeof(summary),&game); gui_widget_form_add_string(form,"Comments",8,summary,summaryc,1);
  summaryc=edit_summarize_lists(summary,sizeof(summary),&game); gui_widget_form_add_string(form,"Lists",5,summary,summaryc,1);
  
  // Plays and blobs, I think no reason to display these.
  // Same with comments, though I'm uneasy about it. User should want to edit this data. Just, it's really inconvenient without a keyboard.

  return 0;
}
