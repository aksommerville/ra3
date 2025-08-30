#include "../mn_internal.h"
#include "lib/emuhost.h"
#include "lib/eh_driver.h"
#include "lib/inmgr/inmgr.h"
#include "opt/gui/gui_font.h"
#include <math.h>

#define INDEV_BUTTON_LIMIT 200
#define INDEV_MIN_COLW 16 /* Minimum column width in characters. */
#define INDEV_HIGHLIGHT_LIMIT 8
#define INDEV_HIGHLIGHT_TIME 60

/* Object definition.
 */
 
struct mn_widget_indev {
  struct gui_widget hdr;
  int devid;
  int listenerid;
  struct gui_texture *nametex;
  int namex,namey;
  struct gui_font *font; // WEAK
  struct indev_cell { // Column-major, unlike our usual row-major ordering.
    uint32_t btnid;
    int value;
    int dstbtnid;
    int hidusage,srclo,srchi; // Recorded from caps; we'll need them if we change the mapping.
  } *cellv;
  int cellc,cella;
  int colc,rowc; // Established at pack. The last column can be incomplete.
  int colw,rowh;
  int gridx,gridy; // Origin of the grid, relative to widget.
  int margin;
  struct gui_texture *gridtex;
  int selx,sely;
  struct indev_highlight {
    int col,row,ttl;
  } highlightv[INDEV_HIGHLIGHT_LIMIT];
};

#define WIDGET ((struct mn_widget_indev*)widget)

/* Cleanup.
 */
 
static void _indev_del(struct gui_widget *widget) {
  inmgr_unlisten(WIDGET->listenerid);
  inmgr_device_enable(WIDGET->devid,1);
  gui_texture_del(WIDGET->nametex);
  gui_texture_del(WIDGET->gridtex);
  if (WIDGET->cellv) free(WIDGET->cellv);
}

/* Init.
 */
 
static int _indev_init(struct gui_widget *widget) {

  WIDGET->margin=10;

  /* We need a very small font because we want to show every button, without scrolling.
   * Keyboards have 104 buttons.
   * A font with 16x24-pixel glyphs seems to fit pretty nice.
   */
  if (!(WIDGET->font=gui_font_get(widget->gui,"vintage24",-1))) return -1;
  if (!WIDGET->font->charw) return -1; // Whatever font we use, it must be monospace.
  
  if (!(WIDGET->gridtex=gui_texture_new())) return -1;

  return 0;
}

/* Measure.
 */
 
static void _indev_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  *w=*h=0;
  gui_texture_get_size(w,h,WIDGET->nametex);
  
  /* Guess grid size.
   */
  if (WIDGET->cellc>0) {
    int wavail=wlimit-*w-(WIDGET->margin<<1);
    int havail=hlimit-*h-(WIDGET->margin*3);
    int colcavail=wavail/(WIDGET->font->charw*INDEV_MIN_COLW);
    int rowcavail=havail/WIDGET->font->lineh;
    int cellcavail=colcavail*rowcavail;
    if (cellcavail<WIDGET->cellc) {
      // Not enough room for the grid. Panic and request the entire screen.
      *w=wlimit;
      *h=hlimit;
      return;
    }
    // Drop as many columns as we can. Fewer columns is better, cells like to expand to fill their width.
    // This doesn't need to be iterative, I'm just not as good at math as I think I am.
    while (colcavail>1) {
      int qcellc=(colcavail-1)*rowcavail;
      if (qcellc<WIDGET->cellc) break;
      colcavail--;
      cellcavail=colcavail*rowcavail;
    }
    // Now drop rows the same way. But this time I am smart enough to not iterate...
    rowcavail=(WIDGET->cellc+colcavail-1)/colcavail;
    int gridw=colcavail*INDEV_MIN_COLW*WIDGET->font->charw;
    int gridh=rowcavail*WIDGET->font->lineh;
    if (gridw>*w) *w=gridw;
    (*h)+=gridh;
    (*h)+=WIDGET->margin;
  }
  
  // Outer margins.
  (*w)+=WIDGET->margin<<1;
  (*h)+=WIDGET->margin<<1;
}

/* Draw one cell image into an RGBA buffer, must be large enough (WIDGET->colw,WIDGET->rowh).
 * Caller should clear it first.
 */
 
static void indev_draw_cell(uint8_t *dst,int dststride,struct gui_widget *widget,struct indev_cell *cell) {
  int clampvalue=cell->value;
  if (clampvalue<-99) clampvalue=-99;
  else if (clampvalue>999) clampvalue=999;
  char text[64];
  int textc=snprintf(text,sizeof(text),"%3d %04x ",clampvalue,cell->btnid);
  if ((textc<0)||(textc>=sizeof(text))) return;
  int err=inmgr_btnid_repr(text+textc,sizeof(text)-textc,cell->dstbtnid);
  if ((err>0)&&(textc<=sizeof(text)-err)) textc+=err;
  gui_font_render_line(dst,WIDGET->colw,WIDGET->rowh,dststride,WIDGET->font,text,textc,0xffffff);
}

/* Draw and upload one cell's worth of gridtex.
 */
 
static void indev_redraw_gridtex_cell(struct gui_widget *widget,int col,int row) {
  if ((col<0)||(col>=WIDGET->colc)) return;
  if ((row<0)||(row>=WIDGET->rowc)) return;
  int cellp=col*WIDGET->rowc+row;
  if (cellp>=WIDGET->cellc) return;
  struct indev_cell *cell=WIDGET->cellv+cellp;
  int stride=WIDGET->colw*4;
  uint8_t *rgba=calloc(stride,WIDGET->rowh);
  if (!rgba) return;
  indev_draw_cell(rgba,stride,widget,cell);
  int x=col*WIDGET->colw;
  int y=row*WIDGET->rowh;
  gui_texture_upload_rgba_partial(WIDGET->gridtex,x,y,WIDGET->colw,WIDGET->rowh,rgba);
  free(rgba);
}

/* Draw the grid texture from scratch.
 */
 
static void indev_redraw_gridtex(struct gui_widget *widget) {
  int w=WIDGET->colc*WIDGET->colw;
  int h=WIDGET->rowc*WIDGET->rowh;
  if ((w<1)||(h<1)) return;
  int stride=w*4;
  uint8_t *rgba=calloc(w*h,4);
  if (!rgba) return;
  
  int x=0,y=0,row=0;
  struct indev_cell *cell=WIDGET->cellv;
  int i=WIDGET->cellc;
  for (;i-->0;cell++) {
    indev_draw_cell(rgba+stride*y+x*4,stride,widget,cell);
    y+=WIDGET->rowh;
    row++;
    if (row>=WIDGET->rowc) {
      row=0;
      y=0;
      x+=WIDGET->colw;
    }
  }
  
  gui_texture_upload_rgba(WIDGET->gridtex,w,h,rgba);
  free(rgba);
}

/* Pack.
 */
 
static void _indev_pack(struct gui_widget *widget) {

  // Name gets centered at the top.
  int namew=0,nameh=0;
  gui_texture_get_size(&namew,&nameh,WIDGET->nametex);
  WIDGET->namex=(widget->w>>1)-(namew>>1);
  WIDGET->namey=WIDGET->margin;
  
  if (WIDGET->cellc<1) return;
  
  /* Establish cell and grid size.
   * We do some complicated and fuzzy mathing during measure, but here it's more concrete.
   * Use as many rows as fit with the font's line height, minimum 1.
   * Then make columns as wide as they can to fill the available width.
   */
  WIDGET->gridx=WIDGET->margin;
  WIDGET->gridy=WIDGET->margin*2+nameh;
  WIDGET->rowh=WIDGET->font->lineh;
  WIDGET->rowc=(widget->h-WIDGET->margin-WIDGET->gridy)/WIDGET->rowh;
  if (WIDGET->rowc<1) WIDGET->rowc=1;
  WIDGET->colc=(WIDGET->cellc+WIDGET->rowc-1)/WIDGET->rowc;
  WIDGET->colw=(widget->w-(WIDGET->margin<<1))/WIDGET->colc;
  if (WIDGET->colw<1) WIDGET->colw=1;
  // No need to visit each cell, they are a uniform grid.
  
  indev_redraw_gridtex(widget);
}

/* Draw.
 */
 
static void _indev_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x005020ff);
  gui_draw_texture(widget->gui,x+WIDGET->namex,y+WIDGET->namey,WIDGET->nametex);
  struct indev_highlight *highlight=WIDGET->highlightv;
  int i=INDEV_HIGHLIGHT_LIMIT;
  for (;i-->0;highlight++) {
    if (highlight->ttl<=0) continue;
    highlight->ttl--;
    uint32_t rgba=0x00608000|((highlight->ttl*0xff)/INDEV_HIGHLIGHT_TIME); // don't go super light; white text should remain legible
    gui_draw_rect(widget->gui,x+WIDGET->gridx+highlight->col*WIDGET->colw,y+WIDGET->gridy+highlight->row*WIDGET->rowh,WIDGET->colw,WIDGET->rowh,rgba);
  }
  gui_draw_texture(widget->gui,x+WIDGET->gridx,y+WIDGET->gridy,WIDGET->gridtex);
  gui_frame_rect(widget->gui,x+WIDGET->gridx+WIDGET->selx*WIDGET->colw,y+WIDGET->gridy+WIDGET->sely*WIDGET->rowh,WIDGET->colw,WIDGET->rowh,0xffff00ff);
}

/* Search cells by btnid.
 */
 
static int indev_cellv_search(const struct gui_widget *widget,int btnid) {
  int lo=0,hi=WIDGET->cellc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct indev_cell *cell=WIDGET->cellv+ck;
         if (btnid<cell->btnid) hi=ck;
    else if (btnid>cell->btnid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Button list for modal.
 * Sorted for display, not sorted by (btnid).
 */
 
static const struct indev_option {
  int btnid;
  const char *name; // We could ask inmgr for this on the fly, but what the heck, maybe we'll want slightly different formatting.
} indev_optionv[]={
  #define _(tag) {EH_BTN_##tag,#tag},
  _(DPAD) _(HORZ) _(VERT)
  _(LEFT) _(RIGHT) _(UP) _(DOWN)
  _(SOUTH) _(WEST) _(EAST) _(NORTH)
  _(L1) _(R1) _(L2) _(R2)
  _(AUX1) _(AUX2) _(AUX3)
  _(QUIT) _(FULLSCREEN) _(MUTE) _(PAUSE) _(SCREENCAP)
  _(SAVESTATE) _(LOADSTATE) _(MENU) _(RESET)
  _(DEBUG) _(STEP) _(FASTFWD)
  #undef _
};

/* Commit edit.
 */
 
static void indev_cb_map(struct gui_widget *pickone,int p,void *userdata) {
  struct gui_widget *widget=userdata;
  int cellp=WIDGET->selx*WIDGET->rowc+WIDGET->sely;
  if ((cellp>=0)&&(cellp<WIDGET->cellc)) {
    struct indev_cell *cell=WIDGET->cellv+cellp;
    int optionc=sizeof(indev_optionv)/sizeof(indev_optionv[0]);
    if ((p>=0)&&(p<optionc)) {
      int dstbtnid=indev_optionv[p].btnid;
      if (dstbtnid!=cell->dstbtnid) { // NB zero is perfectly valid for dstbtnid, but it's also the fallback
        MN_SOUND(ACTIVATE)
        if (inmgr_remap_button(WIDGET->devid,cell->btnid,dstbtnid)>=0) {
          eh_inmgr_dirty();
          cell->dstbtnid=dstbtnid;
          indev_redraw_gridtex_cell(widget,WIDGET->selx,WIDGET->sely);
        }
      }
    }
  }
  gui_dismiss_modal(widget->gui,pickone);
}

/* Begin edit.
 */
 
static void indev_edit_selected_cell(struct gui_widget *widget) {
  int selp=WIDGET->selx*WIDGET->rowc+WIDGET->sely;
  if ((selp<0)||(selp>=WIDGET->cellc)) return;
  struct indev_cell *cell=WIDGET->cellv+selp;
  struct gui_widget *pickone=gui_push_modal(widget->gui,&gui_widget_type_pickone);
  if (!pickone) return;
  MN_SOUND(ACTIVATE)
  gui_widget_pickone_set_callback(pickone,indev_cb_map,widget);
  int optionc=sizeof(indev_optionv)/sizeof(indev_optionv[0]);
  int i=0;
  const struct indev_option *option=indev_optionv;
  int focusp=-1;
  for (;i<optionc;i++,option++) {
    gui_widget_pickone_add_option(pickone,option->name,-1);
    if (option->btnid==cell->dstbtnid) focusp=i;
  }
  if ((focusp>=0)&&(focusp<pickone->childc)) gui_widget_pickone_focus(pickone,pickone->childv[focusp]);
}

/* Highlight cell.
 */
 
static void indev_highlight(struct gui_widget *widget,int col,int row) {
  struct indev_highlight *highlight=WIDGET->highlightv;
  if (highlight->ttl) {
    struct indev_highlight *q=WIDGET->highlightv;
    int i=INDEV_HIGHLIGHT_LIMIT;
    for (;i-->0;q++) {
      if (q->ttl<highlight->ttl) {
        highlight=q;
      }
    }
  }
  highlight->col=col;
  highlight->row=row;
  highlight->ttl=INDEV_HIGHLIGHT_TIME;
}

/* Motion.
 */
 
static void _indev_motion(struct gui_widget *widget,int dx,int dy) {
  if (WIDGET->cellc<1) return;
  MN_SOUND(MOTION)
  WIDGET->selx+=dx;
  if (WIDGET->selx<0) WIDGET->selx=WIDGET->colc-1;
  else if (WIDGET->selx>=WIDGET->colc) WIDGET->selx=0;
  WIDGET->sely+=dy;
  if (WIDGET->sely<0) WIDGET->sely=WIDGET->rowc-1;
  else if (WIDGET->sely>=WIDGET->rowc) WIDGET->sely=0;
  // Allow the user to focus empty cells, it does no harm.
}

/* Signal.
 */
 
static void _indev_signal(struct gui_widget *widget,int sigid) {
  switch (sigid) {
    case GUI_SIGID_CANCEL: MN_SOUND(CANCEL) gui_dismiss_modal(widget->gui,widget); break;
    case GUI_SIGID_ACTIVATE: indev_edit_selected_cell(widget); break;
  }
}

/* Callback from inmgr.
 */

static void indev_cb_event(int devid,int btnid,int value,int state,void *userdata) {
  struct gui_widget *widget=userdata;
  if (devid!=WIDGET->devid) return;
  
  if ((btnid==-1)&&(value==-1)) {
    fprintf(stderr,"TODO: %s: Device disconnected. Drop any held state.\n",__func__);
    return;
  }
  if (!btnid) return;
  
  int p=indev_cellv_search(widget,btnid);
  if (p<0) return;
  struct indev_cell *cell=WIDGET->cellv+p;
  if (value==cell->value) return;
  cell->value=value;
  int col=p/WIDGET->rowc;
  int row=p%WIDGET->rowc;
  indev_redraw_gridtex_cell(widget,col,row);
  indev_highlight(widget,col,row);
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_indev={
  .name="indev",
  .objlen=sizeof(struct mn_widget_indev),
  .del=_indev_del,
  .init=_indev_init,
  .measure=_indev_measure,
  .pack=_indev_pack,
  .draw=_indev_draw,
  .motion=_indev_motion,
  .signal=_indev_signal,
};

/* Report capability.
 */
 
static int indev_cb_cap(int btnid,uint32_t hidusage,int lo,int hi,int value,void *userdata) {
  struct gui_widget *widget=userdata;
  //fprintf(stderr,"  0x%08x =%-6d %6d..%-6d usage=0x%08x\n",btnid,value,lo,hi,hidusage);
  
  if (WIDGET->cellc>=INDEV_BUTTON_LIMIT) return 1;
  if (WIDGET->cellc>=WIDGET->cella) {
    int na=WIDGET->cella+32;
    if (na>INT_MAX/sizeof(struct indev_cell)) return -1;
    void *nv=realloc(WIDGET->cellv,sizeof(struct indev_cell)*na);
    if (!nv) return -1;
    WIDGET->cellv=nv;
    WIDGET->cella=na;
  }
  struct indev_cell *cell=WIDGET->cellv+WIDGET->cellc++;
  memset(cell,0,sizeof(struct indev_cell));
  
  cell->btnid=btnid;
  cell->value=value;
  cell->dstbtnid=inmgr_get_dstbtnid(WIDGET->devid,btnid);
  cell->hidusage=hidusage;
  cell->srclo=lo;
  cell->srchi=hi;
  
  return 0;
}

/* Query device for capabilities and build up our UI.
 */
 
static int indev_cell_cmp(const void *a,const void *b) {
  const struct indev_cell *A=a,*B=b;
  return (int)A->btnid-(int)B->btnid;
}
 
static int indev_build_caps(struct gui_widget *widget) {
  struct eh_input_driver *driver=eh_input_driver_for_device(WIDGET->devid);
  if (driver) {
    if (driver->type->list_buttons) {
      if (driver->type->list_buttons(driver,WIDGET->devid,indev_cb_cap,widget)<0) return -1;
    }
  } else {
    // No driver, assume it's the System Keyboard.
    int btnid;
    for (btnid=0x00070004;btnid<=0x00070063;btnid++) { // A..KPDOT, basically the whole thing.
      if (indev_cb_cap(btnid,btnid,0,2,0,widget)<0) return -1;
    }
    for (btnid=0x000700e0;btnid<=0x000700e7;btnid++) { // Modifiers.
      if (indev_cb_cap(btnid,btnid,0,2,0,widget)<0) return -1;
    }
  }
  qsort(WIDGET->cellv,WIDGET->cellc,sizeof(struct indev_cell),indev_cell_cmp);
  return 0;
}

/* Public setup.
 */
 
int mn_widget_indev_setup(struct gui_widget *widget,int devid) {
  if (!widget||(widget->type!=&mn_widget_type_indev)) return -1;
  if (WIDGET->devid) return -1;
  WIDGET->devid=devid;
  
  if ((WIDGET->listenerid=inmgr_listen(indev_cb_event,widget))<1) return -1;
  inmgr_device_enable(WIDGET->devid,0);
  
  const char *name=eh_input_device_name(WIDGET->devid);
  if (!name||!name[0]) name="Unknown Device";
  int namec=0;
  while (name[namec]) namec++;
  if (!(WIDGET->nametex=gui_texture_from_text(widget->gui,0,name,namec,0xffffff))) return -1;
  
  /* Make a cell for each button, then sort by (btnid).
   * Sorting is important because we have to search later by btnid at event cadence.
   * I don't believe order is meaningful to the user.
   * inmgr does not cache caps, so we're pulling them from scratch each time the modal opens.
   */
  if (indev_build_caps(widget)<0) return -1;
  
  return 0;
}
