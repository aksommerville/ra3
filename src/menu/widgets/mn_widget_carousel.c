/* mn_widget_carousel.
 * Top row of home page.
 */
 
#include "../mn_internal.h"
#include "opt/png/png.h"
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"
#include <math.h>

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>

/* Object definition.
 */
 
struct mn_widget_carousel {
  struct gui_widget hdr;
  struct gui_program *program;
  GLuint loc_dstposition;
  GLuint loc_dstsize;
  GLuint loc_alpha;
  GLfloat alpha,alpha_target;
  struct gui_texture *texture_noscreencap;
  int gamelistseq;
  struct gui_texture *prev_page_texture;
  struct gui_texture *next_page_texture;
  struct png_image *indicators_image;
  struct gui_texture *list_deletion_texture;
  int list_deletion_listid;
  
  struct mn_carousel_entry {
    int gameid;
    char *scpath;
    struct gui_texture *scap;
    GLfloat x; // in model space. 0=center
    GLfloat z; // '' 0.5=center
    GLfloat t; // 0=focus, -pi/2=left pi/2=right
    GLfloat dstx,dstt,dstz; // target, if animating
  } *entryv;
  int entryc,entrya;
  int entryp;
  int animation_stable;
  
  /* -1 or 1 if we should bind to one edge of the next incoming set of games.
   */
  int autoselect_direction;
};

#define WIDGET ((struct mn_widget_carousel*)widget)

/* Shader.
 */
 
static const char carousel_vsrc[]=
  "uniform vec2 screensize;\n"
  "uniform vec2 dstposition;\n"
  "uniform vec2 dstsize;\n"
  "attribute vec3 aposition;\n"
  "attribute vec2 atexcoord;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
    "float adjx=(aposition.x*dstsize.y)/dstsize.x;\n"
    "float adjz=0.300+aposition.z*0.200;\n"
    "gl_Position=vec4(\n"
      "dstposition.x/screensize.x+(adjx*dstsize.x)/screensize.x,\n"
      "dstposition.y/screensize.y+(aposition.y*dstsize.y)/screensize.y,\n"
      "adjz,\n"
      "adjz\n"
    ");\n"
    "vtexcoord=atexcoord;\n"
  "}\n"
"";

static const char carousel_fsrc[]=
  "uniform sampler2D sampler;\n"
  "uniform float alpha;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
    "gl_FragColor=texture2D(sampler,vtexcoord);\n"
    "gl_FragColor.a*=alpha;\n"
  "}\n"
"";

/* Delete.
 */
 
static void mn_carousel_entry_cleanup(struct mn_carousel_entry *entry) {
  if (entry->scpath) free(entry->scpath);
  if (entry->scap) gui_texture_del(entry->scap);
}
 
static void _carousel_del(struct gui_widget *widget) {
  gui_program_del(WIDGET->program);
  gui_texture_del(WIDGET->texture_noscreencap);
  gui_texture_del(WIDGET->prev_page_texture);
  gui_texture_del(WIDGET->next_page_texture);
  gui_texture_del(WIDGET->list_deletion_texture);
  png_image_del(WIDGET->indicators_image);
  if (WIDGET->entryv) {
    while (WIDGET->entryc-->0) mn_carousel_entry_cleanup(WIDGET->entryv+WIDGET->entryc);
    free(WIDGET->entryv);
  }
}

/* Init.
 */
 
static int _carousel_init(struct gui_widget *widget) {

  if (!(WIDGET->program=gui_program_new("carousel",carousel_vsrc,sizeof(carousel_vsrc)-1,carousel_fsrc,sizeof(carousel_fsrc)-1))) return -1;
  WIDGET->loc_dstposition=gui_program_get_uniform(WIDGET->program,"dstposition");
  WIDGET->loc_dstsize=gui_program_get_uniform(WIDGET->program,"dstsize");
  WIDGET->loc_alpha=gui_program_get_uniform(WIDGET->program,"alpha");
  
  if (!(WIDGET->texture_noscreencap=gui_texture_new())) return -1;
  gui_texture_set_filter(WIDGET->texture_noscreencap,1);
  void *serial=0;
  int serialc=file_read(&serial,mn_data_path("noscreencap.png"));
  if (serialc<0) return -1;
  struct png_image *image=png_decode(serial,serialc);
  free(serial);
  if (!image) return -1;
  if ((image->depth!=8)||(image->colortype!=6)) return -1;
  gui_texture_upload_rgba(WIDGET->texture_noscreencap,image->w,image->h,image->pixels);
  png_image_del(image);
  
  if ((serialc=file_read(&serial,mn_data_path("carousel-edges.png")))>=0) {
    WIDGET->indicators_image=png_decode(serial,serialc);
    free(serial);
    if (WIDGET->indicators_image&&(
      (WIDGET->indicators_image->w<160)||
      (WIDGET->indicators_image->h<48)||
      (WIDGET->indicators_image->depth!=8)||
      (WIDGET->indicators_image->colortype!=6)
    )) {
      png_image_del(WIDGET->indicators_image);
      WIDGET->indicators_image=0;
    }
  }
  
  WIDGET->animation_stable=1;
  WIDGET->alpha=1.0f;
  WIDGET->alpha_target=1.0f;
  
  return 0;
}

/* Pack.
 */
 
static void _carousel_pack(struct gui_widget *widget) {
}

/* Draw a previous-page or next-page indicator.
 */
 
static void mn_carousel_render_indicator(struct gui_widget *widget,struct gui_texture *texture,GLfloat x) {
  GLfloat w=0.150f;
  GLfloat h=0.150f;
  struct carousel_vertex {
    GLfloat x,y,z;
    GLubyte tx,ty;
  } vtxv[]={
    {x-w, h,1.0f,0,0},
    {x-w,-h,1.0f,0,1},
    {x+w, h,1.0f,1,0},
    {x+w,-h,1.0f,1,1},
  };
  glVertexAttribPointer(0,3,GL_FLOAT,0,sizeof(struct carousel_vertex),&vtxv[0].x);
  glVertexAttribPointer(1,2,GL_UNSIGNED_BYTE,0,sizeof(struct carousel_vertex),&vtxv[0].tx);
  gui_texture_use(texture);
  glDrawArrays(GL_TRIANGLE_STRIP,0,sizeof(vtxv)/sizeof(vtxv[0]));
}

/* Draw one entry. Caller preps the context in advance.
 */
 
static void mn_carousel_draw_entry(struct gui_widget *widget,struct mn_carousel_entry *entry) {

  struct gui_texture *texture=entry->scap;
  if (!texture) texture=WIDGET->texture_noscreencap;
  GLfloat cost=cosf(entry->t);
  GLfloat sint=sinf(entry->t);
  int texw,texh;
  gui_texture_get_size(&texw,&texh,texture);
  GLfloat logw=0.300f;
  GLfloat logh=((GLfloat)texh*logw)/(GLfloat)texw;
  gui_texture_use(texture);
  
  struct carousel_vertex {
    GLfloat x,y,z;
    GLubyte tx,ty;
  } vtxv[]={
    {entry->x-cost*logw, logh,entry->z+sint*logw,0,0},
    {entry->x-cost*logw,-logh,entry->z+sint*logw,0,1},
    {entry->x+cost*logw, logh,entry->z-sint*logw,1,0},
    {entry->x+cost*logw,-logh,entry->z-sint*logw,1,1},
  };
  glVertexAttribPointer(0,3,GL_FLOAT,0,sizeof(struct carousel_vertex),&vtxv[0].x);
  glVertexAttribPointer(1,2,GL_UNSIGNED_BYTE,0,sizeof(struct carousel_vertex),&vtxv[0].tx);
  glDrawArrays(GL_TRIANGLE_STRIP,0,sizeof(vtxv)/sizeof(vtxv[0]));
}

/* Draw.
 */
 
static void _carousel_draw(struct gui_widget *widget,int x,int y) {
  if (WIDGET->entryc<1) {
    if (WIDGET->list_deletion_texture) {
      int tw,th;
      gui_texture_get_size(&tw,&th,WIDGET->list_deletion_texture);
      gui_draw_texture(widget->gui,x+(widget->w>>1)-(tw>>1),y+(widget->h>>1)-(th>>1),WIDGET->list_deletion_texture);
    }
    return;
  }

  gui_program_use(WIDGET->program);
  int screenw,screenh;
  gui_get_screen_size(&screenw,&screenh,widget->gui);
  glViewport(x,screenh-widget->h-y,widget->w,widget->h);
  gui_program_set_screensize(WIDGET->program,widget->w,widget->h);
  gui_program_set_sampler(WIDGET->program,0);
  glUniform2f(WIDGET->loc_dstposition,0.0f,0.0f);
  glUniform2f(WIDGET->loc_dstsize,widget->w,widget->h);
  glUniform1f(WIDGET->loc_alpha,WIDGET->alpha);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_DST_ALPHA);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  
  //XXX Don't load an unlimited number of screencaps. And don't load them during render, and don't examine each of them each frame after, for ever.
  // I think move this to update, and pay out the loads over the first few updates?
  int ii=0; for (;ii<WIDGET->entryc;ii++) {
    struct mn_carousel_entry *entry=WIDGET->entryv+ii;
    if (entry->scpath&&!entry->scap) {
      if (!(entry->scap=gui_texture_new())) break;
      void *serial=0;
      int serialc=file_read(&serial,entry->scpath);
      if (serialc<0) break;
      struct png_image *image=png_decode(serial,serialc);
      free(serial);
      if (!image) break;
      struct png_image *alt=png_image_reformat(image,0,0,0,0,8,6,1);
      png_image_del(image);
      image=alt;
      if (!image) break;
						// Use the filter above 65x65px; no filter at or below.
						// sic 65: My cap for Upsy-Downsy intentionally has an extra black row on top.
						if ((image->w>65)||(image->h>65)) {
						  gui_texture_set_filter(entry->scap,1);
						} else {
						  gui_texture_set_filter(entry->scap,0);
						}
      gui_texture_upload_rgba(entry->scap,image->w,image->h,image->pixels);
      png_image_del(image);
    }
  }
  
  // Previous and next page indicators, if present.
  if (WIDGET->entryc>0) {
    if (WIDGET->prev_page_texture) {
      mn_carousel_render_indicator(widget,WIDGET->prev_page_texture,WIDGET->entryv[0].x-0.800f);
    }
    if (WIDGET->next_page_texture) {
      mn_carousel_render_indicator(widget,WIDGET->next_page_texture,WIDGET->entryv[WIDGET->entryc-1].x+0.800f);
    }
  }
  
  // Left side.
  int i;
  for (i=0;i<WIDGET->entryp;i++) {
    struct mn_carousel_entry *entry=WIDGET->entryv+i;
    mn_carousel_draw_entry(widget,entry);
  }
  // Right side.
  for (i=WIDGET->entryc-1;i>WIDGET->entryp;i--) {
    struct mn_carousel_entry *entry=WIDGET->entryv+i;
    mn_carousel_draw_entry(widget,entry);
  }
  // Center.
  if ((WIDGET->entryp>=0)&&(WIDGET->entryp<WIDGET->entryc)) {
    mn_carousel_draw_entry(widget,WIDGET->entryv+WIDGET->entryp);
  }
  
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  gui_program_use(0);
  glViewport(0,0,screenw,screenh);
}

/* Rewrite (x,t) for all entries, after a fresh load.
 */
 
static void mn_carousel_reset_entry_positions(struct gui_widget *widget) {
  const GLfloat initial_spacing=0.400f; // between the focus and the first of the herds
  const GLfloat spacing=0.100f; // between sidewise entries
  if ((WIDGET->entryp>=0)&&(WIDGET->entryp<WIDGET->entryc)) {
    struct mn_carousel_entry *entry=WIDGET->entryv+WIDGET->entryp;
    entry->x=entry->dstx=0.0f;
    entry->t=entry->dstt=0.0f;
    entry->z=entry->dstz=0.100f;
  }
  int pl=WIDGET->entryp-1,pr=WIDGET->entryp+1;
  GLfloat xl=-initial_spacing,xr=initial_spacing;
  while (1) {
    int proceed=0;
    if (pl>=0) {
      proceed=1;
      struct mn_carousel_entry *entry=WIDGET->entryv+pl;
      entry->x=entry->dstx=xl;
      entry->t=entry->dstt=M_PI*-0.5f;
      entry->z=entry->dstz=0.500f;
      pl--;
      xl-=spacing;
    }
    if (pr<WIDGET->entryc) {
      proceed=1;
      struct mn_carousel_entry *entry=WIDGET->entryv+pr;
      entry->x=entry->dstx=xr;
      entry->t=entry->dstt=M_PI*0.5f;
      entry->z=entry->dstz=0.500f;
      pr++;
      xr+=spacing;
    }
    if (!proceed) break;
  }
  WIDGET->animation_stable=1;
}

/* Same as resetting positions, but keep current state and only change the targets.
 */
 
static void mn_carousel_animate_entry_positions(struct gui_widget *widget) {
  const GLfloat initial_spacing=0.400f; // between the focus and the first of the herds
  const GLfloat spacing=0.100f; // between sidewise entries
  if ((WIDGET->entryp>=0)&&(WIDGET->entryp<WIDGET->entryc)) {
    struct mn_carousel_entry *entry=WIDGET->entryv+WIDGET->entryp;
    entry->dstx=0.0f;
    entry->dstt=0.0f;
    entry->dstz=0.100f;
  }
  int pl=WIDGET->entryp-1,pr=WIDGET->entryp+1;
  GLfloat xl=-initial_spacing,xr=initial_spacing;
  while (1) {
    int proceed=0;
    if (pl>=0) {
      proceed=1;
      struct mn_carousel_entry *entry=WIDGET->entryv+pl;
      entry->dstx=xl;
      entry->dstt=M_PI*-0.5f;
      entry->dstz=0.500f;
      pl--;
      xl-=spacing;
    }
    if (pr<WIDGET->entryc) {
      proceed=1;
      struct mn_carousel_entry *entry=WIDGET->entryv+pr;
      entry->dstx=xr;
      entry->dstt=M_PI*0.5f;
      entry->dstz=0.500f;
      pr++;
      xr+=spacing;
    }
    if (!proceed) break;
  }
  WIDGET->animation_stable=0;
}

/* Add entry to list.
 */
 
static struct mn_carousel_entry *mn_carousel_append_entry(struct gui_widget *widget) {
  if (WIDGET->entryc>=WIDGET->entrya) {
    int na=WIDGET->entrya+32;
    if (na>INT_MAX/sizeof(struct mn_carousel_entry)) return 0;
    void *nv=realloc(WIDGET->entryv,sizeof(struct mn_carousel_entry)*na);
    if (!nv) return 0;
    WIDGET->entryv=nv;
    WIDGET->entrya=na;
  }
  struct mn_carousel_entry *entry=WIDGET->entryv+WIDGET->entryc++;
  memset(entry,0,sizeof(struct mn_carousel_entry));
  return entry;
}

/* Nonzero if this JSON-encoded string looks like a screencap path.
 * As opposed to other blob paths.
 */
 
static int mn_carousel_encoded_path_looks_like_scap(const char *src,int srcc) {
  if (srcc<6) return 0;
  if (src[0]!='"') return 0;
  if (memcmp(src+srcc-5,".png\"",5)) return 0;
  // Should we bother looking for "-scap-"? I'm thinking one PNG is as good as another.
  return 1;
}

/* Redraw prev_page_texture and next_page_texture.
 */
 
static void mn_carousel_indicators_blit(uint8_t *dst,int dstx,int dsty,struct gui_widget *widget,int srcx,int srcy,int w,int h) {
  int dststride=64*4;
  dst+=dsty*dststride+(dstx<<2);
  const uint8_t *src=(uint8_t*)WIDGET->indicators_image->pixels+srcy*WIDGET->indicators_image->stride+(srcx<<2);
  int cpc=w<<2;
  for (;h-->0;dst+=dststride,src+=WIDGET->indicators_image->stride) {
    memcpy(dst,src,cpc);
  }
}
 
static void mn_carousel_draw_indicator(struct gui_widget *widget,struct gui_texture *texture,int arrowindex,int gamec) {
  // It needs to be square. And if we make it 4x4 tiles, we can fit any reasonable content. So 4x4 (64x64 pixels) always.
  uint8_t *tmp=calloc(64*64,4);
  if (!tmp) return;
  
  int digitsy=0;
  if (arrowindex<0) {
    // For the left bookend at first page, show total count.
    mn_carousel_indicators_blit(tmp,16,0,widget,128,0,32,32);
  } else if (gamec<1) {
    mn_carousel_indicators_blit(tmp,16,8,widget,0,0,32,32);
    mn_carousel_indicators_blit(tmp,16,40,widget,96,0,32,16);
  } else {
    mn_carousel_indicators_blit(tmp,16,0,widget,32+arrowindex*32,0,32,32);
    mn_carousel_indicators_blit(tmp,16,48,widget,96,16,32,16);
  }
  if (gamec>0) {
    if (gamec>=10000) gamec=9999; // room for 4 digits only
    if (gamec>=1000) {
      mn_carousel_indicators_blit(tmp,0,32,widget,(gamec/1000)*16,32,16,16);
      mn_carousel_indicators_blit(tmp,16,32,widget,((gamec/100)%10)*16,32,16,16);
      mn_carousel_indicators_blit(tmp,32,32,widget,((gamec/10)%10)*16,32,16,16);
      mn_carousel_indicators_blit(tmp,48,32,widget,(gamec%10)*16,32,16,16);
    } else if (gamec>=100) {
      mn_carousel_indicators_blit(tmp,8,32,widget,((gamec/100)%10)*16,32,16,16);
      mn_carousel_indicators_blit(tmp,24,32,widget,((gamec/10)%10)*16,32,16,16);
      mn_carousel_indicators_blit(tmp,40,32,widget,(gamec%10)*16,32,16,16);
    } else if (gamec>=10) {
      mn_carousel_indicators_blit(tmp,16,32,widget,((gamec/10)%10)*16,32,16,16);
      mn_carousel_indicators_blit(tmp,32,32,widget,(gamec%10)*16,32,16,16);
    } else {
      mn_carousel_indicators_blit(tmp,24,32,widget,(gamec%10)*16,32,16,16);
    }
  }
  
  gui_texture_upload_rgba(texture,64,64,tmp);
  gui_texture_set_filter(texture,1);
  
  free(tmp);
}
 
static int mn_carousel_refresh_page_indicators(struct gui_widget *widget) {
  if (!WIDGET->indicators_image) return 0;
  
  if (!WIDGET->prev_page_texture) {
    WIDGET->prev_page_texture=gui_texture_new();
  }
  if (WIDGET->prev_page_texture) {
    if (mn.dbs.page>1) mn_carousel_draw_indicator(widget,WIDGET->prev_page_texture,0,(mn.dbs.page-1)*DB_SERVICE_SEARCH_LIMIT);
    else mn_carousel_draw_indicator(widget,WIDGET->prev_page_texture,-1,mn.dbs.totalc);
  }
  
  if (!WIDGET->next_page_texture) {
    WIDGET->next_page_texture=gui_texture_new();
  }
  if (WIDGET->next_page_texture) {
    int pagec=mn.dbs.pagec-mn.dbs.page;
    int rightc=0;
    if (pagec) rightc=(pagec-1)*DB_SERVICE_SEARCH_LIMIT+mn.dbs.totalc%DB_SERVICE_SEARCH_LIMIT;
    mn_carousel_draw_indicator(widget,WIDGET->next_page_texture,1,rightc);
  }
  return 0;
}

/* Having confirmed that a selected list is empty, now let's passively ask the user if she wants to delete it.
 */
 
static void carousel_propose_list_deletion(struct gui_widget *widget,int listid) {
  char msg[256];
  int msgc=snprintf(msg,sizeof(msg),"List %d is empty. If you'd like to delete it, press B.",listid);
  if ((msgc<0)||(msgc>=sizeof(msg))) return;
  struct gui_texture *tex=gui_texture_from_text(widget->gui,0,msg,msgc,0xffc0c0);
  if (!tex) return;
  gui_texture_del(WIDGET->list_deletion_texture);
  WIDGET->list_deletion_texture=tex;
  WIDGET->list_deletion_listid=listid;
}

/* Request a list from the backend.
 * If it's empty, propose to delete it.
 */
 
static void mn_carousel_cb_list(struct eh_http_response *rsp,void *userdata) {
  struct gui_widget *widget=userdata;
  if (WIDGET->entryc) return; // got some other results since we started this, shouldn't happen but whatever, forget it
  struct sr_decoder decoder={.v=rsp->body,.c=rsp->bodyc};
  if (sr_decode_json_object_start(&decoder)<0) return;
  const char *k;
  int kc;
  int id=0,empty=0;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    if ((kc==5)&&!memcmp(k,"games",5)) {
      const char *v;
      int vc=sr_decode_json_expression(&v,&decoder);
      if ((vc==2)&&!memcmp(v,"[]",2)) {
        empty=1;
      }
    } else if ((kc==6)&&!memcmp(k,"listid",6)) {
      if (sr_decode_json_int(&id,&decoder)<0) return;
    } else {
      if (sr_decode_json_skip(&decoder)<0) return;
    }
  }
  if (empty&&id) carousel_propose_list_deletion(widget,id);
}
 
static void carousel_check_for_empty_list(struct gui_widget *widget) {
  char path[256];
  memcpy(path,"/api/list?listid=",17);
  int pathc=17;
  int err=sr_url_encode(path+pathc,sizeof(path)-pathc,mn.dbs.list,mn.dbs.listc);
  if ((err<0)||(pathc>=sizeof(path)-err)) return;
  pathc+=err;
  path[pathc]=0;
  int corrid=dbs_request_http(&mn.dbs,"GET",path,mn_carousel_cb_list,widget);
}

/* Replace game list from JSON.
 */
 
static int mn_carousel_decode_game_to_list(struct gui_widget *widget,struct sr_decoder *decoder) {
  int gameid=0;
  const char *scpath=0; // encoded json string
  int scpathc=0;
  int jsonctx=sr_decode_json_object_start(decoder);
  if (jsonctx<0) return -1;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,decoder))>0) {
    
    if ((kc==6)&&!memcmp(k,"gameid",6)) {
      if (sr_decode_json_int(&gameid,decoder)<0) return -1;
      continue;
    }
    
    if ((kc==5)&&!memcmp(k,"blobs",5)) {
      int subctx=sr_decode_json_array_start(decoder);
      if (subctx<0) return -1;
      while (sr_decode_json_next(0,decoder)>0) {
        const char *bpath=0;
        int bpathc=sr_decode_json_expression(&bpath,decoder);
        if ((bpathc>0)&&mn_carousel_encoded_path_looks_like_scap(bpath,bpathc)) {
          scpath=bpath;
          scpathc=bpathc;
        }
      }
      if (sr_decode_json_end(decoder,subctx)<0) return -1;
      continue;
    }
    
    if (sr_decode_json_skip(decoder)<0) return -1;
  }
  if (sr_decode_json_end(decoder,jsonctx)<0) return -1;
  if (!gameid) return 0;
  
  if (gameid==mn.dbs.gameid) {
    WIDGET->entryp=WIDGET->entryc;
  }
  
  struct mn_carousel_entry *entry=mn_carousel_append_entry(widget);
  if (!entry) return 0;
  entry->gameid=gameid;
  if (scpathc>0) {
    char tmp[1024];
    int tmpc=sr_string_eval(tmp,sizeof(tmp),scpath,scpathc);
    if ((tmpc>0)&&(tmpc<=sizeof(tmp))) {
      if (entry->scpath=malloc(tmpc+1)) {
        memcpy(entry->scpath,tmp,tmpc);
        entry->scpath[tmpc]=0;
      }
    }
  }
  
  return 0;
}
 
static int mn_carousel_replace_list(struct gui_widget *widget,const char *src,int srcc) {
  WIDGET->entryp=-1;
  while (WIDGET->entryc>0) {
    WIDGET->entryc--;
    mn_carousel_entry_cleanup(WIDGET->entryv+WIDGET->entryc);
  }
  
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_array_start(&decoder)<0) return -1;
  while (sr_decode_json_next(0,&decoder)>0) {
    if (mn_carousel_decode_game_to_list(widget,&decoder)<0) return -1;
  }
  if (sr_decode_json_end(&decoder,0)<0) return -1;
  
  gui_texture_del(WIDGET->list_deletion_texture);
  WIDGET->list_deletion_texture=0;
  WIDGET->list_deletion_listid=0;
  if (!WIDGET->entryc&&!mn.dbs.totalc) {
    if (mn.dbs.listc&&(mn.dbs.last_search_interaction==DB_SERVICE_INTERACTION_list)) {
      carousel_check_for_empty_list(widget);
    }
  }
  
  if (WIDGET->entryc>0) {
    if (WIDGET->autoselect_direction<0) WIDGET->entryp=0;
    else if (WIDGET->autoselect_direction>0) WIDGET->entryp=WIDGET->entryc-1;
    else if (WIDGET->entryp<0) WIDGET->entryp=0;//WIDGET->entryp=WIDGET->entryc>>1; // start at the left. could do center, but why...
    dbs_select_game(&mn.dbs,WIDGET->entryv[WIDGET->entryp].gameid);
  }
  WIDGET->autoselect_direction=0;
  mn_carousel_reset_entry_positions(widget);
  mn_carousel_refresh_page_indicators(widget);
  return 0;
}

/* Update.
 */
 
static void _carousel_update(struct gui_widget *widget) {

  if (WIDGET->gamelistseq!=mn.dbs.gamelistseq) {
    WIDGET->gamelistseq=mn.dbs.gamelistseq;
    if (mn_carousel_replace_list(widget,mn.dbs.gamelist,mn.dbs.gamelistc)<0) return;
  }

  if (!WIDGET->animation_stable) {
    WIDGET->animation_stable=1;
    const GLfloat xspeed=0.040f;
    const GLfloat zspeed=0.020f;
    const GLfloat tspeed=0.130f;
    struct mn_carousel_entry *entry=WIDGET->entryv;
    int i=WIDGET->entryc;
    for (;i-->0;entry++) {
      if (entry->x<entry->dstx) {
        if ((entry->x+=xspeed)>=entry->dstx) entry->x=entry->dstx;
        else WIDGET->animation_stable=0;
      } else if (entry->x>entry->dstx) {
        if ((entry->x-=xspeed)<=entry->dstx) entry->x=entry->dstx;
        else WIDGET->animation_stable=0;
      }
      if (entry->z<entry->dstz) {
        if ((entry->z+=zspeed)>=entry->dstz) entry->z=entry->dstz;
        else WIDGET->animation_stable=0;
      } else if (entry->z>entry->dstz) {
        if ((entry->z-=zspeed)<=entry->dstz) entry->z=entry->dstz;
        else WIDGET->animation_stable=0;
      }
      if (entry->t<entry->dstt) {
        if ((entry->t+=tspeed)>=entry->dstt) entry->t=entry->dstt;
        else WIDGET->animation_stable=0;
      } else if (entry->t>entry->dstt) {
        if ((entry->t-=tspeed)<=entry->dstt) entry->t=entry->dstt;
        else WIDGET->animation_stable=0;
      }
    }
  }
  
  const GLfloat alpha_speed=0.050f;
  if (WIDGET->alpha>WIDGET->alpha_target) {
    if ((WIDGET->alpha-=alpha_speed)<WIDGET->alpha_target) WIDGET->alpha=WIDGET->alpha_target;
  } else if (WIDGET->alpha<WIDGET->alpha_target) {
    if ((WIDGET->alpha+=alpha_speed)>WIDGET->alpha_target) WIDGET->alpha=WIDGET->alpha_target;
  }
}

/* Motion bits.
 */
 
static void mn_carousel_entryp_changed(struct gui_widget *widget) {
  mn_carousel_animate_entry_positions(widget);
  dbs_select_game(&mn.dbs,WIDGET->entryv[WIDGET->entryp].gameid);
  mn_cb_sound_effect(GUI_SFXID_MOTION,0);
}

static void mn_carousel_change_page(struct gui_widget *widget,int d) {
  if (d<0) {
    if (mn.dbs.page<=1) return;
    mn.dbs.page--;
    WIDGET->autoselect_direction=1;
    dbs_refresh_search(&mn.dbs);
    mn_cb_sound_effect(GUI_SFXID_MOTION,0);
  } else if (d>0) {
    if (mn.dbs.page>=mn.dbs.pagec) return;
    mn.dbs.page++;
    WIDGET->autoselect_direction=-1;
    dbs_refresh_search(&mn.dbs);
    mn_cb_sound_effect(GUI_SFXID_MOTION,0);
  }
}

/* Motion.
 */
 
static void _carousel_motion(struct gui_widget *widget,int dx,int dy) {
  if (dx<0) {
    if (WIDGET->entryp>0) {
      WIDGET->entryp--;
      mn_carousel_entryp_changed(widget);
    } else if (mn.dbs.page<=1) {
      mn_cb_sound_effect(GUI_SFXID_REJECT,0);
    } else {
      mn_carousel_change_page(widget,-1);
    }
  } else if (dx>0) {
    if (WIDGET->entryp<WIDGET->entryc-1) {
      WIDGET->entryp++;
      mn_carousel_entryp_changed(widget);
    } else if (mn.dbs.page>=mn.dbs.pagec) {
      mn_cb_sound_effect(GUI_SFXID_REJECT,0);
    } else {
      mn_carousel_change_page(widget,1);
    }
  }
}

/* Motion by pages (L1,R1)
 */
 
static void mn_carousel_step_page(struct gui_widget *widget,int d) {
  if (d<0) {
    if (mn.dbs.page<=1) {
      if (WIDGET->entryp<=0) {
        mn_cb_sound_effect(GUI_SFXID_REJECT,0);
      } else {
        WIDGET->entryp=0;
        mn_carousel_entryp_changed(widget);
      }
    } else {
      mn_carousel_change_page(widget,-1);
    }
  } else if (d>0) {
    if (mn.dbs.page>=mn.dbs.pagec) {
      if (WIDGET->entryp>=WIDGET->entryc-1) {
        mn_cb_sound_effect(GUI_SFXID_REJECT,0);
      } else {
        WIDGET->entryp=WIDGET->entryc-1;
        mn_carousel_entryp_changed(widget);
      }
    } else {
      mn_carousel_change_page(widget,1);
    }
  }
}

/* Activate: Launch game if one is selected.
 */
 
static void mn_carousel_activate(struct gui_widget *widget) {
  if (WIDGET->entryp<0) return;
  if (WIDGET->entryp>=WIDGET->entryc) return;
  int gameid=WIDGET->entryv[WIDGET->entryp].gameid;
  if (dbs_launch(&mn.dbs,gameid)<0) {
    fprintf(stderr,"Failed to send launch request for gameid %d\n",gameid);
  }
}

/* Cancel (B): Open details modal for selected game.
 */
 
static void mn_carousel_edit(struct gui_widget *widget) {

  if (!WIDGET->entryc&&WIDGET->list_deletion_texture&&WIDGET->list_deletion_listid) {
    char path[256];
    snprintf(path,sizeof(path),"/api/list?listid=%d",WIDGET->list_deletion_listid);
    dbs_request_http(&mn.dbs,"DELETE",path,0,0);
    gui_texture_del(WIDGET->list_deletion_texture);
    WIDGET->list_deletion_texture=0;
    WIDGET->list_deletion_listid=0;
    dbs_refresh_all_metadata(&mn.dbs);
    dbs_search_set_list(&mn.dbs,"",0);
    MN_SOUND(ACTIVATE)
    return;
  }

  if (WIDGET->entryp<0) return;
  if (WIDGET->entryp>=WIDGET->entryc) return;
  int gameid=WIDGET->entryv[WIDGET->entryp].gameid;
  struct gui_widget *modal=gui_push_modal(widget->gui,&mn_widget_type_edit);
  if (!modal) return;
  MN_SOUND(ACTIVATE)
  mn_widget_edit_setup(modal,gameid);
}

/* Signal.
 */
 
static void _carousel_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_ACTIVATE: mn_carousel_activate(widget); break;
    case GUI_SIGID_CANCEL: mn_carousel_edit(widget); break;
    case GUI_SIGID_FOCUS: WIDGET->alpha_target=1.0f; break;
    case GUI_SIGID_BLUR: WIDGET->alpha_target=0.100f; break;
    case GUI_SIGID_PAGELEFT: mn_carousel_step_page(widget,-1); break;
    case GUI_SIGID_PAGERIGHT: mn_carousel_step_page(widget,1); break;
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_carousel={
  .name="carousel",
  .objlen=sizeof(struct mn_widget_carousel),
  .del=_carousel_del,
  .init=_carousel_init,
  .pack=_carousel_pack,
  .draw=_carousel_draw,
  .update=_carousel_update,
  .motion=_carousel_motion,
  .signal=_carousel_signal,
};
