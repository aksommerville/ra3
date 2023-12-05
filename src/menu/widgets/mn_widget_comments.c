#include "../mn_internal.h"
#include "opt/serial/serial.h"

#define COMMENT_LABEL_LENGTH_LIMIT 16

/* Object definition.
 */
 
struct mn_widget_comments {
  struct gui_widget hdr;
  void (*cb)(struct gui_widget *comments,void *userdata);
  void *userdata;
  int gameid;
  int editp;
  struct comment_row { // parallel to comments from setup
    char label[COMMENT_LABEL_LENGTH_LIMIT];
    int labelc;
    char *text;
    int textc;
    char time[32];
    int timec;
  } *rowv;
  int rowc,rowa;
  int http_corrid;
};

#define WIDGET ((struct mn_widget_comments*)widget)

/* Delete.
 */
 
static void comment_row_cleanup(struct comment_row *row) {
  if (row->text) free(row->text);
}
 
static void _comments_del(struct gui_widget *widget) {
  if (WIDGET->rowv) {
    while (WIDGET->rowc-->0) comment_row_cleanup(WIDGET->rowv+WIDGET->rowc);
    free(WIDGET->rowv);
  }
}

/* HTTP response (PATCH,PUT).
 */
 
static void comments_cb_http(struct eh_http_response *rsp,void *userdata) {
  struct gui_widget *widget=userdata;
  WIDGET->http_corrid=0;
  if (rsp->status==200) {
    MN_SOUND(ACTIVATE)
    dbs_refresh_search(&mn.dbs);
    if ((WIDGET->editp<0)||(WIDGET->editp>=WIDGET->rowc)) {
      // We've added one. It's complicated to try adding rows to the form. Safer to self-dismiss.
      gui_dismiss_modal(widget->gui,widget);
    }
  } else {
    MN_SOUND(REJECT)
  }
}

/* HTTP response (DELETE).
 */
 
static void comments_cb_delete(struct eh_http_response *rsp,void *userdata) {
  struct gui_widget *widget=userdata;
  WIDGET->http_corrid=0;
  MN_SOUND(ACTIVATE)
  dbs_refresh_search(&mn.dbs);
  gui_dismiss_modal(widget->gui,widget);
}

/* Encode request body for update or insert.
 */
 
static int comments_encode_request(struct sr_encoder *dst,struct gui_widget *widget,int patch,const char *v,int c) {
  if (sr_encode_json_object_start(dst,0,0)!=0) return -1;
  sr_encode_json_int(dst,"gameid",6,WIDGET->gameid);
  
  if (patch) {
    if ((WIDGET->editp<0)||(WIDGET->editp>=WIDGET->rowc)) return -1;
    struct comment_row *row=WIDGET->rowv+WIDGET->editp;
    sr_encode_json_string(dst,"time",4,row->time,row->timec);
    sr_encode_json_string(dst,"k",1,row->label,row->labelc);
    
  } else {
    // We're not providing UI to set (k).
    // User may prefix the text with "[K]", we'll figure it out here.
    if ((c>=1)&&(v[0]=='[')) {
      const char *k=v+1;
      int kc=0;
      while ((1+kc<c)&&(v[1+kc]!=']')) kc++;
      if (1+kc<c) {
        v+=2+kc;
        c-=2+kc;
        while (c&&((unsigned char)v[0]<=0x20)) { v++; c--; }
        sr_encode_json_string(dst,"k",1,k,kc);
      } else {
        sr_encode_json_string(dst,"k",1,"text",4);
      }
    } else {
      sr_encode_json_string(dst,"k",1,"text",4);
    }
  }
  
  sr_encode_json_string(dst,"v",1,v,c);
  if (sr_encode_json_object_end(dst,0)<0) return -1;
  return 0;
}

/* Commit edit.
 */
 
static void comments_cb_edited(struct gui_widget *entry,const char *v,int c,void *userdata) {
  struct gui_widget *widget=userdata;
  if ((WIDGET->editp<0)||(WIDGET->editp>=WIDGET->rowc)) return;
  struct comment_row *row=WIDGET->rowv+WIDGET->editp;
  
  if (!c) {
    char reqpath[256];
    int reqpathc=snprintf(reqpath,sizeof(reqpath),"/api/comment?gameid=%d&time=%.*s&k=%.*s",WIDGET->gameid,row->timec,row->time,row->labelc,row->label);
    if ((reqpathc<0)||(reqpathc>=sizeof(reqpath))) return;
    WIDGET->http_corrid=dbs_request_http(&mn.dbs,"DELETE",reqpath,comments_cb_delete,widget);
    // gui_widget_form doesn't let us delete rows, and it's a little complicated.
    // So to keep things from getting out of hand, we're going to self-dismiss once the deletion succeeds.
    gui_dismiss_modal(widget->gui,entry);
    return;
  }

  char *nv=malloc(c+1);
  if (nv) {
    memcpy(nv,v,c);
    nv[c]=0;
    if (row->text) free(row->text);
    row->text=nv;
    row->textc=c;
    if (widget->childc>=1) {
      struct gui_widget *form=widget->childv[0];
      if (WIDGET->editp<form->childc) {
        struct gui_widget *button=form->childv[WIDGET->editp];
        gui_widget_button_set_label(button,v,(c<30)?c:30,0xffffff);
        gui_dirty_pack(widget->gui);
      }
    }
  }
  
  struct sr_encoder req={0};
  if (comments_encode_request(&req,widget,1,v,c)<0) {
    sr_encoder_cleanup(&req);
    return;
  }
  gui_dismiss_modal(widget->gui,entry);
  WIDGET->http_corrid=dbs_request_http_with_body(&mn.dbs,"PATCH","/api/comment",req.v,req.c,0,comments_cb_http,widget);
  sr_encoder_cleanup(&req);
}

/* Commit new comment.
 */
 
static void comments_cb_insert(struct gui_widget *entry,const char *v,int c,void *userdata) {
  struct gui_widget *widget=userdata;
  if (!c) return;
  struct sr_encoder req={0};
  if (comments_encode_request(&req,widget,0,v,c)<0) {
    sr_encoder_cleanup(&req);
    return;
  }
  gui_dismiss_modal(widget->gui,entry);
  WIDGET->http_corrid=dbs_request_http_with_body(&mn.dbs,"PUT","/api/comment",req.v,req.c,0,comments_cb_http,widget);
  sr_encoder_cleanup(&req);
}

/* Edit existing comment.
 */
 
static void comments_edit_1(struct gui_widget *widget,int p) {
  if ((p<0)||(p>=WIDGET->rowc)) return;
  struct gui_widget *entry=gui_push_modal(widget->gui,&gui_widget_type_entry);
  if (!entry) return;
  gui_widget_entry_setup(entry,WIDGET->rowv[p].text,WIDGET->rowv[p].textc,comments_cb_edited,widget);
  WIDGET->editp=p;
}

/* Begin adding new comment.
 */
 
static void comments_begin_new(struct gui_widget *widget) {
  struct gui_widget *entry=gui_push_modal(widget->gui,&gui_widget_type_entry);
  if (!entry) return;
  gui_widget_entry_setup(entry,0,0,comments_cb_insert,widget);
  WIDGET->editp=-1;
}

/* Form item selected.
 */
 
static void comments_cb_form(struct gui_widget *form,int p,void *userdata) {
  struct gui_widget *widget=userdata;
  if (p<0) return;
  if (p<WIDGET->rowc) {
    comments_edit_1(widget,p);
  } else {
    comments_begin_new(widget);
  }
}

/* Init.
 */
 
static int _comments_init(struct gui_widget *widget) {
  WIDGET->editp=-1;
  struct gui_widget *form=gui_widget_spawn(widget,&gui_widget_type_form);
  if (!form) return -1;
  gui_widget_form_set_index_callback(form,comments_cb_form,widget);
  return 0;
}

/* Measure, pack, draw, motion: Defer to form.
 */
 
static void _comments_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  if (widget->childc<1) return;
  struct gui_widget *form=widget->childv[0];
  gui_widget_measure(w,h,form,wlimit,hlimit);
}
 
static void _comments_pack(struct gui_widget *widget) {
  if (widget->childc<1) return;
  struct gui_widget *form=widget->childv[0];
  form->x=0;
  form->y=0;
  form->w=widget->w;
  form->h=widget->h;
  gui_widget_pack(form);
}
 
static void _comments_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x302050ff);
  int i=0;
  for (;i<widget->childc;i++) {
    struct gui_widget *child=widget->childv[i];
    gui_widget_draw(child,x+child->x,y+child->y);
  }
  if (WIDGET->http_corrid) {
    gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x80808080);
  }
}
 
static void _comments_motion(struct gui_widget *widget,int dx,int dy) {
  if (WIDGET->http_corrid) return;
  if (widget->childc<1) return;
  struct gui_widget *form=widget->childv[0];
  if (form->type->motion) form->type->motion(form,dx,dy);
}

/* Signal.
 */
 
static void _comments_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_CANCEL: MN_SOUND(CANCEL) gui_dismiss_modal(widget->gui,widget); return;
    default: {
        if (WIDGET->http_corrid) return;
        if (widget->childc<1) return;
        struct gui_widget *form=widget->childv[0];
        if (form->type->signal) form->type->signal(form,sigid);
      } break;
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_comments={
  .name="comments",
  .objlen=sizeof(struct mn_widget_comments),
  .del=_comments_del,
  .init=_comments_init,
  .measure=_comments_measure,
  .pack=_comments_pack,
  .draw=_comments_draw,
  .motion=_comments_motion,
  .signal=_comments_signal,
};

/* Populate our form.
 */
 
static int comments_populate(struct gui_widget *widget,const char *src,int srcc) {
  if (widget->childc<1) return -1;
  
  while (WIDGET->rowc>0) {
    WIDGET->rowc--;
    comment_row_cleanup(WIDGET->rowv+WIDGET->rowc);
  }
  WIDGET->editp=-1;
  
  struct gui_widget *form=widget->childv[0];
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_array_start(&decoder)<0) return 0;
  while (sr_decode_json_next(0,&decoder)>0) {
    char label[16],value[1024],time[32];
    int labelc=0,valuec=0,timec=3;
    int objctx=sr_decode_json_object_start(&decoder);
    if (objctx<0) return -1;
    const char *k;
    int kc;
    while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    
      if ((kc==1)&&(k[0]=='k')) {
        if ((labelc=sr_decode_json_string(label,sizeof(label),&decoder))<0) return -1;
        if (labelc>sizeof(label)) return -1;
        
      } else if ((kc==1)&&(k[0]=='v')) {
        if ((valuec=sr_decode_json_string(value,sizeof(value),&decoder))<0) return -1;
        if (valuec>sizeof(value)) {
          memset(value,'.',3);
          valuec=3;
          if (sr_decode_json_skip(&decoder)<0) return -1;
        }
        
      } else if ((kc==4)&&!memcmp(k,"time",4)) {
        if ((timec=sr_decode_json_string(time,sizeof(time),&decoder))<0) return -1;
        if (timec>sizeof(time)) return -1;
        
      } else {
        if (sr_decode_json_skip(&decoder)<0) return -1;
      }
    }
    if (sr_decode_json_end(&decoder,objctx)<0) return -1;
    
    if (WIDGET->rowc>=WIDGET->rowa) {
      int na=WIDGET->rowa+8;
      if (na>INT_MAX/sizeof(struct comment_row)) return -1;
      void *nv=realloc(WIDGET->rowv,sizeof(struct comment_row)*na);
      if (!nv) return -1;
      WIDGET->rowv=nv;
      WIDGET->rowa=na;
    }
    struct comment_row *row=WIDGET->rowv+WIDGET->rowc++;
    memset(row,0,sizeof(struct comment_row));
    memcpy(row->label,label,labelc);
    row->labelc=labelc;
    if (!(row->text=malloc(valuec+1))) return -1;
    memcpy(row->text,value,valuec);
    row->text[valuec]=0;
    row->textc=valuec;
    memcpy(row->time,time,timec);
    row->timec=timec;
    
    const int lenlimit=30; // arbitrary length limit on value, display an elided prefix if exceeded (as opposed to eliding the whole thing, if it overflows the buffer)
    if (valuec>lenlimit) { 
      memset(value+lenlimit-3,'.',3);
      valuec=lenlimit;
    }
    if (!gui_widget_form_add_string(form,label,labelc,value,valuec,1)) return -1;
  }
  return 0;
}

/* Setup.
 */

int mn_widget_comments_setup(
  struct gui_widget *widget,
  int gameid,
  const char *src,int srcc,
  void (*cb)(struct gui_widget *comments,void *userdata),
  void *userdata
) {
  if (!widget||(widget->type!=&mn_widget_type_comments)) return -1;
  WIDGET->gameid=gameid;
  WIDGET->cb=cb;
  WIDGET->userdata=userdata;
  if (comments_populate(widget,src,srcc)<0) return -1;
  if (!gui_widget_form_add_string(widget->childv[0],"New",3,"...",3,1)) return -1;
  return 0;
}
