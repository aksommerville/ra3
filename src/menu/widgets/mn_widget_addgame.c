/* mn_widget_addgame.c
 * From Settings menu, for picking one game off the filesystem (presumably from a USB drive) and importing it to Romassist.
 * Operational logic all lives on the backend, we invoke it by PUT /api/game/file, same as the web app.
 */
 
#include "../mn_internal.h"
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"
#include "opt/gui/gui_font.h"
#include <stdarg.h>

#define ADDGAME_VISIBLE_OPTION_COUNT 10
#define ADDGAME_MINIMUM_WIDTH 400

/* Object definition.
 */
 
struct mn_widget_addgame {
  struct gui_widget hdr;
  struct sr_encoder path;
  char **basev;
  int basec,basea;
  struct gui_texture *pathtex;
  struct addgame_option { // Changes as you scroll, visible ones only.
    struct gui_texture *texture;
  } optionv[ADDGAME_VISIBLE_OPTION_COUNT];
  int scroll; // Index in (basev) of the first displayed option.
  int focusp; // '' the one selected. (NB this is in basev, not optionv)
  struct gui_font *font;
  int marginx,marginy;
  int http_corrid;
};

#define WIDGET ((struct mn_widget_addgame*)widget)

/* Cleanup.
 */
 
static void addgame_basev_clear(struct gui_widget *widget) {
  while (WIDGET->basec>0) {
    WIDGET->basec--;
    free(WIDGET->basev[WIDGET->basec]);
  }
}

static void addgame_optionv_clear(struct gui_widget *widget) {
  struct addgame_option *option=WIDGET->optionv;
  int i=ADDGAME_VISIBLE_OPTION_COUNT;
  for (;i-->0;option++) {
    gui_texture_del(option->texture);
    option->texture=0;
  }
}
 
static void _addgame_del(struct gui_widget *widget) {
  if (WIDGET->http_corrid) {
    dbs_cancel_request(&mn.dbs,WIDGET->http_corrid);
    WIDGET->http_corrid=0;
  }
  sr_encoder_cleanup(&WIDGET->path);
  if (WIDGET->basev) {
    addgame_basev_clear(widget);
    free(WIDGET->basev);
  }
  gui_texture_del(WIDGET->pathtex);
  addgame_optionv_clear(widget);
}

/* Rebuild basev. Only addgame_set_wdf() should do this.
 */
 
static int addgame_basev_append(struct gui_widget *widget,const char *src) {
  if (WIDGET->basec>=WIDGET->basea) {
    int na=WIDGET->basea+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(WIDGET->basev,sizeof(void*)*na);
    if (!nv) return -1;
    WIDGET->basev=nv;
    WIDGET->basea=na;
  }
  if (!(WIDGET->basev[WIDGET->basec]=strdup(src))) return -1;
  WIDGET->basec++;
  return 0;
}
 
static int addgame_rebuild_basev_cb(const char *path,const char *base,char type,void *userdata) {
  struct gui_widget *widget=userdata;
  if (!base[0]||(base[0]=='.')) return 0;
  if (type=='d') {
    // Append a slash to directories.
    char slashed[64];
    int basec=0; while (base[basec]) basec++;
    if (basec>=sizeof(slashed)-1) return 0;
    memcpy(slashed,base,basec);
    slashed[basec]='/';
    slashed[basec+1]=0;
    if (addgame_basev_append(widget,slashed)<0) return -1;
  } else {
    if (addgame_basev_append(widget,base)<0) return -1;
  }
  return 0;
}

static int addgame_basev_cmp(const void *A,const void *B) {
  const char *const*a=A,*const*b=B;
  return strcasecmp(*a,*b);
}
 
static int addgame_rebuild_basev(struct gui_widget *widget) {
  addgame_basev_clear(widget);
  if (WIDGET->path.c>1) { // No "../" option when you're at root.
    if (addgame_basev_append(widget,"../")<0) return -1;
  }
  if (dir_read(WIDGET->path.v,addgame_rebuild_basev_cb,widget)<0) return -1;
  qsort(WIDGET->basev,WIDGET->basec,sizeof(void*),addgame_basev_cmp);
  return 0;
}

/* Rebuild (optionv) from scratch, for the current (basev,scroll).
 */
 
static int addgame_rebuild_optionv(struct gui_widget *widget) {
  int optionp=0,basep=WIDGET->scroll;
  struct addgame_option *option=WIDGET->optionv;
  for (;optionp<ADDGAME_VISIBLE_OPTION_COUNT;optionp++,basep++,option++) {
    gui_texture_del(option->texture);
    option->texture=0;
    if (basep>=WIDGET->basec) continue;
    if (!(option->texture=gui_texture_from_text(widget->gui,WIDGET->font,WIDGET->basev[basep],-1,0xc0c0c0))) return -1;
  }
  return 0;
}

/* Set (path), terminate it, and validate that it exists.
 * Not existing, we clear (path) and report an error.
 * If it does exist, we rebuild basev.
 */
 
static int addgame_chdir_pre(struct gui_widget *widget) {
  addgame_basev_clear(widget);
  addgame_optionv_clear(widget);
  WIDGET->path.c=0;
  WIDGET->scroll=0;
  WIDGET->focusp=0;
  return 0;
}

static int addgame_chdir_post(struct gui_widget *widget) {
  if (file_get_type(WIDGET->path.v)!='d') {
    WIDGET->path.c=0;
    WIDGET->path.v[0]=0;
    return -1;
  }
  if (addgame_rebuild_basev(widget)<0) return -1;
  if (addgame_rebuild_optionv(widget)<0) return -1;
  gui_texture_del(WIDGET->pathtex);
  if (!(WIDGET->pathtex=gui_texture_from_text(widget->gui,WIDGET->font,WIDGET->path.v,WIDGET->path.c,0xffffff))) return -1;
  gui_dirty_pack(widget->gui);
  return 0;
}
 
static int addgame_set_wdf(struct gui_widget *widget,const char *fmt,...) {
  if (addgame_chdir_pre(widget)<0) return -1;
  // serial.h gives us sr_encode_fmt but not one that takes a va_list.
  // It doesn't want to either; that would require every serial consumer to get stdarg.h.
  while (1) {
    va_list vargs;
    va_start(vargs,fmt);
    int err=vsnprintf(WIDGET->path.v,WIDGET->path.a,fmt,vargs);
    if ((err<0)||(err>=INT_MAX)) return -1;
    if (err<WIDGET->path.a) {
      WIDGET->path.c=err;
      WIDGET->path.v[err]=0;
      break;
    }
    if (sr_encoder_require(&WIDGET->path,err+1)<0) return -1;
  }
  return addgame_chdir_post(widget);
}

static int addgame_set_wd(struct gui_widget *widget,const char *src,int srcc) {
  if (addgame_chdir_pre(widget)<0) return -1;
  if (sr_encode_raw(&WIDGET->path,src,srcc)<0) return -1;
  if (sr_encoder_terminate(&WIDGET->path)<0) return -1;
  return addgame_chdir_post(widget);
}

/* Set initial working directory.
 */
 
static int addgame_set_initial_wd(struct gui_widget *widget) {
  int err;
  const char *USER=getenv("USER");
  if (USER&&USER[0]) {
    // Prefer "/media/$USER" if it exists.
    if (addgame_set_wdf(widget,"/media/%s",USER)>=0) return 0;
    // "/home/$USER" also a sensible choice, though probably not as convenient for our purposes.
    if (addgame_set_wdf(widget,"/home/%s",USER)>=0) return 0;
  }
  if (addgame_set_wdf(widget,"/")>=0) return 0;
  return -1;
}

/* Init.
 */
 
static int _addgame_init(struct gui_widget *widget) {
  WIDGET->marginx=10;
  WIDGET->marginy=10;
  if (!(WIDGET->font=gui_font_get_default(widget->gui))) return -1;
  if (addgame_set_initial_wd(widget)<0) return -1;
  return 0;
}

/* Measure.
 */
 
static void _addgame_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  *w=ADDGAME_MINIMUM_WIDTH;
  *h=0;
  int tw,th;
  if (WIDGET->pathtex) {
    gui_texture_get_size(&tw,&th,WIDGET->pathtex);
    if (tw>*w) *w=tw;
    (*h)+=th;
  }
  const struct addgame_option *option=WIDGET->optionv;
  int i=ADDGAME_VISIBLE_OPTION_COUNT;
  for (;i-->0;option++) {
    if (option->texture) {
      gui_texture_get_size(&tw,&th,option->texture);
      if (tw>*w) *w=tw;
    }
    // Add the font's line height whether an option exists or not; we should have a fixed height throughout.
    (*h)+=WIDGET->font->lineh;
  }
  (*w)+=WIDGET->marginx<<1;
  (*h)+=WIDGET->marginy<<1;
}

/* Pack.
 */
 
static void _addgame_pack(struct gui_widget *widget) {
}

/* Draw.
 */
 
static void _addgame_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x303030ff);
  int dstx=x+WIDGET->marginx;
  int dsty=y+WIDGET->marginy;
  if (WIDGET->pathtex) gui_draw_texture(widget->gui,dstx,dsty,WIDGET->pathtex);
  dsty+=WIDGET->font->lineh; // whether pathtex existed or not
  struct addgame_option *option=WIDGET->optionv;
  int i=ADDGAME_VISIBLE_OPTION_COUNT,basep=WIDGET->scroll;
  for (;i-->0;option++,dsty+=WIDGET->font->lineh,basep++) {
    if (option->texture) gui_draw_texture(widget->gui,dstx,dsty,option->texture);
    if (basep==WIDGET->focusp) {
      gui_frame_rect(widget->gui,dstx,dsty,widget->w-(WIDGET->marginx<<1),WIDGET->font->lineh,0xffff00ff);
    }
  }
  if (WIDGET->http_corrid) {
    gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x80808080);
  }
}

/* Copy file to provided path.
 * Get the source path from our UI.
 * This happens after preflight and we've prevented any UI changes during preflight.
 */
 
static int addgame_copy_file(const char *dstpath,struct gui_widget *widget) {
  if ((WIDGET->focusp<0)||(WIDGET->focusp>=WIDGET->basec)) return -1;
  char srcpath[1024];
  const char *sep="/"; // usually
  if (WIDGET->path.c==1) sep=""; // but at root, we don't need it
  int srcpathc=snprintf(srcpath,sizeof(srcpath),"%.*s%s%s",WIDGET->path.c,WIDGET->path.v,sep,WIDGET->basev[WIDGET->focusp]);
  if ((srcpathc<1)||(srcpathc>=sizeof(srcpath))) return -1;
  
  if (dir_mkdirp_parent(dstpath)<0) return -1;
  
  void *serial=0;
  int serialc=file_read(&serial,srcpath);
  if (serialc<0) return -1;
  
  int err=file_write(dstpath,serial,serialc);
  free(serial);
  return err;
}

/* PUT /api/game complete.
 * The game is now installed, and the response body should be its id, no wrappers.
 */
 
static void addgame_cb_put(struct eh_http_response *rsp,void *userdata) {
  struct gui_widget *widget=userdata;
  WIDGET->http_corrid=0;
  int gameid=0;
  if (sr_int_eval(&gameid,rsp->body,rsp->bodyc)<2) {
    fprintf(stderr,"Failed to evaluate gameid after upload: '%.*s'\n",rsp->bodyc,rsp->body);
    MN_SOUND(REJECT)
    return;
  }
  fprintf(stderr,"Uploaded gameid %d\n",gameid);
  MN_SOUND(ACTIVATE)
  
  /* mn_widget_type_edit currently expects (gameid) to be in the latest search results.
   * It's definitely not there, we can say that for sure.
   * TODO Have 'edit' make an HTTP call when needed. Or maybe db_service. 
   * Not sure exactly whose problem that is, but surely not mine.
  struct gui_widget *modal=gui_push_modal(widget->gui,&mn_widget_type_edit);
  if (!modal) return;
  mn_widget_edit_setup(modal,gameid);
  */
  
  gui_dismiss_modal(widget->gui,widget);
}

/* Upload preflight complete.
 * This was PUT /api/game/file?dry-run=1, which will tell us the path where it would have installed.
 * Since we only run on the same host as the server, no worries, we can install it there ourselves.
 * That's the only way to handle it, since we can't send more than 64 kB at a time thru fakews.
 */
 
static void addgame_cb_preflight(struct eh_http_response *rsp,void *userdata) {
  struct gui_widget *widget=userdata;
  WIDGET->http_corrid=0;
  
  char dstpath[1024],platform[32],name[32];
  int dstpathc=0,platformc=0,namec=0;
  if (rsp->status==200) {
    struct sr_decoder decoder={.v=rsp->body,.c=rsp->bodyc};
    if (sr_decode_json_object_start(&decoder)>=0) {
      const char *k;
      int kc;
      while ((kc=sr_decode_json_next(&k,&decoder))>0) {
      
        if ((kc==4)&&!memcmp(k,"path",4)) {
          dstpathc=sr_decode_json_string(dstpath,sizeof(dstpath),&decoder);
          if (dstpathc>=sizeof(dstpath)) dstpathc=0; // don't skip, just let it fail at the next "next".
      
        } else if ((kc==8)&&!memcmp(k,"platform",8)) {
          platformc=sr_decode_json_string(platform,sizeof(platform),&decoder);
          if (platformc>sizeof(platform)) platformc=0;
      
        } else if ((kc==4)&&!memcmp(k,"name",4)) {
          namec=sr_decode_json_string(name,sizeof(name),&decoder);
          if (namec>sizeof(name)) namec=0;
          
        } else {
          sr_decode_json_skip(&decoder);
        }
      }
    }
  }
  
  if ((dstpathc<=0)||(platformc<=0)||(namec<=0)) {
    fprintf(stderr,"Add game preflight failed. Maybe we couldn't guess the platform?\n");
    MN_SOUND(REJECT)
    return;
  }
  fprintf(stderr,
    "Add game preflight looks good. platform=%.*s name='%.*s' path='%.*s'\n",
    platformc,platform,namec,name,dstpathc,dstpath
  );

  if (addgame_copy_file(dstpath,widget)<0) {
    fprintf(stderr,"%s: Copying ROM file failed! Aborting installation.\n",dstpath);
    MN_SOUND(REJECT)
    return;
  }
  
  struct sr_encoder req={0};
  sr_encode_json_object_start(&req,0,0);
  sr_encode_json_string(&req,"path",4,dstpath,dstpathc);
  sr_encode_json_string(&req,"platform",8,platform,platformc);
  sr_encode_json_string(&req,"name",4,name,namec);
  if (sr_encode_json_object_end(&req,0)!=0) {
    MN_SOUND(REJECT)
    sr_encoder_cleanup(&req);
    return;
  }
  
  if ((WIDGET->http_corrid=dbs_request_http_with_body(&mn.dbs,"PUT","/api/game?detail=id",req.v,req.c,0,addgame_cb_put,widget))<0) {
    MN_SOUND(REJECT)
    sr_encoder_cleanup(&req);
    WIDGET->http_corrid=0;
    return;
  }
  
  sr_encoder_cleanup(&req);
}

/* Activate.
 */
 
static void addgame_activate(struct gui_widget *widget) {
  if (WIDGET->http_corrid) return;
  if ((WIDGET->focusp<0)||(WIDGET->focusp>=WIDGET->basec)) return;
  char path[1024];
  const char *sep="/"; // usually
  if (WIDGET->path.c==1) sep=""; // but at root, we don't need it
  int pathc=snprintf(path,sizeof(path),"%.*s%s%s",WIDGET->path.c,WIDGET->path.v,sep,WIDGET->basev[WIDGET->focusp]);
  if ((pathc<1)||(pathc>=sizeof(path))) return;
  
  // If they picked a double-dot entry, back out in place.
  if ((pathc>=4)&&!memcmp(path+pathc-4,"/../",4)) {
    int p=pathc-5;
    while (p&&(path[p]!='/')) p--;
    if (!p) p=1;
    if (addgame_set_wd(widget,path,p)<0) return;
    MN_SOUND(MINOR_OK)
    return;
  }
  
  // If they picked anything else that ends with slash, drop the slash and chdir to it.
  if (path[pathc-1]=='/') {
    if (addgame_set_wd(widget,path,pathc-1)<0) return;
    MN_SOUND(MINOR_OK)
    return;
  }
  
  /* God damn it.
   * fakews has a packet size limit of 64k. ROM files can easily exceed that; the one I'm testing with happens to be like 500k.
   * So we can't use PUT /api/game/file, it's a real shame.
   * We can still kind of use it -- make a dry-run call to acquire the preferred path, then copy the file ourselves.
   */
  char reqpath[256]="/api/game/file?dry-run=1&name=";
  int reqpathc=30;
  int err=sr_url_encode(reqpath+reqpathc,sizeof(reqpath)-reqpathc,WIDGET->basev[WIDGET->focusp],-1);
  if ((err<0)||(reqpathc>=sizeof(reqpath)-err)) {
    return;
  }
  err=dbs_request_http(&mn.dbs,"PUT",reqpath,addgame_cb_preflight,widget);
  if (err<0) {
    fprintf(stderr,"%s:%d: Error sending upload preflight.\n",__FILE__,__LINE__);
    return;
  }
  WIDGET->http_corrid=err;
}

/* If the focus is outside our visible window, adjust scroll just enough to catch it.
 * Rebuilds (optionv) as needed.
 */
 
static int addgame_ensure_scroll(struct gui_widget *widget) {
  int nscroll=WIDGET->scroll;
  if (WIDGET->focusp<nscroll) {
    nscroll=WIDGET->focusp;
    if (nscroll<0) nscroll=0;
  }
  if (WIDGET->focusp>=nscroll+ADDGAME_VISIBLE_OPTION_COUNT) {
    nscroll=WIDGET->focusp+1-ADDGAME_VISIBLE_OPTION_COUNT;
    if (nscroll<0) nscroll=0;
  }
  if (nscroll==WIDGET->scroll) return 0;
  WIDGET->scroll=nscroll;
  if (addgame_rebuild_optionv(widget)<0) return -1; // This is heavy-handed. Usually only one texture needs redrawn. TODO
  return 0;
}

/* Motion.
 */
 
static void _addgame_motion(struct gui_widget *widget,int dx,int dy) {
  if (WIDGET->http_corrid) return;
  if (!dy) return;
  int np=WIDGET->focusp+dy;
  if (np<0) np=WIDGET->basec-1;
  else if (np>=WIDGET->basec) np=0;
  if (np==WIDGET->focusp) return;
  MN_SOUND(MOTION)
  WIDGET->focusp=np;
  addgame_ensure_scroll(widget);
}

/* Signal.
 */
 
static void _addgame_signal(struct gui_widget *widget,int sigid) {
  // Cancelling is permitted while (http_corrid) in flight. All others must check.
  switch (sigid) {
    case GUI_SIGID_CANCEL: MN_SOUND(CANCEL) gui_dismiss_modal(widget->gui,widget); return;
    case GUI_SIGID_ACTIVATE: addgame_activate(widget); break;
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_addgame={
  .name="addgame",
  .objlen=sizeof(struct mn_widget_addgame),
  .del=_addgame_del,
  .init=_addgame_init,
  .measure=_addgame_measure,
  .pack=_addgame_pack,
  .draw=_addgame_draw,
  .motion=_addgame_motion,
  .signal=_addgame_signal,
};
