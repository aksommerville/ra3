#include "../mn_internal.h"
#include "opt/png/png.h"
#include "opt/fs/fs.h"

// Don't reread the PNG file each time we instantiate. (but do make a new texture, to be safe)
static struct png_image *mn_flags_image=0;
static int mn_flags_colw=32;
static int mn_flags_rowh=32;
static int mn_flags_colc=0;

/* Object definition.
 */
 
struct mn_widget_flags {
  struct gui_widget hdr;
  int focus;
  void (*cb)(struct gui_widget *flags,const char *v,int c,void *userdata);
  void *userdata;
  struct gui_texture *tex;
  uint32_t v;
  int focusp;
};

#define WIDGET ((struct mn_widget_flags*)widget)

/* Cleanup.
 */
 
static void _flags_del(struct gui_widget *widget) {
  gui_texture_del(WIDGET->tex);
}

/* Init.
 */
 
static int _flags_init(struct gui_widget *widget) {
  
  if (!mn_flags_image) {
    void *serial=0;
    int serialc=file_read(&serial,mn_data_path("realflags.png"));
    if (serialc<0) return -1;
    mn_flags_image=png_decode(serial,serialc);
    free(serial);
    if (!mn_flags_image) return -1;
    if ((mn_flags_image->depth!=8)||(mn_flags_image->colortype!=6)) {
      png_image_del(mn_flags_image);
      mn_flags_image=0;
      return -1;
    }
    mn_flags_colc=mn_flags_image->w/mn_flags_colw;
  }
  
  if (!(WIDGET->tex=gui_texture_new())) return -1;
  gui_texture_upload_rgba(WIDGET->tex,mn_flags_image->w,mn_flags_image->h,mn_flags_image->pixels);
  
  return 0;
}

/* Measure.
 */
 
static void _flags_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  gui_texture_get_size(w,h,WIDGET->tex);
}

/* Draw.
 */
 
static void _flags_draw(struct gui_widget *widget,int x,int y) {
  uint32_t mask=1;
  int i=0;
  for (;i<mn_flags_colc;i++,mask<<=1) {
    if (!(WIDGET->v&mask)) continue;
    int dstx=x+i*mn_flags_colw;
    int dsty=y;
    gui_draw_rect(widget->gui,dstx,dsty,mn_flags_colw,mn_flags_rowh,0x000000ff);
  }
  gui_draw_texture(widget->gui,x,y,WIDGET->tex);
  if (WIDGET->focus) {
    int dstx=x+WIDGET->focusp*mn_flags_colw;
    int dsty=y;
    gui_frame_rect(widget->gui,dstx,dsty,mn_flags_colw,mn_flags_rowh,0xffff00ff);
  }
}

/* Bits to/from text.
 */
 
static uint32_t flags_eval(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if ((srcc>=2)&&(src[0]=='"')&&(src[srcc-1]=='"')) {
    // We'll usually receive them as a JSON-encoded string of space-delimited tokens.
    // The tokens are derived originally from cpp macro names, so there's no chance they would contain any escapes.
    src++;
    srcc-=2;
  }
  uint32_t flags=0;
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
    
    //TODO There is a flag names service from the backend, we ought to use it.
         if ((tokenc==7)&&!memcmp(token,"player1",7)) flags|=0x0001;
    else if ((tokenc==7)&&!memcmp(token,"player2",7)) flags|=0x0002;
    else if ((tokenc==7)&&!memcmp(token,"player3",7)) flags|=0x0004;
    else if ((tokenc==7)&&!memcmp(token,"player4",7)) flags|=0x0008;
    else if ((tokenc==10)&&!memcmp(token,"playermore",10)) flags|=0x0010;
    else if ((tokenc==6)&&!memcmp(token,"faulty",6)) flags|=0x0020;
    else if ((tokenc==7)&&!memcmp(token,"foreign",7)) flags|=0x0040;
    else if ((tokenc==4)&&!memcmp(token,"hack",4)) flags|=0x0080;
    else if ((tokenc==8)&&!memcmp(token,"hardware",8)) flags|=0x0100;
    else if ((tokenc==6)&&!memcmp(token,"review",6)) flags|=0x0200;
    else if ((tokenc==7)&&!memcmp(token,"obscene",7)) flags|=0x0400;
    else if ((tokenc==8)&&!memcmp(token,"favorite",8)) flags|=0x0800;
    else if ((tokenc==8)&&!memcmp(token,"seeother",8)) flags|=0x1000;
  }
  return flags;
}

static int flags_repr(char *dst,int dsta,uint32_t flags) {
  int dstc=0;
  const char *names[32]={
    "player1","player2","player3","player4","playermore","faulty","foreign","hack","hardware","review","obscene","favorite",
    "seeother","13","14","15","16","17","18","19",
    "20","21","22","23","24","25","26","27","28","29",
    "30","31",
  };
  const char **name=names;
  for (;flags;flags>>=1,name++) {
    if (flags&1) {
      if (dstc) {
        if (dstc<dsta) dst[dstc]=' ';
        dstc++;
      }
      int srcc=0;
      while ((*name)[srcc]) srcc++;
      if (dstc<=dsta-srcc) memcpy(dst+dstc,*name,srcc);
      dstc+=srcc;
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Activate.
 */
 
static void flags_activate(struct gui_widget *widget) {
  WIDGET->v^=1<<WIDGET->focusp;
  if (!WIDGET->cb) return;
  char tmp[256];
  int tmpc=flags_repr(tmp,sizeof(tmp),WIDGET->v);
  if ((tmpc<0)||(tmpc>sizeof(tmp))) return;
  WIDGET->cb(widget,tmp,tmpc,WIDGET->userdata);
}

/* Motion.
 */
 
static void _flags_motion(struct gui_widget *widget,int dx,int dy) {
  if (mn_flags_colc<1) return;
  MN_SOUND(MOTION)
  WIDGET->focusp+=dx;
  if (WIDGET->focusp<0) WIDGET->focusp=mn_flags_colc-1;
  else if (WIDGET->focusp>=mn_flags_colc) WIDGET->focusp=0;
}

/* Signal.
 */
 
static void _flags_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_ACTIVATE: flags_activate(widget); break;
    case GUI_SIGID_BLUR: WIDGET->focus=0; break;
    case GUI_SIGID_FOCUS: WIDGET->focus=1; break;
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_flags={
  .name="flags",
  .objlen=sizeof(struct mn_widget_flags),
  .del=_flags_del,
  .init=_flags_init,
  .measure=_flags_measure,
  .draw=_flags_draw,
  .motion=_flags_motion,
  .signal=_flags_signal,
};

/* Public setup.
 */
 
int mn_widget_flags_setup(
  struct gui_widget *widget,
  void (*cb)(struct gui_widget *flags,const char *v,int c,void *userdata),
  void *userdata,
  const char *v,int c
) {
  if (!widget||(widget->type!=&mn_widget_type_flags)) return -1;
  WIDGET->cb=cb;
  WIDGET->userdata=userdata;
  WIDGET->v=flags_eval(v,c);
  return 0;
}
