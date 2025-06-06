/* mn_widget_home.c
 * The main page widget.
 * We're divided into 3 main sections, vertically: menubar, carousel, gamedetails
 */

#include "../mn_internal.h"
#include "lib/emuhost.h"

/* Object definition.
 */
 
struct mn_widget_home {
  struct gui_widget hdr;
  int focusp; // index in childv: 0=menubar, 1=carousel, 2=gamedetails
  uint16_t pvinput;
  int unlock_state; // L1+R1 and Down Down Down: Exit kiosk mode.
};

#define WIDGET ((struct mn_widget_home*)widget)

/* Delete.
 */
 
static void _home_del(struct gui_widget *widget) {
}

/* Init.
 */
 
static int _home_init(struct gui_widget *widget) {
  widget->opaque=1;
  
  if (!mn.kiosk) {
    if (!gui_widget_spawn(widget,&mn_widget_type_menubar)) return -1;
    WIDGET->focusp=1;
  } else {
    WIDGET->focusp=0;
  }
  if (!gui_widget_spawn(widget,&mn_widget_type_carousel)) return -1;
  if (!gui_widget_spawn(widget,&mn_widget_type_gamedetails)) return -1;
  
  return 0;
}

/* Pack.
 */
 
static void _home_pack(struct gui_widget *widget) {
  struct gui_widget *menubar=0,*carousel=0,*gamedetails=0;
  if (widget->childc==2) {
    carousel=widget->childv[0];
    gamedetails=widget->childv[1];
  } else if (widget->childc==3) {
    menubar=widget->childv[0];
    carousel=widget->childv[1];
    gamedetails=widget->childv[2];
  } else return;
  
  /* All three children get the full width.
   * menubar gets its preferred height, and the other two split the remainder evenly.
   */
  int chw,chh=0;
  if (menubar) {
    gui_widget_measure(&chw,&chh,menubar,widget->w,widget->h);
    menubar->x=0;
    menubar->y=0;
    menubar->w=widget->w;
    menubar->h=chh;
    gui_widget_pack(menubar);
  }
  carousel->x=0;
  carousel->y=chh;
  carousel->w=widget->w;
  carousel->h=(widget->h-carousel->y)>>1;
  gamedetails->x=0;
  gamedetails->y=carousel->y+carousel->h;
  gamedetails->w=widget->w;
  gamedetails->h=widget->h-gamedetails->y;
  gui_widget_pack(carousel);
  gui_widget_pack(gamedetails);
}

/* Draw.
 */
 
static void _home_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_gradient(widget->gui,x,y,widget->w,widget->h,0x302038ff,0x302038ff,0x200018ff,0x100030ff);
  int i=0;
  for (;i<widget->childc;i++) {
    struct gui_widget *child=widget->childv[i];
    gui_widget_draw(child,x+child->x,y+child->y);
  }
}

/* Update.
 */
 
static void _home_update(struct gui_widget *widget) {

  // Check for nonstandard input changes.
  uint16_t input=eh_input_get(0);
  if (WIDGET->pvinput!=input) {
    if ((input&EH_BTN_R2)&&!(WIDGET->pvinput&EH_BTN_R2)) {
      MN_SOUND(ACTIVATE)
      dbs_randomize(&mn.dbs);
    }
    if (mn.kiosk&&(input&EH_BTN_L1)&&(input&EH_BTN_R1)) {
      if (input&~(EH_BTN_L1|EH_BTN_R1|EH_BTN_DOWN)) {
        WIDGET->unlock_state=0;
      } else if ((input&EH_BTN_DOWN)&&!(WIDGET->pvinput&EH_BTN_DOWN)) {
        WIDGET->unlock_state++;
        if (WIDGET->unlock_state>=3) {
          mn.kiosk=0;
          mn.rebuild_gui=1;
          WIDGET->unlock_state=0;
        }
      }
    } else {
      WIDGET->unlock_state=0;
    }
    WIDGET->pvinput=input;
  }

  int i=widget->childc;
  while (i-->0) {
    struct gui_widget *child=widget->childv[i];
    if (child->type->update) child->type->update(child);
  }
}

/* Send focus or blur events to the focussed child, if there is one.
 */
 
static void mn_home_blur_child(struct gui_widget *widget) {
  if (WIDGET->focusp<0) return;
  if (WIDGET->focusp>=widget->childc) return;
  struct gui_widget *focus=widget->childv[WIDGET->focusp];
  if (!focus->type->signal) return;
  focus->type->signal(focus,GUI_SIGID_BLUR);
}
 
static void mn_home_focus_child(struct gui_widget *widget) {
  if (WIDGET->focusp<0) return;
  if (WIDGET->focusp>=widget->childc) return;
  struct gui_widget *focus=widget->childv[WIDGET->focusp];
  if (!focus->type->signal) return;
  focus->type->signal(focus,GUI_SIGID_FOCUS);
}

/* Motion.
 */
 
static void _home_motion(struct gui_widget *widget,int dx,int dy) {
  if (dy<0) {
    if (mn.kiosk) {
      // In kiosk mode, there's only one row. Do nothing.
    } else if (WIDGET->focusp>0) {
      mn_home_blur_child(widget);
      WIDGET->focusp--;
      mn_home_focus_child(widget);
      mn_cb_sound_effect(GUI_SFXID_MOTION,0);
    } else {
      mn_cb_sound_effect(GUI_SFXID_REJECT,0);
    }
  } else if (dy>0) {
    if (mn.kiosk) {
      // In kiosk mode, there's only one row. Do nothing.
    } else if ((WIDGET->focusp<1)&&(widget->childc>=2)) { // Only the first two kids can focus; details should not.
      mn_home_blur_child(widget);
      WIDGET->focusp++;
      mn_home_focus_child(widget);
      mn_cb_sound_effect(GUI_SFXID_MOTION,0);
    } else {
      mn_cb_sound_effect(GUI_SFXID_REJECT,0);
    }
  } else if ((WIDGET->focusp>=0)&&(WIDGET->focusp<widget->childc)) {
    struct gui_widget *child=widget->childv[WIDGET->focusp];
    if (child->type->motion) child->type->motion(child,dx,dy);
  }
}

/* Signal.
 */
 
static void _home_signal(struct gui_widget *widget,int sigid) {
  if (sigid==GUI_SIGID_BLUR) {
    mn_home_blur_child(widget);
  } else if (sigid==GUI_SIGID_FOCUS) {
    mn_home_focus_child(widget);
  } else if ((WIDGET->focusp>=0)&&(WIDGET->focusp<widget->childc)) {
    struct gui_widget *child=widget->childv[WIDGET->focusp];
    if (child->type->signal) child->type->signal(child,sigid);
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_home={
  .name="home",
  .objlen=sizeof(struct mn_widget_home),
  .del=_home_del,
  .init=_home_init,
  .pack=_home_pack,
  .draw=_home_draw,
  .update=_home_update,
  .motion=_home_motion,
  .signal=_home_signal,
};
