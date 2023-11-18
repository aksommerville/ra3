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
  struct gui_texture *texture_noscreencap;
  int gamelistseq;
  
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
      "dstposition.y/screensize.y+(aposition.y*dstsize.y)/screensize.y-0.100,\n"
      "adjz,\n"
      "adjz\n"
    ");\n"
    "vtexcoord=atexcoord;\n"
  "}\n"
"";

static const char carousel_fsrc[]=
  "uniform sampler2D sampler;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
    "gl_FragColor=texture2D(sampler,vtexcoord);\n"
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
  
  //XXX be less strict about this, and also don't require working directory!
  if (!(WIDGET->texture_noscreencap=gui_texture_new())) return -1;
  gui_texture_set_filter(WIDGET->texture_noscreencap,1);
  void *serial=0;
  int serialc=file_read(&serial,"src/menu/data/noscreencap.png");
  if (serialc<0) return -1;
  struct png_image *image=png_decode(serial,serialc);
  free(serial);
  if (!image) return -1;
  if ((image->depth!=8)||(image->colortype!=6)) return -1;
  gui_texture_upload_rgba(WIDGET->texture_noscreencap,image->w,image->h,image->pixels);
  png_image_del(image);
  
  WIDGET->animation_stable=1;
  
  return 0;
}

/* Measure.
 */
 
static void _carousel_measure(int *w,int *h,struct gui_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=400;//TODO
}

/* Pack.
 */
 
static void _carousel_pack(struct gui_widget *widget) {
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
  if (WIDGET->entryc<1) return;

  gui_program_use(WIDGET->program);
  int screenw,screenh;
  gui_get_screen_size(&screenw,&screenh,widget->gui);
  glViewport(x,screenh-widget->h-y,widget->w,widget->h);
  gui_program_set_screensize(WIDGET->program,widget->w,widget->h);
  gui_program_set_sampler(WIDGET->program,0);
  glUniform2f(WIDGET->loc_dstposition,x,y);
  glUniform2f(WIDGET->loc_dstsize,widget->w,widget->h);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_DST_ALPHA);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  
  //XXX Don't load an unlimited number of screencaps. And don't load them during render, and don't examine each of them each frame after, for ever.
  // I think move this to update, and pay out the loads over the first few updates?
  int ii=0; for (;ii<WIDGET->entryc;ii++) {
    struct mn_carousel_entry *entry=WIDGET->entryv+ii;
    if (entry->scpath&&!entry->scap) {
      if (!(entry->scap=gui_texture_new())) return;
      gui_texture_set_filter(entry->scap,1);
      void *serial=0;
      int serialc=file_read(&serial,entry->scpath);
      if (serialc<0) return;
      struct png_image *image=png_decode(serial,serialc);
      free(serial);
      if (!image) return;
      struct png_image *alt=png_image_reformat(image,0,0,0,0,8,6,1);
      png_image_del(image);
      image=alt;
      if (!image) return;
      gui_texture_upload_rgba(entry->scap,image->w,image->h,image->pixels);
      png_image_del(image);
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
  
  if (WIDGET->entryc>0) {
    if (WIDGET->autoselect_direction<0) WIDGET->entryp=0;
    else if (WIDGET->autoselect_direction>0) WIDGET->entryp=WIDGET->entryc-1;
    else if (WIDGET->entryp<0) WIDGET->entryp=WIDGET->entryc>>1;
    dbs_select_game(&mn.dbs,WIDGET->entryv[WIDGET->entryp].gameid);
  }
  WIDGET->autoselect_direction=0;
  mn_carousel_reset_entry_positions(widget);
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
}

/* Motion.
 */
 
static void _carousel_motion(struct gui_widget *widget,int dx,int dy) {
  if (dx<0) {
    if (WIDGET->entryp>0) {
      WIDGET->entryp--;
      mn_carousel_animate_entry_positions(widget);
      dbs_select_game(&mn.dbs,WIDGET->entryv[WIDGET->entryp].gameid);
    } else {
      //TODO load previous page and autoselect_direction=1
    }
  } else if (dx>0) {
    if (WIDGET->entryp<WIDGET->entryc-1) {
      WIDGET->entryp++;
      mn_carousel_animate_entry_positions(widget);
      dbs_select_game(&mn.dbs,WIDGET->entryv[WIDGET->entryp].gameid);
    } else {
      //TODO load next page and autoselect_direction=-1
    }
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_carousel={
  .name="carousel",
  .objlen=sizeof(struct mn_widget_carousel),
  .del=_carousel_del,
  .init=_carousel_init,
  .measure=_carousel_measure,
  .pack=_carousel_pack,
  .draw=_carousel_draw,
  .update=_carousel_update,
  .motion=_carousel_motion,
};
