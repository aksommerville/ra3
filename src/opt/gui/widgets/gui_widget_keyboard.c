#include "../gui_internal.h"
#include "lib/emuhost.h"

/* Object definition.
 */
 
struct gui_widget_keyboard {
  struct gui_widget hdr;
  void (*cb)(struct gui_widget *widget,int codepoint,void *userdata);
  void *userdata;
  struct gui_keyboard_button {
    int x,y,w,h;
    const char *natural,*shifted;
    int naturalc,shiftedc;
    int ncodepoint,scodepoint;
  } *buttonv;
  int buttonc,buttona;
  int bx,by,bw,bh; // size and position of the buttons grid, also size of the texture
  struct gui_texture *capstex; // all keycap labels in one texture, our full size
  struct gui_font *font; // WEAK
  int focusp;
  int shifted;
  uint16_t pvinput;
};

#define WIDGET ((struct gui_widget_keyboard*)widget)

/* Cleanup.
 */
 
static void _keyboard_del(struct gui_widget *widget) {
  gui_texture_del(WIDGET->capstex);
}

/* Add buttons.
 * During the construction process, we abuse (bx,by) as current position.
 * Add with codepoint zero to infer from the label.
 */
 
static struct gui_keyboard_button *keyboard_add_button(
  struct gui_widget *widget,
  const char *natural,
  const char *shifted,
  int ncodepoint,
  int scodepoint
) {
  if (WIDGET->buttonc>=WIDGET->buttona) {
    int na=WIDGET->buttona+32;
    if (na>INT_MAX/sizeof(struct gui_keyboard_button)) return 0;
    void *nv=realloc(WIDGET->buttonv,sizeof(struct gui_keyboard_button)*na);
    if (!nv) return 0;
    WIDGET->buttonv=nv;
    WIDGET->buttona=na;
  }
  if (natural&&!ncodepoint) ncodepoint=*natural;
  if (shifted&&!scodepoint) scodepoint=*shifted;
  struct gui_keyboard_button *button=WIDGET->buttonv+WIDGET->buttonc++;
  memset(button,0,sizeof(struct gui_keyboard_button));
  button->x=WIDGET->bx;
  button->y=WIDGET->by;
  if (button->natural=natural) while (button->natural[button->naturalc]) button->naturalc++;
  if (button->shifted=shifted) while (button->shifted[button->shiftedc]) button->shiftedc++;
  button->w=(button->naturalc>button->shiftedc)?button->naturalc:button->shiftedc;
  button->w*=WIDGET->font->charw;
  button->h=WIDGET->font->lineh;
  button->ncodepoint=ncodepoint;
  button->scodepoint=scodepoint;
  WIDGET->bx+=button->w;
  if (button->x+button->w>WIDGET->bw) WIDGET->bw=button->x+button->w;
  if (button->y+button->h>WIDGET->bh) WIDGET->bh=button->y+button->h;
  return button;
}

static void keyboard_add_button_margin(struct gui_widget *widget,int chc) {
  WIDGET->bx+=chc*WIDGET->font->charw;
}

static void keyboard_end_button_row(struct gui_widget *widget) {
  WIDGET->bx=0;
  WIDGET->by=WIDGET->bh;
}

/* Draw capstex.
 */
 
static int keyboard_refresh_capstex(struct gui_widget *widget) {
  if (!WIDGET->capstex) {
    if (!(WIDGET->capstex=gui_texture_new())) return -1;
  }
  if ((WIDGET->bw<1)||(WIDGET->bh<1)) return -1;
  
  int stride=WIDGET->bw<<2;
  uint8_t *rgba=malloc(stride*WIDGET->bh);
  if (!rgba) return -1;
  uint8_t *p=rgba;
  int i=WIDGET->bw*WIDGET->bh;
  for (;i-->0;p+=4) { p[0]=p[1]=p[2]=0xff; p[3]=0x00; }
  
  const struct gui_keyboard_button *button=WIDGET->buttonv;
  for (i=WIDGET->buttonc;i-->0;button++) {
    int dstx=button->x;
    int dsty=button->y;
    const char *src=button->natural;
    int srcc=button->naturalc;
    if (WIDGET->shifted) {
      src=button->shifted;
      srcc=button->shiftedc;
    }
    gui_font_render_line(
      rgba+dsty*stride+(dstx<<2),
      WIDGET->bw-dstx,WIDGET->bh-dsty,stride,
      WIDGET->font,src,srcc,0xffffff
    );
  }
  
  gui_texture_upload_rgba(WIDGET->capstex,WIDGET->bw,WIDGET->bh,rgba);
  
  free(rgba);
  return 0;
}

/* Init.
 */
 
static int _keyboard_init(struct gui_widget *widget) {

  if (!(WIDGET->font=gui_font_get(widget->gui,"giantmono",9))) return -1;
  
  keyboard_add_button(widget,"`","~",0,0);
  keyboard_add_button(widget,"1","!",0,0);
  keyboard_add_button(widget,"2","@",0,0);
  keyboard_add_button(widget,"3","#",0,0);
  keyboard_add_button(widget,"4","$",0,0);
  keyboard_add_button(widget,"5","%",0,0);
  keyboard_add_button(widget,"6","^",0,0);
  keyboard_add_button(widget,"7","&",0,0);
  keyboard_add_button(widget,"8","*",0,0);
  keyboard_add_button(widget,"9","(",0,0);
  keyboard_add_button(widget,"0",")",0,0);
  keyboard_add_button(widget,"-","_",0,0);
  keyboard_add_button(widget,"=","+",0,0);
  keyboard_end_button_row(widget);
  
  keyboard_add_button(widget,"q","Q",0,0);
  keyboard_add_button(widget,"w","W",0,0);
  keyboard_add_button(widget,"e","E",0,0);
  keyboard_add_button(widget,"r","R",0,0);
  keyboard_add_button(widget,"t","T",0,0);
  keyboard_add_button(widget,"y","Y",0,0);
  keyboard_add_button(widget,"u","U",0,0);
  keyboard_add_button(widget,"i","I",0,0);
  keyboard_add_button(widget,"o","O",0,0);
  keyboard_add_button(widget,"p","P",0,0);
  keyboard_add_button(widget,"[","{",0,0);
  keyboard_add_button(widget,"]","}",0,0);
  keyboard_add_button(widget,"\\","|",0,0);
  keyboard_end_button_row(widget);
  
  keyboard_add_button(widget,"a","A",0,0);
  keyboard_add_button(widget,"s","S",0,0);
  keyboard_add_button(widget,"d","D",0,0);
  keyboard_add_button(widget,"f","F",0,0);
  keyboard_add_button(widget,"g","G",0,0);
  keyboard_add_button(widget,"h","H",0,0);
  keyboard_add_button(widget,"j","J",0,0);
  keyboard_add_button(widget,"k","K",0,0);
  keyboard_add_button(widget,"l","L",0,0);
  keyboard_add_button(widget,";",":",0,0);
  keyboard_add_button(widget,"'","\"",0,0);
  keyboard_add_button_margin(widget,1);
  keyboard_add_button(widget,"OK","OK",0x0a,0x0a);
  WIDGET->focusp=WIDGET->buttonc-1;
  keyboard_end_button_row(widget);
  
  keyboard_add_button(widget,"z","Z",0,0);
  keyboard_add_button(widget,"x","X",0,0);
  keyboard_add_button(widget,"c","C",0,0);
  keyboard_add_button(widget,"v","V",0,0);
  keyboard_add_button(widget,"b","B",0,0);
  keyboard_add_button(widget,"n","N",0,0);
  keyboard_add_button(widget,"m","M",0,0);
  keyboard_add_button(widget,",","<",0,0);
  keyboard_add_button(widget,".",">",0,0);
  keyboard_add_button(widget,"/","?",0,0);
  keyboard_add_button_margin(widget,1);
  keyboard_add_button(widget,"UPR","lwr",0x01,0x01);
  keyboard_end_button_row(widget);
  
  keyboard_add_button(widget,"spc","spc",0x20,0x20);
  keyboard_add_button_margin(widget,1);
  keyboard_add_button(widget,"Cancel","Cancel",0x02,0x02);
  keyboard_add_button_margin(widget,1);
  keyboard_add_button(widget,"<<<","<<<",0x08,0x08);
  keyboard_end_button_row(widget);
  
  WIDGET->bx=0;
  WIDGET->by=0;
  
  if (keyboard_refresh_capstex(widget)<0) return -1;
  
  return 0;
}

/* Measure.
 */
 
static void _keyboard_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  *w=*h=0;
  const struct gui_keyboard_button *button=WIDGET->buttonv;
  int i=WIDGET->buttonc;
  for (;i-->0;button++) {
    int right=button->x+button->w;
    if (right>*w) *w=right;
    int bottom=button->y+button->h;
    if (bottom>*h) *h=bottom;
  }
}

/* Pack.
 */
 
static void _keyboard_pack(struct gui_widget *widget) {
  WIDGET->bx=(widget->w>>1)-(WIDGET->bw>>1);
  WIDGET->by=(widget->h>>1)-(WIDGET->bh>>1);
}

/* Draw.
 */
 
static void _keyboard_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x505050ff);
  if ((WIDGET->focusp>=0)&&(WIDGET->focusp<WIDGET->buttonc)) {
    const struct gui_keyboard_button *focus=WIDGET->buttonv+WIDGET->focusp;
    int dstx=x+WIDGET->bx+focus->x;
    int dsty=y+WIDGET->by+focus->y;
    int dstw=focus->w;
    int dsth=focus->h;
    dstx-=WIDGET->font->charw>>2;
    dstw+=WIDGET->font->charw>>1;
    gui_draw_rect(widget->gui,dstx,dsty,dstw,dsth,0x002030ff);
  }
  gui_draw_texture(widget->gui,x+WIDGET->bx,y+WIDGET->by,WIDGET->capstex);
}

/* Backspace. (via GUI_SIGID_CANCEL)
 */
 
static void keyboard_backspace(struct gui_widget *widget) {
  if (WIDGET->cb) WIDGET->cb(widget,0x08,WIDGET->userdata);
}

/* Activate focussed button.
 */
 
static void keyboard_press(struct gui_widget *widget) {
  if ((WIDGET->focusp<0)||(WIDGET->focusp>=WIDGET->buttonc)) return;
  const struct gui_keyboard_button *focus=WIDGET->buttonv+WIDGET->focusp;
  int codepoint=WIDGET->shifted?focus->scodepoint:focus->ncodepoint;
  if (!codepoint) return;
  switch (codepoint) {
    case 0x01: {
        WIDGET->shifted=WIDGET->shifted?0:1;
        keyboard_refresh_capstex(widget);
      } break;
    case 0x02: if (WIDGET->cb) WIDGET->cb(widget,0,WIDGET->userdata); break;
    default: if (WIDGET->cb) WIDGET->cb(widget,codepoint,WIDGET->userdata);
  }
}

/* Set default focus. Shouldn't ever happen.
 */
 
static void keyboard_focus_default(struct gui_widget *widget) {
  if (WIDGET->buttonc<1) return;
  WIDGET->focusp=0;
}

/* Find a button relative to some other.
 */
 
static struct gui_keyboard_button *keyboard_find_neighbor(
  struct gui_widget *widget,
  struct gui_keyboard_button *from,
  int boundx,int boundw,int boundy,int boundh, // only consider buttons intersecting this
  int condx,int condy, // normal vector: (1,0) means look for the highest X value
  int param // limit for that condition
) {
  int bestscore=0,bestoobscore=0;
  struct gui_keyboard_button *best=0,*bestoob=0;
  struct gui_keyboard_button *q=WIDGET->buttonv;
  int i=WIDGET->buttonc;
  for (;i-->0;q++) {
    if (q==from) continue;
    
    if (q->x>=boundx+boundw) continue;
    if (q->y>=boundy+boundh) continue;
    if (q->x+q->w<=boundx) continue;
    if (q->y+q->h<=boundy) continue;
    
    int score=0,oob=0;
    if (condx>0) {
      if (q->x>=param) oob=1;
      score=q->x+1;
    } else if (condx<0) {
      if (q->x<=param) oob=1;
      score=WIDGET->bw-q->x+1;
    } else if (condy>0) {
      if (q->y>=param) oob=1;
      score=q->y+1;
    } else if (condy<0) {
      if (q->y<=param) oob=1;
      score=WIDGET->bh-q->y+1;
    }
    if (oob) {
      if (score<=bestoobscore) continue;
      bestoob=q;
      bestoobscore=score;
    } else {
      if (score<=bestscore) continue;
      best=q;
      bestscore=score;
    }
  }
  return best?best:bestoob;
}

/* Motion.
 */
 
static void _keyboard_motion(struct gui_widget *widget,int dx,int dy) {
  
  if ((WIDGET->focusp<0)||(WIDGET->focusp>=WIDGET->buttonc)) {
    keyboard_focus_default(widget);
    return;
  }
  struct gui_keyboard_button *focus=WIDGET->buttonv+WIDGET->focusp;
  struct gui_keyboard_button *next=0;
       if (dx<0) next=keyboard_find_neighbor(widget,focus,0,WIDGET->bw,focus->y,focus->h,1,0,focus->x);
  else if (dx>0) next=keyboard_find_neighbor(widget,focus,0,WIDGET->bw,focus->y,focus->h,-1,0,focus->x);
  else if (dy<0) next=keyboard_find_neighbor(widget,focus,focus->x,focus->w,0,WIDGET->bh,0,1,focus->y);
  else if (dy>0) next=keyboard_find_neighbor(widget,focus,focus->x,focus->w,0,WIDGET->bh,0,-1,focus->y);
  if (!next) return;
  GUI_SOUND(MOTION)
  
  WIDGET->focusp=next-WIDGET->buttonv;
}

/* Signal.
 */
 
static void _keyboard_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_ACTIVATE: keyboard_press(widget); break;
    case GUI_SIGID_CANCEL: keyboard_backspace(widget); break;
    case GUI_SIGID_FOCUS: break;//TODO
    case GUI_SIGID_BLUR: break;//TODO
  }
}

/* Update.
 */
 
static void _keyboard_update(struct gui_widget *widget) {
  if (WIDGET->pvinput!=widget->gui->pvinput) {
    if (widget->gui->pvinput&EH_BTN_L1) {
      if (!WIDGET->shifted) {
        WIDGET->shifted=1;
        keyboard_refresh_capstex(widget);
      }
    } else {
      if (WIDGET->shifted) {
        WIDGET->shifted=0;
        keyboard_refresh_capstex(widget);
      }
    }
    if ((widget->gui->pvinput&EH_BTN_AUX1)&&!(WIDGET->pvinput&EH_BTN_AUX1)) {
      if (WIDGET->cb) WIDGET->cb(widget,0x0a,WIDGET->userdata);
    }
    WIDGET->pvinput=widget->gui->pvinput;
  }
}

/* Type definition.
 */
 
const struct gui_widget_type gui_widget_type_keyboard={
  .name="keyboard",
  .objlen=sizeof(struct gui_widget_keyboard),
  .del=_keyboard_del,
  .init=_keyboard_init,
  .measure=_keyboard_measure,
  .pack=_keyboard_pack,
  .draw=_keyboard_draw,
  .motion=_keyboard_motion,
  .signal=_keyboard_signal,
  .update=_keyboard_update,
};

/* Public accessors.
 */
 
void gui_widget_keyboard_set_callback(struct gui_widget *widget,void (*cb)(struct gui_widget *keyboard,int codepoint,void *userdata),void *userdata) {
  if (!widget||(widget->type!=&gui_widget_type_keyboard)) return;
  WIDGET->cb=cb;
  WIDGET->userdata=userdata;
}
