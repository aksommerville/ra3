#include "inmgr_internal.h"

/* Fire signal.
 */
 
static void inmgr_signal(int btnid) {
  int p=inmgr_signalv_search(btnid);
  if (p<0) return;
  inmgr.signalv[p].cb();
}

/* Set playerid for a device if we don't have one yet.
 * This happens at the first nonzero state change.
 */
 
static void inmgr_device_require_player(struct inmgr_device *device) {
  if (device->playerid) return;
  if (device->keyboard) {
    device->playerid=1;
    return;
  }
  int count_by_playerid[1+INMGR_PLAYER_LIMIT]={0};
  struct inmgr_device *other=inmgr.devicev;
  int i=inmgr.devicec;
  for (;i-->0;other++) {
    if (other->keyboard) continue;
    count_by_playerid[other->playerid]++;
  }
  device->playerid=1;
  for (i=2;i<=inmgr.playerc;i++) {
    // Strictly '<': Ties break low.
    if (count_by_playerid[i]<count_by_playerid[device->playerid]) device->playerid=i;
  }
  if (device->enable) {
    inmgr.playerv[device->playerid]|=INMGR_BTN_CD;
    inmgr.playerv[0]|=INMGR_BTN_CD;
  }
}

/* Set button state in device and cascade to player if changed.
 * One bit at a time.
 */
 
static void inmgr_device_set_button(struct inmgr_device *device,int btnid,int value) {
  if (value) {
    if (device->state&btnid) return;
    inmgr_device_require_player(device);
    device->state|=btnid;
    if (device->enable) {
      if (!(inmgr.playerv[device->playerid]&btnid)) {
        inmgr.playerv[device->playerid]|=btnid;
        inmgr.playerv[0]|=btnid;
      }
    }
  } else {
    if (!(device->state&btnid)) return;
    device->state&=~btnid;
    if (device->enable) {
      if (inmgr.playerv[device->playerid]&btnid) { // whether or not assigned yet
        inmgr.playerv[device->playerid]&=~btnid;
        inmgr.playerv[0]&=~btnid;
      }
    }
  }
}

/* Send event to spies.
 */
 
static void inmgr_spy_event(int devid,int btnid,int value,int state) {
  // Important to run backward: Spies are allowed to remove themselves during the callback.
  struct inmgr_spy *spy=inmgr.spyv+inmgr.spyc-1;
  int i=inmgr.spyc;
  for (;i-->0;spy--) spy->cb(devid,btnid,value,state,spy->userdata);
}

/* Button state change.
 */
 
void inmgr_event(int devid,int btnid,int value) {
  if (!btnid) return;
  int devp=inmgr_devicev_search(devid);
  if (devp<0) { inmgr_spy_event(devid,btnid,value,0); return; }
  struct inmgr_device *device=inmgr.devicev+devp;
  struct inmgr_button *button=inmgr_device_get_button(device,btnid);
  if (!button) { inmgr_spy_event(devid,btnid,value,device->state); return; }
  //fprintf(stderr,"%s %d.0x%08x=%d =>%04x\n",__func__,devid,btnid,value,button->dstbtnid);
  if (value==button->srcvalue) { inmgr_spy_event(devid,btnid,value,device->state); return; }
  button->srcvalue=value;
  switch (button->mode) {
  
    case INMGR_BUTTON_MODE_SIGNAL: {
        int dstvalue=((value>=button->srclo)&&(value<=button->srchi))?1:0;
        if (dstvalue!=button->dstvalue) {
          button->dstvalue=dstvalue;
          if (dstvalue) inmgr_signal(button->dstbtnid);
        }
      } break;
      
    case INMGR_BUTTON_MODE_BUTTON: {
        int dstvalue=((value>=button->srclo)&&(value<=button->srchi))?1:0;
        if (dstvalue!=button->dstvalue) {
          button->dstvalue=dstvalue;
          inmgr_device_set_button(device,button->dstbtnid,dstvalue);
        }
      } break;
      
    case INMGR_BUTTON_MODE_AXIS: {
        int dstvalue;
        if (button->srclo<button->srchi) dstvalue=(value<=button->srclo)?-1:(value>=button->srchi)?1:0;
        else dstvalue=(value<=button->srclo)?1:(value>=button->srchi)?-1:0;
        int pv=button->dstvalue;
        if (dstvalue==pv) break;
        button->dstvalue=dstvalue;
        if (dstvalue<0) {
          inmgr_device_set_button(device,button->dstbtnid&(INMGR_BTN_LEFT|INMGR_BTN_UP),1);
          inmgr_device_set_button(device,button->dstbtnid&(INMGR_BTN_RIGHT|INMGR_BTN_DOWN),0);
        } else if (dstvalue>0) {
          inmgr_device_set_button(device,button->dstbtnid&(INMGR_BTN_LEFT|INMGR_BTN_UP),0);
          inmgr_device_set_button(device,button->dstbtnid&(INMGR_BTN_RIGHT|INMGR_BTN_DOWN),1);
        } else {
          inmgr_device_set_button(device,button->dstbtnid&(INMGR_BTN_LEFT|INMGR_BTN_UP),0);
          inmgr_device_set_button(device,button->dstbtnid&(INMGR_BTN_RIGHT|INMGR_BTN_DOWN),0);
        }
      } break;
      
    case INMGR_BUTTON_MODE_HAT: {
        int dstvalue=value-button->srclo;
        if ((dstvalue<0)||(dstvalue>7)) dstvalue=-1;
        if (dstvalue==button->dstvalue) break;
        button->dstvalue=dstvalue;
        int dx=0,dy=0;
        switch (dstvalue) {
          case 1: case 2: case 3: dx=1; break;
          case 5: case 6: case 7: dx=-1; break;
        }
        switch (dstvalue) {
          case 0: case 1: case 7: dy=-1; break;
          case 3: case 4: case 5: dy=1; break;
        }
        if (dx<0) {
          inmgr_device_set_button(device,INMGR_BTN_LEFT,1);
          inmgr_device_set_button(device,INMGR_BTN_RIGHT,0);
        } else if (dx>0) {
          inmgr_device_set_button(device,INMGR_BTN_LEFT,0);
          inmgr_device_set_button(device,INMGR_BTN_RIGHT,1);
        } else {
          inmgr_device_set_button(device,INMGR_BTN_LEFT,0);
          inmgr_device_set_button(device,INMGR_BTN_RIGHT,0);
        }
        if (dy<0) {
          inmgr_device_set_button(device,INMGR_BTN_UP,1);
          inmgr_device_set_button(device,INMGR_BTN_DOWN,0);
        } else if (dy>0) {
          inmgr_device_set_button(device,INMGR_BTN_UP,0);
          inmgr_device_set_button(device,INMGR_BTN_DOWN,1);
        } else {
          inmgr_device_set_button(device,INMGR_BTN_UP,0);
          inmgr_device_set_button(device,INMGR_BTN_DOWN,0);
        }
      } break;
  }
  inmgr_spy_event(devid,btnid,value,device->state);
}

/* Pre-mapped event from client.
 */
 
void inmgr_artificial_event(int playerid,int btnid,int value) {
  if ((playerid<0)||(playerid>inmgr.playerc)) return;
  if (value) {
    inmgr.playerv[playerid]|=btnid;
    inmgr.playerv[0]|=btnid;
  } else {
    inmgr.playerv[playerid]&=~btnid;
    inmgr.playerv[0]&=~btnid;
  }
}

/* Begin connecting new device.
 */

void *inmgr_connect_begin(int devid,int vid,int pid,int version,const char *name,int namec) {
  if (inmgr.connect_in_progress) return 0;
  if (!(inmgr.connect.device=inmgr_device_add(devid))) return 0;
  inmgr.connect_in_progress=&inmgr.connect;
  if (!name) namec=0; else if (namec<0) { namec=0; while (name[namec]) namec++; }
  inmgr.connect.devid=devid;
  if (inmgr.connect.gdev=gcfg_input_get_device(inmgr.gcfg,vid,pid,version,name,namec)) {
    inmgr.connect.device->gcfg_mapid=inmgr.connect.gdev->mapid;
  } else {
    inmgr.connect.device->gcfg_mapid=0;
  }
  inmgr.connect.unsetc=0;
  //fprintf(stderr,"%s %d %04x:%04x:%04x '%.*s' gdev=%p\n",__func__,devid,vid,pid,version,namec,name,inmgr.connect.gdev);
  return inmgr.connect_in_progress;
}

/* Add a record to the "unset buttons" list.
 */
 
static int inmgr_unset_add(int btnid,int hidusage,int vlo,int vhi,int value) {
  if (vlo>=vhi) return 0; // No sense recording invalid ranges.
  if (inmgr.connect.unsetc>=INMGR_UNSET_LIMIT) return -1;
  if (inmgr.connect.unsetc>=inmgr.connect.unseta) {
    int na=inmgr.connect.unseta+32;
    if (na>INT_MAX/sizeof(struct inmgr_unset)) return -1;
    void *nv=realloc(inmgr.connect.unsetv,sizeof(struct inmgr_unset)*na);
    if (!nv) return -1;
    inmgr.connect.unsetv=nv;
    inmgr.connect.unseta=na;
  }
  int lo=0,hi=inmgr.connect.unsetc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=inmgr.connect.unsetv[ck].btnid;
         if (btnid<q) hi=ck;
    else if (btnid>q) lo=ck+1;
    else return -1;
  }
  struct inmgr_unset *unset=inmgr.connect.unsetv+lo;
  memmove(unset+1,unset,sizeof(struct inmgr_unset)*(inmgr.connect.unsetc-lo));
  inmgr.connect.unsetc++;
  unset->btnid=btnid;
  unset->hidusage=hidusage;
  unset->lo=vlo;
  unset->hi=vhi;
  unset->value=value;
  return 0;
}

/* Determine our output btnid from gcfg.
 */
 
static int inmgr_btnid_from_name(const char *name,int namec) {
  if (!name) return 0;
  if (namec<0) { namec=0; while (name[namec]) namec++; }
  if (!namec) return 0;
  #define _(tag) if ((namec==sizeof(#tag)-1)&&!memcmp(name,#tag,namec)) return INMGR_SIGNAL_##tag;
  INMGR_FOR_EACH_SIGNAL
  #undef _
  return 0;
}

static int inmgr_btnid_from_gcfg(int gbtnid,const char *name,int namec) {
  switch (gbtnid) {
    case GCFG_BTNID_DPAD: return INMGR_BTN_LEFT|INMGR_BTN_RIGHT|INMGR_BTN_UP|INMGR_BTN_DOWN;
    case GCFG_BTNID_HORZ: return INMGR_BTN_LEFT|INMGR_BTN_RIGHT;
    case GCFG_BTNID_VERT: return INMGR_BTN_UP|INMGR_BTN_DOWN;
    case GCFG_BTNID_LEFT: return INMGR_BTN_LEFT;
    case GCFG_BTNID_RIGHT: return INMGR_BTN_RIGHT;
    case GCFG_BTNID_UP: return INMGR_BTN_UP;
    case GCFG_BTNID_DOWN: return INMGR_BTN_DOWN;
    case GCFG_BTNID_SOUTH: return INMGR_BTN_SOUTH;
    case GCFG_BTNID_WEST: return INMGR_BTN_WEST;
    case GCFG_BTNID_EAST: return INMGR_BTN_EAST;
    case GCFG_BTNID_NORTH: return INMGR_BTN_NORTH;
    case GCFG_BTNID_L1: return INMGR_BTN_L1;
    case GCFG_BTNID_R1: return INMGR_BTN_R1;
    case GCFG_BTNID_L2: return INMGR_BTN_L2;
    case GCFG_BTNID_R2: return INMGR_BTN_R2;
    case GCFG_BTNID_AUX1: return INMGR_BTN_AUX1;
    case GCFG_BTNID_AUX2: return INMGR_BTN_AUX2;
    case GCFG_BTNID_AUX3: return INMGR_BTN_AUX3;
    case GCFG_BTNID_LX: return INMGR_BTN_LEFT|INMGR_BTN_RIGHT;
    case GCFG_BTNID_LY: return INMGR_BTN_UP|INMGR_BTN_DOWN;
    case GCFG_BTNID_RX: // Known, not mapped...
    case GCFG_BTNID_RY:
    case GCFG_BTNID_LP:
    case GCFG_BTNID_RP:
      return 0;
  }
  return inmgr_btnid_from_name(name,namec);
}

int inmgr_gcfg_btn_from_btnid(int imbtnid) {
  switch (imbtnid) {
    case INMGR_BTN_LEFT|INMGR_BTN_RIGHT|INMGR_BTN_UP|INMGR_BTN_DOWN: return GCFG_BTNID_DPAD;
    case INMGR_BTN_LEFT|INMGR_BTN_RIGHT: return GCFG_BTNID_HORZ;
    case INMGR_BTN_UP|INMGR_BTN_DOWN: return GCFG_BTNID_VERT;
    case INMGR_BTN_LEFT: return GCFG_BTNID_LEFT;
    case INMGR_BTN_RIGHT: return GCFG_BTNID_RIGHT;
    case INMGR_BTN_UP: return GCFG_BTNID_UP;
    case INMGR_BTN_DOWN: return GCFG_BTNID_DOWN;
    case INMGR_BTN_SOUTH: return GCFG_BTNID_SOUTH;
    case INMGR_BTN_WEST: return GCFG_BTNID_WEST;
    case INMGR_BTN_EAST: return GCFG_BTNID_EAST;
    case INMGR_BTN_NORTH: return GCFG_BTNID_NORTH;
    case INMGR_BTN_L1: return GCFG_BTNID_L1;
    case INMGR_BTN_R1: return GCFG_BTNID_R1;
    case INMGR_BTN_L2: return GCFG_BTNID_L2;
    case INMGR_BTN_R2: return GCFG_BTNID_R2;
    case INMGR_BTN_AUX1: return GCFG_BTNID_AUX1;
    case INMGR_BTN_AUX2: return GCFG_BTNID_AUX2;
    case INMGR_BTN_AUX3: return GCFG_BTNID_AUX3;
  }
  return 0;
}

/* Extract content from a gcfg comment.
 */
 
static int inmgr_comment_exists(const char *src,int srcc,const char *q,int qc) {
  if (!src||!q) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (qc<0) { qc=0; while (q[qc]) qc++; }
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) {
      srcp++;
      continue;
    }
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
    if ((tokenc==qc)&&!memcmp(token,q,qc)) return 1;
  }
  return 0;
}

/* Configure button.
 * If it doesn't add up, we'll leave (mode) zero. Those should be reaped before finalizing.
 * (comment) comes from gcfg; I haven't determined yet what will go there, but certainly space-delimited optional tokens.
 */
 
void inmgr_button_init(
  struct inmgr_button *button,
  int dstbtnid,
  const char *comment,int commentc,
  int hidusage,int lo,int hi,int value
) {
  int range=hi-lo+1;
  if (range<2) return;
  switch (button->dstbtnid=dstbtnid) {
    case 0: return;
    
    // Hats.
    case INMGR_BTN_LEFT|INMGR_BTN_RIGHT|INMGR_BTN_UP|INMGR_BTN_DOWN: {
        if (range!=8) return;
        button->mode=INMGR_BUTTON_MODE_HAT;
        button->srclo=lo;
        button->srchi=hi;
        button->srcvalue=lo-1;
        button->dstvalue=-1; // 0..7, or -1 for idle
      } break;
      
    // Axes.
    case INMGR_BTN_LEFT|INMGR_BTN_RIGHT:
    case INMGR_BTN_UP|INMGR_BTN_DOWN: {
        if (range<3) return;
        button->mode=INMGR_BUTTON_MODE_AXIS;
        int mid=(lo+hi)>>1;
        int midlo=(lo+mid)>>1;
        int midhi=(mid+hi)>>1;
        if (midlo>=mid) midlo=mid-1;
        if (midhi<=mid) midhi=mid+1;
        button->srclo=midlo; // (srclo,srchi) are the inclusive edges of the "on" ranges.
        button->srchi=midhi;
        button->srcvalue=mid;
        if (inmgr_comment_exists(comment,commentc,"reverse",7)) {
          button->srclo=midhi;
          button->srchi=midlo;
        }
      } break;
      
    // Signals.
    default: if (button->dstbtnid&~0xffff) {
        button->mode=INMGR_BUTTON_MODE_SIGNAL;
        button->srclo=lo+1;
        button->srchi=INT_MAX;
        
      // Two-state.
      } else {
        button->mode=INMGR_BUTTON_MODE_BUTTON;
        button->srclo=lo+1;
        button->srchi=INT_MAX;
      }
  }
}

/* Receive device capability.
 */
 
void inmgr_connect_more(void *ctx,int btnid,int hidusage,int lo,int hi,int value) {
  if (!ctx||(ctx!=inmgr.connect_in_progress)) return;
  if (lo>=hi) return;
  
  // If the button is known to gcfg and makes sense to us, map it immediately.
  struct gcfg_input_button *gbtn=gcfg_input_device_get_button(inmgr.connect.gdev,btnid,0);
  if (gbtn) {
    int imbtnid=0;
    if (gbtn->btnid>0) imbtnid=inmgr_btnid_from_gcfg(gbtn->btnid,gbtn->btnname,gbtn->btnnamec);
    else if (gbtn->btnid<0) imbtnid=inmgr_btnid_from_name(gbtn->btnname,gbtn->btnnamec);
    else return; // Marked zero in cfg -- ignore it.
    if (!imbtnid) imbtnid=inmgr_btnid_from_name(gbtn->name,gbtn->namec);
    if (imbtnid) {
      struct inmgr_button *button=inmgr_device_add_button(inmgr.connect.device,btnid);
      if (button) {
        inmgr_button_init(button,imbtnid,gbtn->comment,gbtn->commentc,hidusage,lo,hi,value);
        return;
      }
    }
    // Pass thru if didn't acquire an ID. We'll still record it as "unset".
  }
  
  inmgr_unset_add(btnid,hidusage,lo,hi,value);
}

/* Skip piecemeal capability declarations and grab the System Keyboard wholesale.
 */

void inmgr_connect_keyboard(void *ctx) {
  if (!ctx||(ctx!=inmgr.connect_in_progress)) return;
  
  /* Set the (keyboard) flag so when we wrap up, if it's not fully mapped, we can create a sensible default map.
   * See inmgr_device_assign_unset_keyboard.
   */
  inmgr.connect.device->keyboard=1;
  
  /* If there was an explicit config, use that.
   * Caps are not being reported, just assume they all exist.
   */
  if (inmgr.connect.gdev) {
    const struct gcfg_input_button *gbtn=inmgr.connect.gdev->buttonv;
    int i=inmgr.connect.gdev->buttonc;
    for (;i-->0;gbtn++) {
      int imbtnid=0;
      if (gbtn->btnid>0) imbtnid=inmgr_btnid_from_gcfg(gbtn->btnid,gbtn->btnname,gbtn->btnnamec);
      else if (gbtn->btnid<0) imbtnid=inmgr_btnid_from_name(gbtn->btnname,gbtn->btnnamec);
      else continue; // Marked zero in cfg -- ignore it.
      if (!imbtnid) imbtnid=inmgr_btnid_from_name(gbtn->name,gbtn->namec);
      if (imbtnid) {
        struct inmgr_button *button=inmgr_device_add_button(inmgr.connect.device,gbtn->srcbtnid);
        if (button) {
          inmgr_button_init(button,imbtnid,gbtn->comment,gbtn->commentc,gbtn->srcbtnid,0,1,0);
        }
      }
    }
  }
}

/* Special assign-unset logic for keyboards only.
 */
 
static int inmgr_unset_exists(const struct inmgr_unset *unset,int unsetc,int btnid) {
  for (;unsetc-->0;unset++) if (unset->btnid==btnid) return 1;
  return 0;
}
 
static int inmgr_device_assign_unset_keyboard(struct inmgr_device *device,const struct inmgr_unset *unset,int unsetc) {

  /* If we have at least one button mapped already, a manual config exists.
   * Don't make any further changes.
   * See inmgr_connect_keyboard.
   */
  if (device->buttonc) return 0;

  /* Check what's already mapped. Just on/off per button, no need to count them.
   */
  int cap=0;
  struct inmgr_button *button=device->buttonv;
  int i=device->buttonc;
  for (;i-->0;button++) {
    if (button->dstbtnid&~0xffff) continue; // Signals don't count.
    cap|=button->dstbtnid;
  }
  
  #define ADDBTN(usage,tag) { \
    if (button=inmgr_device_add_button(device,0x00070000|usage)) { \
      inmgr_button_init(button,INMGR_BTN_##tag,0,0,0,0,1,0); \
    } \
  }
  
  /* If we didn't get a dpad, use arrows, WASD, and keypad.
   * But if any dpad button is already mapped, do nothing.
   * Don't bother verifying that the source buttons exist.
   */
  if (!(cap&(INMGR_BTN_LEFT|INMGR_BTN_RIGHT|INMGR_BTN_UP|INMGR_BTN_DOWN))) {
    ADDBTN(0x04,LEFT)  // A
    ADDBTN(0x07,RIGHT) // D
    ADDBTN(0x16,DOWN)  // S
    ADDBTN(0x1a,UP)    // W
    ADDBTN(0x4f,RIGHT) // Right arrow
    ADDBTN(0x50,LEFT)  // Left arrow
    ADDBTN(0x51,DOWN)  // Down arrow
    ADDBTN(0x52,UP)    // Up arrow
    ADDBTN(0x5a,DOWN)  // KP2
    ADDBTN(0x5c,LEFT)  // KP4
    ADDBTN(0x5d,DOWN)  // KP5
    ADDBTN(0x5e,RIGHT) // KP6
    ADDBTN(0x60,UP)    // KP8
  }
  
  /* All other buttons are a la carte, assign if we don't have it yet.
   * Most have at least two sources (main section and keypad).
   */
  if (!(cap&INMGR_BTN_SOUTH)) {
    ADDBTN(0x1d,SOUTH) // Z
    ADDBTN(0x2c,SOUTH) // Space
    ADDBTN(0x62,SOUTH) // KP0
  }
  if (!(cap&INMGR_BTN_WEST)) {
    ADDBTN(0x1b,WEST) // X
    ADDBTN(0x08,WEST) // E
    ADDBTN(0x57,WEST) // KP Enter
  }
  if (!(cap&INMGR_BTN_EAST)) {
    ADDBTN(0x06,EAST) // C
    ADDBTN(0x14,EAST) // Q
    ADDBTN(0x57,EAST) // KP Plus
  }
  if (!(cap&INMGR_BTN_NORTH)) {
    ADDBTN(0x19,NORTH) // V
    ADDBTN(0x15,NORTH) // R
    ADDBTN(0x63,NORTH) // KP Dot
  }
  if (!(cap&INMGR_BTN_L1)) {
    ADDBTN(0x2b,L1) // Tab
    ADDBTN(0x5f,L1) // KP7
  }
  if (!(cap&INMGR_BTN_R1)) {
    ADDBTN(0x31,R1) // Backslash
    ADDBTN(0x61,R1) // KP9
  }
  if (!(cap&INMGR_BTN_L2)) {
    ADDBTN(0x35,L2) // Grave
    ADDBTN(0x59,L2) // KP1
  }
  if (!(cap&INMGR_BTN_R2)) {
    ADDBTN(0x2a,R2) // Backspace
    ADDBTN(0x5b,R2) // KP3
  }
  if (!(cap&INMGR_BTN_AUX1)) {
    ADDBTN(0x28,AUX1) // Enter
    ADDBTN(0x54,AUX1) // KP Slash
  }
  if (!(cap&INMGR_BTN_AUX2)) {
    ADDBTN(0x30,AUX2) // Close Bracket
    ADDBTN(0x55,AUX2) // KP Star
  }
  if (!(cap&INMGR_BTN_AUX3)) {
    ADDBTN(0x2f,AUX3) // Open Bracket
    ADDBTN(0x56,AUX3) // KP Dash
  }

  #undef ADDBTN
  return 0;
}

/* Examine the list of unset buttons and assign them if there's a sensible place.
 * When a device is not known to gcfg, this is the whole story for assignment.
 */
 
static int inmgr_device_assign_unset(struct inmgr_device *device,const struct inmgr_unset *unset,int unsetc) {
  
  /* If it's a keyboard, handle differently since there's a lot of buttons and they're fairly standard.
   * Note that we do still build up (unsetv) for keyboards, which is maybe not ideal?
   */
  if (device->keyboard) {
    return inmgr_device_assign_unset_keyboard(device,unset,unsetc);
  }

  /* Tally what we have so far.
   */
  int dpad[4]={0}; // Count per: LEFT,RIGHT,UP,DOWN
  int thumb[4]={0}; // Count per: SOUTH,WEST,EAST,NORTH
  int trigger[4]={0}; // Count per: L1,R1,L2,R2
  int aux[3]={0}; // Count per: AUX1,AUX2,AUX3
  struct inmgr_button *button=device->buttonv;
  int i=device->buttonc;
  for (;i-->0;button++) {
    switch (button->dstbtnid) {
      case INMGR_BTN_LEFT|INMGR_BTN_RIGHT|INMGR_BTN_UP|INMGR_BTN_DOWN: dpad[0]++; dpad[1]++; dpad[2]++; dpad[3]++; break;
      case INMGR_BTN_LEFT|INMGR_BTN_RIGHT: dpad[0]++; dpad[1]++; break;
      case INMGR_BTN_UP|INMGR_BTN_DOWN: dpad[2]++; dpad[3]++; break;
      case INMGR_BTN_LEFT: dpad[0]++; break;
      case INMGR_BTN_RIGHT: dpad[1]++; break;
      case INMGR_BTN_UP: dpad[2]++; break;
      case INMGR_BTN_DOWN: dpad[3]++; break;
      case INMGR_BTN_SOUTH: thumb[0]++; break;
      case INMGR_BTN_WEST: thumb[1]++; break;
      case INMGR_BTN_EAST: thumb[2]++; break;
      case INMGR_BTN_NORTH: thumb[3]++; break;
      case INMGR_BTN_L1: trigger[0]++; break;
      case INMGR_BTN_R1: trigger[1]++; break;
      case INMGR_BTN_L2: trigger[2]++; break;
      case INMGR_BTN_R2: trigger[3]++; break;
      case INMGR_BTN_AUX1: aux[0]++; break;
      case INMGR_BTN_AUX2: aux[1]++; break;
      case INMGR_BTN_AUX3: aux[2]++; break;
    }
  }
  
  /* Any button unset in the global mask, bump its count impossibly high so we skip it.
   * But not so high that we can't increment it; selection logic is fuzzy and we can in theory pick unmasked buttons.
   */
  if (inmgr.btnmask!=0x7fff) {
    if (!(inmgr.btnmask&INMGR_BTN_LEFT)) dpad[0]=INT_MAX>>1;
    if (!(inmgr.btnmask&INMGR_BTN_RIGHT)) dpad[1]=INT_MAX>>1;
    if (!(inmgr.btnmask&INMGR_BTN_UP)) dpad[2]=INT_MAX>>1;
    if (!(inmgr.btnmask&INMGR_BTN_DOWN)) dpad[3]=INT_MAX>>1;
    if (!(inmgr.btnmask&INMGR_BTN_SOUTH)) thumb[0]=INT_MAX>>1;
    if (!(inmgr.btnmask&INMGR_BTN_WEST)) thumb[1]=INT_MAX>>1;
    if (!(inmgr.btnmask&INMGR_BTN_EAST)) thumb[2]=INT_MAX>>1;
    if (!(inmgr.btnmask&INMGR_BTN_NORTH)) thumb[3]=INT_MAX>>1;
    if (!(inmgr.btnmask&INMGR_BTN_L1)) trigger[0]=INT_MAX>>1;
    if (!(inmgr.btnmask&INMGR_BTN_R1)) trigger[1]=INT_MAX>>1;
    if (!(inmgr.btnmask&INMGR_BTN_L2)) trigger[2]=INT_MAX>>1;
    if (!(inmgr.btnmask&INMGR_BTN_R2)) trigger[3]=INT_MAX>>1;
    if (!(inmgr.btnmask&INMGR_BTN_AUX1)) aux[0]=INT_MAX>>1;
    if (!(inmgr.btnmask&INMGR_BTN_AUX2)) aux[1]=INT_MAX>>1;
    if (!(inmgr.btnmask&INMGR_BTN_AUX3)) aux[2]=INT_MAX>>1;
  }
  
  /* Then examine each unset button in turn.
   */
  for (;unsetc-->0;unset++) {
    int range=unset->hi-unset->lo+1;
    if (range<2) continue; // What is this doing here?
    
    #define DPAD { \
      if (button=inmgr_device_add_button(device,unset->btnid)) { \
        inmgr_button_init(button,INMGR_BTN_LEFT|INMGR_BTN_RIGHT|INMGR_BTN_UP|INMGR_BTN_DOWN,0,0,0,unset->lo,unset->hi,unset->value); \
        dpad[0]++; \
        dpad[1]++; \
        dpad[2]++; \
        dpad[3]++; \
      } \
      continue; \
    }
    #define XAXIS { \
      if (button=inmgr_device_add_button(device,unset->btnid)) { \
        inmgr_button_init(button,INMGR_BTN_LEFT|INMGR_BTN_RIGHT,0,0,0,unset->lo,unset->hi,unset->value); \
        dpad[0]++; \
        dpad[1]++; \
      } \
      continue; \
    }
    #define YAXIS { \
      if (button=inmgr_device_add_button(device,unset->btnid)) { \
        inmgr_button_init(button,INMGR_BTN_UP|INMGR_BTN_DOWN,0,0,0,unset->lo,unset->hi,unset->value); \
        dpad[2]++; \
        dpad[3]++; \
      } \
      continue; \
    }
    #define SINGLE(tag,cfld) { \
      if (button=inmgr_device_add_button(device,unset->btnid)) { \
        inmgr_button_init(button,INMGR_BTN_##tag,0,0,0,unset->lo,unset->hi,unset->value); \
        cfld++; \
      } \
      continue; \
    }
    
    /* Range of 8 could only be a hat, and hats can only be range 8. Easy.
     */
    if (range==8) DPAD
    
    /* If HID Usage indicates something we know, and range agrees, go with that.
     */
    switch (unset->hidusage) {
      case 0x00010030: if (range>=3) XAXIS break;
      case 0x00010031: if (range>=3) YAXIS break;
      // 0x00010039 is Hat, but we don't want it if the range is not 8. Already handled.
      case 0x0001003d: SINGLE(AUX1,aux[0]) break;
      case 0x0001003e: SINGLE(AUX2,aux[1]) break;
      case 0x00010090: SINGLE(UP,dpad[2]) break;
      case 0x00010091: SINGLE(DOWN,dpad[3]) break;
      case 0x00010092: SINGLE(RIGHT,dpad[1]) break;
      case 0x00010093: SINGLE(LEFT,dpad[0]) break;
    }
    
    /* Range of at least 3 signed is likely an axis.
     * Also if the range is at least 3, and initial value lies between them (not upon), call it axis.
     */
    if ((range>=3)&&(
      ((unset->lo<0)&&(unset->hi>0))||
      ((unset->value>unset->lo)&&(unset->value<unset->hi))
    )) {
      int xc=dpad[0]+dpad[1];
      int yc=dpad[2]+dpad[3];
      if (xc<=yc) XAXIS
      else YAXIS
    }
    
    /* Assign willy-nilly to the least popular two-state button.
     * Do not assign to the dpad. Those are expected to come from a hat or axis.
     * Gamepads with dpad modelled as buttons do exist (and they are wonderful), but they're unusual so you have to configure it explicitly.
     */
    int lowc=INT_MAX;
    for (i=4;i-->0;) if (thumb[i]<lowc) lowc=thumb[i];
    for (i=4;i-->0;) if (trigger[i]<lowc) lowc=trigger[i];
    for (i=3;i-->0;) if (aux[i]<lowc) lowc=aux[i];
    // The order here is how we break ties. It reads a bit disorganized, because I'm imposing an opinion of how important each button is.
    // Bear in mind that if we reach this point at all, we're going to guess wrong most of the time, no matter what we do.
    if (lowc==thumb[0]) SINGLE(SOUTH,thumb[0])
    if (lowc==thumb[1]) SINGLE(WEST,thumb[1])
    if (lowc==thumb[2]) SINGLE(EAST,thumb[2])
    if (lowc==thumb[3]) SINGLE(NORTH,thumb[3])
    if (lowc==aux[0]) SINGLE(AUX1,aux[0])
    if (lowc==trigger[0]) SINGLE(L1,trigger[0])
    if (lowc==trigger[1]) SINGLE(R1,trigger[1])
    if (lowc==aux[2]) SINGLE(AUX3,aux[2])
    if (lowc==trigger[2]) SINGLE(L2,trigger[2])
    if (lowc==trigger[3]) SINGLE(R2,trigger[3])
    if (lowc==aux[1]) SINGLE(AUX2,aux[1])
    
    #undef DPAD
    #undef XAXIS
    #undef YAXIS
    #undef SINGLE
  }
  return 0;
}

/* Remove buttons with mode zero. Those are ones we couldn't map.
 */
 
static void inmgr_device_remove_modeless(struct inmgr_device *device) {
  int i=device->buttonc;
  while (i-->0) {
    struct inmgr_button *button=device->buttonv+i;
    if (button->mode) continue;
    device->buttonc--;
    memmove(button,button+1,sizeof(struct inmgr_button)*(device->buttonc-i));
  }
}

/* Set (device->keyboard) if some heurstics show it is.
 */
 
static int inmgr_device_detect_keyboard(struct inmgr_device *device,const struct inmgr_unset *unset,int unsetc) {
  if (device->keyboard) return 0;
  int page7c=0,totalc=device->buttonc+unsetc,i;
  const struct inmgr_button *button=device->buttonv;
  for (i=device->buttonc;i-->0;button++) if ((button->srcbtnid&0x00070000)==0x00070000) page7c++;
  for (;unsetc-->0;unset++) if ((unset->btnid&0x00070000)==0x00070000) page7c++;
  if (page7c>=40) device->keyboard=1;
  return 0;
}

/* Device is done mapping. Should we keep it?
 */
 
static int inmgr_device_usable(const struct inmgr_device *device) {
  uint16_t cap=0;
  const struct inmgr_button *button=device->buttonv;
  int i=device->buttonc;
  for (;i-->0;button++) {
    if (button->dstbtnid&~0xffff) continue; // Signals don't count.
    cap|=button->dstbtnid;
  }
  uint16_t dpad=INMGR_BTN_LEFT|INMGR_BTN_RIGHT|INMGR_BTN_UP|INMGR_BTN_DOWN;
  if ((cap&dpad)!=dpad) return 0; // Dpad is absolutely required.
  uint16_t thumb=INMGR_BTN_SOUTH|INMGR_BTN_WEST|INMGR_BTN_EAST|INMGR_BTN_NORTH;
  if (!(cap&thumb)) return 0; // At least one thumb button is required, but we don't care which.
  return 1; // OK keep it.
}

/* Finish connecting device.
 */
 
int inmgr_connect_end(void *ctx) {
  int result=0;
  if (ctx&&(ctx==inmgr.connect_in_progress)) {
    //fprintf(stderr,"%s buttonc=%d unsetc=%d\n",__func__,inmgr.connect.device->buttonc,inmgr.connect.unsetc);
    inmgr_device_detect_keyboard(inmgr.connect.device,inmgr.connect.unsetv,inmgr.connect.unsetc);
    inmgr_device_assign_unset(inmgr.connect.device,inmgr.connect.unsetv,inmgr.connect.unsetc);
    inmgr_device_remove_modeless(inmgr.connect.device);
    if (inmgr_device_usable(inmgr.connect.device)) {
      // Great, let's keep it. Do not assign (playerid) yet, we'll wait for a real event first, so they assign in a sensible order.
      result=1;
      inmgr.connect.device->state=INMGR_BTN_CD;
      inmgr_spy_event(inmgr.connect.device->devid,0,0,inmgr.connect.device->state);
    } else {
      inmgr_device_remove(inmgr.connect.device);
      inmgr.connect.device=0;
    }
    inmgr.connect_in_progress=0;
  }
  return result;
}

/* Disconnect device.
 */

void inmgr_disconnect(int devid) {
  int i=inmgr.devicec;
  while (i-->0) {
    struct inmgr_device *device=inmgr.devicev+i;
    if (device->devid!=devid) continue;
    int playerid=device->playerid;
    int state=device->state;
    inmgr_device_cleanup(device);
    inmgr.devicec--;
    memmove(device,device+1,sizeof(struct inmgr_device)*(inmgr.devicec-i));
    
    // Drop any held state. This will drop CD too, so then rerun over the devices for CD only.
    if (playerid&&device->enable) {
      inmgr.playerv[playerid]&=~state;
      inmgr.playerv[0]&=~state;
      for (i=inmgr.devicec,device=inmgr.devicev;i-->0;device++) {
        if (!device->playerid||!device->enable) continue;
        inmgr.playerv[device->playerid]|=INMGR_BTN_CD;
        inmgr.playerv[0]|=INMGR_BTN_CD;
      }
    }

    inmgr_spy_event(devid,-1,-1,0);
    return;
  }
}
