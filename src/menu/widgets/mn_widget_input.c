#include "../mn_internal.h"
#include "lib/emuhost.h"
#include "lib/eh_inmgr.h"

#define INPUT_ALARM_TIME 100
#define INPUT_NAME_LIMIT 32 /* Truncate device names, sometimes they're ridiculous. Just for looks. */

/* Object definition.
 */
 
struct mn_widget_input {
  struct gui_widget hdr;
  struct gui_texture *instructions;
  struct input_row {
    struct gui_texture *nametex;
    int y;
    int devid; // <=0 for fake rows: -1=Done
    int alarmw;
    int alarm; // Counts down after an event.
    int activate; // Event callback sets to 1. Update sets >1 after processing. Callback sets to zero.
    uint16_t state; // inmgr doesn't track mapped state per device, only per player. but we get enough information to reconstruct it.
  } *rowv;
  int rowc,rowa;
  int rowp;
  int margin;
  int listenerid;
};

#define WIDGET ((struct mn_widget_input*)widget)

/* Cleanup.
 */
 
static void input_row_cleanup(struct input_row *row) {
  gui_texture_del(row->nametex);
}
 
static void _input_del(struct gui_widget *widget) {
  gui_texture_del(WIDGET->instructions);
  if (WIDGET->rowv) {
    while (WIDGET->rowc-->0) input_row_cleanup(WIDGET->rowv+WIDGET->rowc);
    free(WIDGET->rowv);
  }
  eh_inmgr_unlisten(eh_get_inmgr(),WIDGET->listenerid);
}

/* Add row.
 */
 
static struct input_row *input_add_row(struct gui_widget *widget,const char *name,int namec,int devid) {
  if (WIDGET->rowc>=WIDGET->rowa) {
    int na=WIDGET->rowa+8;
    if (na>INT_MAX/sizeof(struct input_row)) return 0;
    void *nv=realloc(WIDGET->rowv,sizeof(struct input_row)*na);
    if (!nv) return 0;
    WIDGET->rowv=nv;
    WIDGET->rowa=na;
  }
  if (!name) namec=0; else if (namec<0) { namec=0; while (name[namec]) namec++; }
  if (namec>INPUT_NAME_LIMIT) namec=INPUT_NAME_LIMIT;
  struct input_row *row=WIDGET->rowv+WIDGET->rowc++;
  memset(row,0,sizeof(struct input_row));
  if (!(row->nametex=gui_texture_from_text(widget->gui,0,name,namec,0xffffff))) return 0;
  row->devid=devid;
  return row;
}

/* Remove row.
 */
 
static void input_remove_row_by_devid(struct gui_widget *widget,int devid) {
  int i=WIDGET->rowc;
  struct input_row *row=WIDGET->rowv+i-1;
  for (;i-->0;row--) {
    if (row->devid!=devid) continue;
    input_row_cleanup(row);
    WIDGET->rowc--;
    memmove(row,row+1,sizeof(struct input_row)*(WIDGET->rowc-i));
    if (WIDGET->rowp>i) WIDGET->rowp--;
    else if (WIDGET->rowp==i) WIDGET->rowp=-1;
    return;
  }
}

/* Find row.
 */
 
static struct input_row *input_row_by_devid(struct gui_widget *widget,int devid) {
  int i=WIDGET->rowc;
  struct input_row *row=WIDGET->rowv+i-1;
  for (;i-->0;row--) {
    if (row->devid!=devid) continue;
    return row;
  }
  return 0;
}

/* Event from inmgr.
 */
 
static int input_cb_event(void *userdata,const struct eh_inmgr_event *event) {
  struct gui_widget *widget=userdata;
  /**
  fprintf(stderr,
    "%s widget=%p 0x%04x=%d[%04x] %d.%d=%d\n",
    __func__,widget,event->btnid,event->value,event->state,event->srcdevid,event->srcbtnid,event->srcvalue
  );
  /**/
  
  if (event->srcdevid<=0) return 0; // Artificial player event, not interesting.

  if ((event->srcbtnid==0)&&(event->srcvalue==0)) { // Connect
    struct eh_inmgr *inmgr=eh_get_inmgr();
    if (!inmgr) return 0;
    const char *name=eh_inmgr_device_name(inmgr,event->srcdevid);
    if (!name||!name[0]) name="Unknown Device";
    input_remove_row_by_devid(widget,event->srcdevid);
    struct input_row *row=input_add_row(widget,name,-1,event->srcdevid);
    if (!row) return 0;
    gui_dirty_pack(widget->gui);
    return 0;
  }

  if ((event->srcbtnid==-1)&&(event->srcvalue==-1)) { // Disconnect
    input_remove_row_by_devid(widget,event->srcdevid);
    gui_dirty_pack(widget->gui);
    return 0;
  }

  // Regular state change.
  struct input_row *row=input_row_by_devid(widget,event->srcdevid);
  if (!row) return 0;
  if (event->btnid) {
    if (event->value) row->state|=event->btnid;
    else row->state&=~event->btnid;
  }
  row->alarm=INPUT_ALARM_TIME;
  if ((row->state&(EH_BTN_SOUTH|EH_BTN_WEST))==(EH_BTN_SOUTH|EH_BTN_WEST)) {
    if (!row->activate) row->activate=1; // only touch if it's zero
  } else {
    if (row->activate>1) row->activate=0; // only reset if it's been processed
  }
  
  return 0;
}

/* Init.
 */
 
static int _input_init(struct gui_widget *widget) {

  WIDGET->margin=10;

  if (!(WIDGET->instructions=gui_texture_from_text(widget->gui,0,"(A+B) to select",-1,0xc0c0c0))) return -1;
  if (!input_add_row(widget,"Done",4,-1)) return -1;
  
  struct eh_inmgr_delegate delegate={
    .userdata=widget,
    .cb_event=input_cb_event,
  };
  if ((WIDGET->listenerid=eh_inmgr_listen_source(eh_get_inmgr(),0,&delegate))<0) return -1;

  return 0;
}

/* Measure.
 */
 
static void _input_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  *w=*h=0;
  gui_texture_get_size(w,h,WIDGET->instructions);
  struct input_row *row=WIDGET->rowv;
  int i=WIDGET->rowc;
  for (;i-->0;row++) {
    int chw,chh;
    gui_texture_get_size(&chw,&chh,row->nametex);
    if (chw>*w) *w=chw;
    (*h)+=chh;
    if (row->devid>0) {
      row->alarmw=chh;
      if (row->alarmw+chw>*w) *w=row->alarmw+chw;
    } else {
      row->alarmw=0;
    }
  }
  (*w)+=WIDGET->margin<<1;
  (*h)+=WIDGET->margin<<1;
}

/* Pack.
 */
 
static void _input_pack(struct gui_widget *widget) {
  int chw,chh;
  gui_texture_get_size(&chw,&chh,WIDGET->instructions);
  int y=WIDGET->margin+chh;
  struct input_row *row=WIDGET->rowv;
  int i=WIDGET->rowc;
  for (;i-->0;row++) {
    row->y=y;
    gui_texture_get_size(&chw,&chh,row->nametex);
    y+=chh;
  }
}

/* Draw.
 */
 
static void _input_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x402000ff);
  gui_draw_texture(widget->gui,x+WIDGET->margin,y+WIDGET->margin,WIDGET->instructions);
  const struct input_row *row=WIDGET->rowv;
  int i=0;
  for (;i<WIDGET->rowc;i++,row++) {
    gui_draw_texture(widget->gui,x+WIDGET->margin+row->alarmw,y+row->y,row->nametex);
    if (row->alarmw>0) {
      if (row->alarm>0) {
        int a=0xff;
        if (row->alarm<INPUT_ALARM_TIME) a=(row->alarm*0xff)/INPUT_ALARM_TIME;
        gui_draw_rect(widget->gui,x+WIDGET->margin,y+row->y,row->alarmw,row->alarmw,0x00ff0000|a);
      }
    }
    if (i==WIDGET->rowp) {
      int chw,chh;
      gui_texture_get_size(&chw,&chh,row->nametex);
      gui_frame_rect(widget->gui,x+WIDGET->margin+row->alarmw,y+row->y,chw,chh,0xffff00ff);
    }
  }
}

/* Motion.
 */
 
static void _input_motion(struct gui_widget *widget,int dx,int dy) {
  if (WIDGET->rowc<1) return;
  int np=WIDGET->rowp;
  if (dy<0) np--;
  else if (dy>0) np++;
  if (np<0) np=WIDGET->rowc-1;
  else if (np>=WIDGET->rowc) np=0;
  if (np==WIDGET->rowp) return;
  MN_SOUND(MOTION)
  WIDGET->rowp=np;
}

/* Signal.
 * We actually ignore these.
 * Because we are an input monitor, users might be pressing their buttons without thinking what the UI might do with those presses.
 * So we require A+B to actuate buttons. Dpad works as usual, but that won't change state except the row selection, no big deal.
 */
 
static void _input_signal(struct gui_widget *widget,int sigid) {
}

/* Open device-specific modal.
 */
 
static void input_open_indev(struct gui_widget *widget,struct input_row *row) {
  if (!row) return;
  
  /* Don't allow them to open indev when there's only one device connected.
   * indev requires that you use a second controller to do the configuring, and the one under configuration becomes inert temporarily.
   * We have one fake row "Done", so the threshold is two.
   */
  if (WIDGET->rowc<=2) {
    struct gui_texture *ntexture=gui_texture_from_text(widget->gui,0,"Second controller required.",-1,0xff8080);
    if (ntexture) {
      gui_texture_del(WIDGET->instructions);
      WIDGET->instructions=ntexture;
      gui_dirty_pack(widget->gui);
    }
    MN_SOUND(REJECT)
    return;
  }
  
  struct gui_widget *indev=gui_push_modal(widget->gui,&mn_widget_type_indev);
  if (!indev) return;
  MN_SOUND(ACTIVATE)
  mn_widget_indev_setup(indev,row->devid);
}

/* Activate.
 */
 
static void input_activate(struct gui_widget *widget) {
  if ((WIDGET->rowp<0)||(WIDGET->rowp>=WIDGET->rowc)) return;
  struct input_row *row=WIDGET->rowv+WIDGET->rowp;
  switch (row->devid) {
    case -1: MN_SOUND(CANCEL) gui_dismiss_modal(widget->gui,widget); return;
    default: if (row->devid>0) input_open_indev(widget,row);
  }
}

/* Update.
 */
 
static void _input_update(struct gui_widget *widget) {
  struct input_row *row=WIDGET->rowv;
  int i=WIDGET->rowc;
  for (;i-->0;row++) {
    if (row->alarm>0) row->alarm--;
    if (row->activate==1) {
      row->activate=2;
      input_activate(widget);
    }
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_input={
  .name="input",
  .objlen=sizeof(struct mn_widget_input),
  .del=_input_del,
  .init=_input_init,
  .measure=_input_measure,
  .pack=_input_pack,
  .draw=_input_draw,
  .motion=_input_motion,
  .signal=_input_signal,
  .update=_input_update,
};
