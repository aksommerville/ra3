/* mn_widget_menubar.
 * Top row of home page.
 * Horizontal list of buttons, mostly for filter criteria.
 */
 
#include "../mn_internal.h"
#include "opt/serial/serial.h"

#define COLOR_NONE 0x808080
#define COLOR_ACTIVE 0xffffff

/* Object definition.
 */
 
struct mn_widget_menubar {
  struct gui_widget hdr;
  int margin; // left,top,right
  int marginb; // bottom only -- might be smaller because we're not dodging overscan here
  int spacing;
  int focusp;
  int focus;
};

#define WIDGET ((struct mn_widget_menubar*)widget)

/* Delete.
 */
 
static void _menubar_del(struct gui_widget *widget) {
}

/* Convert between a loose flags list and the index in our "players" menu.
 * Index: [any,1,2,>=3]
 * Flags: player1 player2 player3 player4 playermore
 */
 
static int menubar_players_index_from_flags(const char *src,int srcc) {
  int player1=0,player2=0,player3=0,player4=0,playermore=0;
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
    #define CHK(tag) if ((tokenc==sizeof(#tag)-1)&&!memcmp(token,#tag,tokenc)) tag=1;
         CHK(player1)
    else CHK(player2)
    else CHK(player3)
    else CHK(player4)
    else CHK(playermore)
    #undef CHK
  }
  if (playermore) return 3;
  if (player4) return 3;
  if (player3) return 3;
  if (player2) return 2;
  if (player1) return 1;
  return 0;
}

static int menubar_rewrite_flags_with_player_index(char *dst,int dsta,const char *src,int srcc,int optionp) {
  int dstc=0,srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
    
    // Strip everything at least 7 characters long starting with "player".
    if ((tokenc>=7)&&!memcmp(token,"player",6)) {
    } else {
      if (dstc) {
        if (dstc<dsta) dst[dstc]=' ';
        dstc++;
      }
      if (dstc<=dsta-tokenc) memcpy(dst+dstc,token,tokenc);
      dstc+=tokenc;
    }
  }
  const char *add=0;
  switch (optionp) {
    case 1: add="player1"; break;
    case 2: add="player2"; break;
    case 3: add="player3"; break; // we'd also be interested in player4 and playermore, but these are AND
  }
  if (add) {
    int addc=0; while (add[addc]) addc++;
    if (dstc) {
      if (dstc<dsta) dst[dstc]=' ';
      dstc++;
    }
    if (dstc<=dsta-addc) memcpy(dst+dstc,add,addc);
    dstc+=addc;
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Replace button's label.
 */
 
static int menubar_set_label(struct gui_widget *widget,int p,const char *v,int c,int color) {
  if ((p<0)||(p>=widget->childc)) return -1;
  
  // A little weird here: First strip leading and trailing space, then truncate if needed, then add a leading and trailing space.
  // Those added spaces act as horizontal padding in the button, so the outline doesn't crowd the text.
  if (!v) c=0; else if (c<0) { c=0; while (v[c]) c++; }
  while (c&&((unsigned char)v[c-1]<=0x20)) c--;
  while (c&&((unsigned char)v[0]<=0x20)) { v++; c--; }
  char norm[16];
  int normc=0;
  if (c>sizeof(norm)-2) {
    norm[0]=' ';
    memcpy(norm+1,v,sizeof(norm)-5);
    memset(norm+sizeof(norm)-4,'.',3);
    norm[sizeof(norm)-1]=' ';
    normc=sizeof(norm);
  } else {
    norm[0]=' ';
    memcpy(norm+1,v,c);
    norm[1+c]=' ';
    normc=2+c;
  }
  
  struct gui_widget *button=widget->childv[p];
  if (gui_widget_button_set_label(button,norm,normc,color)<0) return -1;
  return 0;
}

static int menubar_set_label_ints(struct gui_widget *widget,int p,int lo,int hi) {
  char v[32];
  int c=0;
  if (lo&&hi) c=snprintf(v,sizeof(v),"%d..%d",lo,hi);
  else if (lo) c=snprintf(v,sizeof(v),"%d+",lo);
  else if (hi) c=snprintf(v,sizeof(v),"%d-",hi);
  if ((c<0)||(c>=sizeof(v))) return -1;
  return menubar_set_label(widget,p,v,c,COLOR_ACTIVE);
}

/* Populate a pickone with strings from a JSON array, as the metadata calls return them.
 */
 
static int menubar_populate_pickone_from_json(struct gui_widget *pickone,const char *src,int srcc,const char *select,int selectc) {
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
static int menubar_json_member_by_index(char *dst,int dsta,const char *src,int srcc,int p) {
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

/* Entry-widget callbacks, things that affect the search.
 */
 
static void menubar_cb_choose_list(struct gui_widget *pickone,int p,void *userdata) {
  struct gui_widget *widget=userdata;
  char name[64];
  int namec=menubar_json_member_by_index(name,sizeof(name),mn.dbs.lists,mn.dbs.listsc,p-1);
  if (dbs_search_set_list(&mn.dbs,name,namec)<0) return;
  MN_SOUND(ACTIVATE)
  dbs_refresh_search(&mn.dbs);
  if (namec) menubar_set_label(widget,0,name,namec,COLOR_ACTIVE);
  else menubar_set_label(widget,0,"list",4,COLOR_NONE);
  gui_dismiss_modal(mn.gui,pickone);
  gui_dirty_pack(mn.gui);
}

static void menubar_cb_choose_rating(struct gui_widget *rating,int v,void *userdata) {
  struct gui_widget *widget=userdata;
  MN_SOUND(ACTIVATE)
  if (v!=mn.dbs.ratinglo) {
    dbs_search_set_rating(&mn.dbs,v,0);
    dbs_refresh_search(&mn.dbs);
  }
  if (v) menubar_set_label_ints(widget,1,mn.dbs.ratinglo,mn.dbs.ratinghi);
  else menubar_set_label(widget,1,"rating",6,COLOR_NONE);
  gui_dismiss_modal(mn.gui,rating);
  gui_dirty_pack(mn.gui);
}

static void menubar_cb_choose_playerc(struct gui_widget *pickone,int p,void *userdata) {
  struct gui_widget *widget=userdata;
  char tmp[256];
  int tmpc=menubar_rewrite_flags_with_player_index(tmp,sizeof(tmp),mn.dbs.flags,mn.dbs.flagsc,p);
  if ((tmpc>=0)&&(tmpc<=sizeof(tmp))) {
    MN_SOUND(ACTIVATE)
    if (dbs_search_set_flags(&mn.dbs,tmp,tmpc)<0) return;
    dbs_refresh_search(&mn.dbs);
    switch (p) {
      case 0: menubar_set_label(widget,2,"players",7,COLOR_NONE); break;
      case 1: menubar_set_label(widget,2,"1 Player",8,COLOR_ACTIVE); break;
      case 2: menubar_set_label(widget,2,"2 Players",9,COLOR_ACTIVE); break;
      case 3: menubar_set_label(widget,2,"3+ Players",10,COLOR_ACTIVE); break;
    }
    gui_dismiss_modal(mn.gui,pickone);
    gui_dirty_pack(mn.gui);
  }
}

static void menubar_cb_choose_date(struct gui_widget *daterange,void *userdata,int lo,int hi) {
  struct gui_widget *widget=userdata;
  MN_SOUND(ACTIVATE)
  if (hi>=9999) hi=0;
  mn.dbs.pubtimelo=lo;
  mn.dbs.pubtimehi=hi;
  dbs_refresh_search(&mn.dbs);
  if (lo||hi) menubar_set_label_ints(widget,3,lo,hi);
  else menubar_set_label(widget,3,"date",4,COLOR_NONE);
  gui_dismiss_modal(mn.gui,daterange);
  gui_dirty_pack(mn.gui);
}

static void menubar_cb_choose_genre(struct gui_widget *pickone,int p,void *userdata) {
  struct gui_widget *widget=userdata;
  char name[64];
  int namec=menubar_json_member_by_index(name,sizeof(name),mn.dbs.genres,mn.dbs.genresc,p-1);
  if (dbs_search_set_genre(&mn.dbs,name,namec)<0) return;
  MN_SOUND(ACTIVATE)
  dbs_refresh_search(&mn.dbs);
  if (namec) menubar_set_label(widget,4,name,namec,COLOR_ACTIVE);
  else menubar_set_label(widget,4,"genre",5,COLOR_NONE);
  gui_dismiss_modal(mn.gui,pickone);
  gui_dirty_pack(mn.gui);
}

static void menubar_cb_choose_text(struct gui_widget *entry,const char *v,int c,void *userdata) {
  struct gui_widget *widget=userdata;
  if (dbs_search_set_text(&mn.dbs,v,c)<0) return;
  MN_SOUND(ACTIVATE)
  dbs_refresh_search(&mn.dbs);
  if (c) menubar_set_label(widget,5,v,c,COLOR_ACTIVE);
  else menubar_set_label(widget,5,"text",4,COLOR_NONE);
  gui_dismiss_modal(mn.gui,entry);
  gui_dirty_pack(mn.gui);
}

static void menubar_cb_shutdown(struct gui_widget *confirm,int p,void *userdata) {
  struct gui_widget *widget=userdata;
  gui_dismiss_modal(mn.gui,confirm);
  if (p==1) {
    dbs_request_shutdown(&mn.dbs);
  } else {
    MN_SOUND(CANCEL)
  }
}

static void menubar_cb_choose_settings(struct gui_widget *pickone,int p,void *userdata) {
  struct gui_widget *widget=userdata;
  gui_dismiss_modal(mn.gui,pickone);
  MN_SOUND(ACTIVATE)
  switch (p) {
    //case 0: gui_push_modal(widget->gui,&mn_widget_type_video); break;
    case 0: gui_push_modal(widget->gui,&mn_widget_type_audio); break;
    case 1: gui_push_modal(widget->gui,&mn_widget_type_input); break;
    case 2: gui_push_modal(widget->gui,&mn_widget_type_network); break;
    case 3: gui_push_modal(widget->gui,&mn_widget_type_interface); break;
    case 4: gui_push_modal(widget->gui,&mn_widget_type_addgame); break;
    case 5: {
        mn.kiosk=1;
        mn.rebuild_gui=1;
        //TODO Can we show a toast or something with the unlock instructions?
      } break;
    case 6: {
        struct gui_widget *confirm=gui_push_modal(widget->gui,&gui_widget_type_confirm);
        if (!confirm) return;
        gui_widget_confirm_setup(confirm,"Really shut down?",menubar_cb_shutdown,widget,"Cancel","Shut Down");
      } break;
  }
}

/* Button callbacks.
 * (userdata) is always the menubar widget.
 */
 
static void menubar_cb_list(struct gui_widget *button,void *userdata) {
  struct gui_widget *widget=userdata;
  struct gui_widget *modal=gui_push_modal(widget->gui,&gui_widget_type_pickone);
  if (!modal) return;
  MN_SOUND(ACTIVATE)
  gui_modal_place_near(modal,button);
  gui_widget_pickone_set_callback(modal,menubar_cb_choose_list,widget);
  struct gui_widget *option0=gui_widget_pickone_add_option(modal,"All Games",-1);
  if (!mn.dbs.listc) gui_widget_pickone_focus(modal,option0);
  menubar_populate_pickone_from_json(modal,mn.dbs.lists,mn.dbs.listsc,mn.dbs.list,mn.dbs.listc);
}
 
static void menubar_cb_rating(struct gui_widget *button,void *userdata) {
  struct gui_widget *widget=userdata;
  struct gui_widget *modal=gui_push_modal(widget->gui,&mn_widget_type_rating);
  if (!modal) return;
  MN_SOUND(ACTIVATE)
  gui_modal_place_near(modal,button);
  mn_widget_rating_setup(modal,menubar_cb_choose_rating,widget,mn.dbs.ratinglo);
}
 
static void menubar_cb_players(struct gui_widget *button,void *userdata) {
  struct gui_widget *widget=userdata;
  struct gui_widget *modal=gui_push_modal(widget->gui,&gui_widget_type_pickone);
  if (!modal) return;
  MN_SOUND(ACTIVATE)
  gui_modal_place_near(modal,button);
  gui_widget_pickone_set_callback(modal,menubar_cb_choose_playerc,widget);
  gui_widget_pickone_add_option(modal,"Any Players",-1);
  gui_widget_pickone_add_option(modal,"1 Player",-1);
  gui_widget_pickone_add_option(modal,"2 Players",-1);
  gui_widget_pickone_add_option(modal,"3 or More",-1);
  int optionp=menubar_players_index_from_flags(mn.dbs.flags,mn.dbs.flagsc);
  if ((optionp>=0)&&(optionp<modal->childc)) gui_widget_pickone_focus(modal,modal->childv[optionp]);
}
 
static void menubar_cb_date(struct gui_widget *button,void *userdata) {
  struct gui_widget *widget=userdata;
  struct gui_widget *modal=gui_push_modal(widget->gui,&mn_widget_type_daterange);
  if (!modal) return;
  MN_SOUND(ACTIVATE)
  gui_modal_place_near(modal,button);
  int hivalue=mn.dbs.pubtimehi;
  if (!hivalue) hivalue=9999;
  mn_widget_daterange_setup(
    modal,menubar_cb_choose_date,widget,
    mn.dbs.pubtimelo,hivalue,
    mn.dbs.daterange[0],mn.dbs.daterange[1]
  );
}
 
static void menubar_cb_genre(struct gui_widget *button,void *userdata) {
  struct gui_widget *widget=userdata;
  struct gui_widget *modal=gui_push_modal(widget->gui,&gui_widget_type_pickone);
  if (!modal) return;
  MN_SOUND(ACTIVATE)
  gui_modal_place_near(modal,button);
  gui_widget_pickone_set_callback(modal,menubar_cb_choose_genre,widget);
  struct gui_widget *option0=gui_widget_pickone_add_option(modal,"Any Genre",-1);
  if (!mn.dbs.genrec) gui_widget_pickone_focus(modal,option0);
  menubar_populate_pickone_from_json(modal,mn.dbs.genres,mn.dbs.genresc,mn.dbs.genre,mn.dbs.genrec);
}

static void menubar_cb_text(struct gui_widget *button,void *userdata) {
  struct gui_widget *widget=userdata;
  struct gui_widget *modal=gui_push_modal(widget->gui,&gui_widget_type_entry);
  if (!modal) return;
  MN_SOUND(ACTIVATE)
  gui_modal_place_near(modal,button);
  gui_widget_entry_setup(modal,mn.dbs.text,mn.dbs.textc,menubar_cb_choose_text,widget);
}
 
static void menubar_cb_settings(struct gui_widget *button,void *userdata) {
  struct gui_widget *widget=userdata;
  struct gui_widget *modal=gui_push_modal(widget->gui,&gui_widget_type_pickone);
  if (!modal) return;
  MN_SOUND(ACTIVATE)
  gui_modal_place_near(modal,button);
  gui_widget_pickone_set_callback(modal,menubar_cb_choose_settings,widget);
  struct gui_widget *initial=0;
  
  //initial=gui_widget_pickone_add_option(modal,"Video",5);
  initial=gui_widget_pickone_add_option(modal,"Audio",5);
  gui_widget_pickone_add_option(modal,"Input",5);
  gui_widget_pickone_add_option(modal,"Network",7);
  gui_widget_pickone_add_option(modal,"Interface",9);
  gui_widget_pickone_add_option(modal,"Add Game",8);
  gui_widget_pickone_add_option(modal,"Enter Kiosk",11);
  gui_widget_pickone_add_option(modal,"Shut Down",9);
  
  if (initial) gui_widget_pickone_focus(modal,initial);
}

/* Init.
 */
 
static int _menubar_init(struct gui_widget *widget) {

  WIDGET->margin=25;
  WIDGET->marginb=10;
  WIDGET->spacing=10;
  WIDGET->focusp=0;
  WIDGET->focus=0;
  
  if (!(gui_widget_button_spawn(widget," list ",-1,COLOR_NONE,menubar_cb_list,widget))) return -1;
  if (!(gui_widget_button_spawn(widget," rating ",-1,COLOR_NONE,menubar_cb_rating,widget))) return -1;
  if (!(gui_widget_button_spawn(widget," players ",-1,COLOR_NONE,menubar_cb_players,widget))) return -1;
  if (!(gui_widget_button_spawn(widget," date ",-1,COLOR_NONE,menubar_cb_date,widget))) return -1;
  if (!(gui_widget_button_spawn(widget," genre ",-1,COLOR_NONE,menubar_cb_genre,widget))) return -1;
  if (!(gui_widget_button_spawn(widget," text ",-1,COLOR_NONE,menubar_cb_text,widget))) return -1;
  if (!(gui_widget_button_spawn(widget," Settings ",-1,COLOR_ACTIVE,menubar_cb_settings,widget))) return -1;
  
  if (mn.dbs.listc) menubar_set_label(widget,0,mn.dbs.list,mn.dbs.listc,COLOR_ACTIVE);
  if (mn.dbs.ratinglo||mn.dbs.ratinghi) menubar_set_label_ints(widget,1,mn.dbs.ratinglo,mn.dbs.ratinghi);
  switch (menubar_players_index_from_flags(mn.dbs.flags,mn.dbs.flagsc)) {
    case 1: menubar_set_label(widget,2,"1 Player",8,COLOR_ACTIVE); break;
    case 2: menubar_set_label(widget,2,"2 Players",9,COLOR_ACTIVE); break;
    case 3: menubar_set_label(widget,2,"3+ Players",10,COLOR_ACTIVE); break;
  }
  if (mn.dbs.pubtimelo||mn.dbs.pubtimehi) menubar_set_label_ints(widget,3,mn.dbs.pubtimelo,mn.dbs.pubtimehi);
  if (mn.dbs.genrec) menubar_set_label(widget,4,mn.dbs.genre,mn.dbs.genrec,COLOR_ACTIVE);
  if (mn.dbs.textc) menubar_set_label(widget,5,mn.dbs.text,mn.dbs.textc,COLOR_ACTIVE);

  return 0;
}

/* Measure.
 */
 
static void _menubar_measure(int *w,int *h,struct gui_widget *widget,int maxw,int maxh) {
  *w=0;
  *h=0;
  if (widget->childc>0) {
    int i=widget->childc;
    while (i-->0) {
      struct gui_widget *child=widget->childv[i];
      int chw,chh;
      gui_widget_measure(&chw,&chh,child,maxw,maxh);
      (*w)+=chw;
      if (chh>*h) *h=chh;
    }
    (*w)+=WIDGET->spacing*(widget->childc-1);
  }
  (*w)+=WIDGET->margin<<1;
  (*h)+=WIDGET->margin+WIDGET->marginb;
}

/* Pack.
 */
 
static void _menubar_pack(struct gui_widget *widget) {
  int x=WIDGET->margin;
  int y=WIDGET->margin;
  int hall=widget->h-WIDGET->margin-WIDGET->marginb;
  if (hall<0) hall=0;
  int i=0;
  int last=widget->childc-1;
  for (;i<widget->childc;i++) {
    struct gui_widget *child=widget->childv[i];
    int chw,chh;
    gui_widget_measure(&chw,&chh,child,widget->w-x,widget->h-y);
    child->x=x;
    child->y=y;
    child->w=chw;
    child->h=hall;
    
    // The last child is special, it binds to the right edge.
    if (i==last) {
      child->x=widget->w-WIDGET->margin-chw;
    }
    
    gui_widget_pack(child);
    x+=child->w+WIDGET->spacing;
  }
}

/* Draw.
 */
 
static void _menubar_draw(struct gui_widget *widget,int x,int y) {
  int i=0;
  for (;i<widget->childc;i++) {
    struct gui_widget *child=widget->childv[i];
    gui_widget_draw(child,x+child->x,y+child->y);
  }
}

/* Focus or blur child.
 */
 
static void menubar_blur(struct gui_widget *widget) {
  if (WIDGET->focusp<0) return;
  if (WIDGET->focusp>=widget->childc) return;
  struct gui_widget *focus=widget->childv[WIDGET->focusp];
  if (focus->type->signal) focus->type->signal(focus,GUI_SIGID_BLUR);
}
 
static void menubar_focus(struct gui_widget *widget) {
  if (WIDGET->focusp<0) return;
  if (WIDGET->focusp>=widget->childc) return;
  struct gui_widget *focus=widget->childv[WIDGET->focusp];
  if (focus->type->signal) focus->type->signal(focus,GUI_SIGID_FOCUS);
}

/* Motion.
 */
 
static void _menubar_motion(struct gui_widget *widget,int dx,int dy) {
  if (dx) {
    if (widget->childc>0) {
      int np=WIDGET->focusp+dx;
      if (np<0) np=widget->childc-1;
      else if (np>=widget->childc) np=0;
      if (np!=WIDGET->focusp) {
        menubar_blur(widget);
        WIDGET->focusp=np;
        menubar_focus(widget);
        mn_cb_sound_effect(GUI_SFXID_MOTION,0);
      } else {
        mn_cb_sound_effect(GUI_SFXID_REJECT,0);
      }
    } else {
      mn_cb_sound_effect(GUI_SFXID_REJECT,0);
    }
  }
}

/* Signal.
 */
 
static void _menubar_signal(struct gui_widget *widget,int sigid) {
  if (sigid==GUI_SIGID_FOCUS) {
    WIDGET->focus=1;
    menubar_focus(widget);
  } else if (sigid==GUI_SIGID_BLUR) {
    WIDGET->focus=0;
    menubar_blur(widget);
  } else if ((WIDGET->focusp>=0)&&(WIDGET->focusp<widget->childc)) {
    struct gui_widget *focus=widget->childv[WIDGET->focusp];
    if (focus->type->signal) focus->type->signal(focus,sigid);
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_menubar={
  .name="menubar",
  .objlen=sizeof(struct mn_widget_menubar),
  .del=_menubar_del,
  .init=_menubar_init,
  .measure=_menubar_measure,
  .pack=_menubar_pack,
  .draw=_menubar_draw,
  .motion=_menubar_motion,
  .signal=_menubar_signal,
};
