#include "eh_inmgr_internal.h"

/* Fire callbacks for an output event.
 */
 
static int eh_inmgr_output_callbacks(struct eh_inmgr *inmgr,int playerid,int btnid,int value) {
  int result=0;
  //fprintf(stderr,"%s %d.0x%04x=%d\n",__func__,playerid,btnid,value);
  
  struct eh_inmgr_event event={
    .srcdevid=inmgr->srcdevid,
    .srcbtnid=inmgr->srcbtnid,
    .srcvalue=inmgr->srcvalue,
    .playerid=playerid,
    .btnid=btnid,
    .value=value,
  };
  if (btnid) {
    struct eh_inmgr_player *player=eh_inmgr_player_by_id(inmgr,playerid);
    if (player) event.state=player->state;
  }
  
  if (inmgr->delegate.cb_event) {
    if (inmgr->delegate.cb_event(inmgr->delegate.userdata,&event)<0) return -1;
  }
  
  int i=inmgr->listenerc;
  struct eh_inmgr_listener *listener=inmgr->listenerv+i-1;
  for (;i-->0;listener--) {
    if (!listener->delegate.cb_event) continue;
    if (listener->reqtype==EH_INMGR_REQTYPE_ALL) {
      if (listener->delegate.cb_event(listener->delegate.userdata,&event)<0) return -1;
      result=1;
    } else if (listener->reqtype==EH_INMGR_REQTYPE_PLAYER) {
      if (!btnid) continue;
      if (listener->reqid==playerid) {
        if (listener->delegate.cb_event(listener->delegate.userdata,&event)<0) return -1;
        result=1;
      }
    } else if (listener->reqtype==EH_INMGR_REQTYPE_DEVICE) {
      if (inmgr->srcdevid) {
        if ((listener->reqid==inmgr->srcdevid)||!listener->reqid) {
          if (listener->delegate.cb_event(listener->delegate.userdata,&event)<0) return -1;
          result=1;
        }
      }
    }
  }
  return result;
}

/* Update player state and trigger callbacks if changed.
 */
 
int eh_inmgr_update_player(struct eh_inmgr *inmgr,int playerid,int btnid,int value) {
  value=value?1:0;
  //fprintf(stderr,"%s %d.%04x=%d\n",__func__,playerid,btnid,value);

  // Stateless events go straight to the callbacks, and btnid zero is noop.
  if (btnid&0xffff0000) return eh_inmgr_output_callbacks(inmgr,playerid,btnid,value);
  if (!btnid) return 0;
  int result=0;

  // Update player state if nonzero.
  struct eh_inmgr_player *player;
  if (playerid&&(player=eh_inmgr_player_by_id(inmgr,playerid))) {
    if (value) {
      if ((player->state&btnid)!=btnid) {
        player->state|=btnid;
        if (eh_inmgr_output_callbacks(inmgr,playerid,btnid,value)<0) return -1;
        result=1;
      }
    } else {
      if (player->state&btnid) {
        player->state&=~btnid;
        if (eh_inmgr_output_callbacks(inmgr,playerid,btnid,value)<0) return -1;
        result=1;
      }
    }
  }
  
  return result;
}

/* Device connected.
 */
 
int eh_inmgr_connect(struct eh_inmgr *inmgr,struct eh_input_driver *driver,int devid) {

  int p=eh_inmgr_devicev_search(inmgr,devid);
  if (p>=0) return 0;
  p=-p-1;
  struct eh_inmgr_device *device=eh_inmgr_devicev_insert(inmgr,p,devid);
  if (!device) return -1;
  device->driver=driver;
  
  if (device->config=eh_inmgr_config_for_device(inmgr,device)) {
    // Got a config already, great.
  } else {
    if (device->config=eh_inmgr_guess_config(inmgr,device->driver,devid)) {
      // Made one up, also ok.
    } else {
      //const char *devname=eh_inmgr_device_name(inmgr,devid);
      //fprintf(stderr,"%s:%d Dropping unconfigurable device '%s'\n",driver?driver->type->name:"(none)",devid,devname);
      eh_inmgr_devicev_remove(inmgr,p);
      return 0;
    }
  }
  if (eh_inmgr_device_apply_config(device)<0) {
    eh_inmgr_devicev_remove(inmgr,p);
    return -1;
  }
  
  inmgr->srcdevid=devid;
  inmgr->srcbtnid=0;
  inmgr->srcvalue=0;
  int result=eh_inmgr_output_callbacks(inmgr,0,0,0);
  inmgr->srcdevid=0;
  inmgr->srcbtnid=0;
  inmgr->srcvalue=0;
  return result;
}

/* Device disconnected.
 */
 
int eh_inmgr_disconnect(struct eh_inmgr *inmgr,int devid) {
  int p=eh_inmgr_devicev_search(inmgr,devid);
  if (p<0) return 0;
  
  // If a player is attached, zero its state.
  // We drop the whole player state, not just that due to this device, because it's more readily available.
  int playerid=inmgr->devicev[p].playerid;
  if (playerid) {
    inmgr->devicev[p].playerid=0;
    struct eh_inmgr_player *player=inmgr->playerv+playerid;
    player->devicec--;
    if (player->state) {
      uint16_t mask=0x8000;
      for (;mask;mask>>=1) {
        if (player->state&mask) {
          eh_inmgr_update_player(inmgr,playerid,mask,0);
        }
      }
    }
  }
  
  // Notify listeners of the disconnection.
  inmgr->srcdevid=devid;
  inmgr->srcbtnid=-1;
  inmgr->srcvalue=-1;
  int result=eh_inmgr_output_callbacks(inmgr,0,0,0);
  inmgr->srcdevid=0;
  inmgr->srcbtnid=0;
  inmgr->srcvalue=0;
  
  // If the device list changed during callbacks, report an error.
  if ((p>=0)&&(p<inmgr->devicec)&&(inmgr->devicev[p].devid==devid)) {
    eh_inmgr_devicev_remove(inmgr,p);
  } else {
    return -1;
  }
  return result;
}

/* Assign (device->playerid) if not already set.
 */
 
static void eh_inmgr_require_playerid(struct eh_inmgr *inmgr,struct eh_inmgr_device *device) {
  if (device->playerid) return;
  
  // Device without a driver is the system keyboard -- it's player one no matter what, and does not add to his (devicec).
  // In other words, the next device connected will also be player one.
  if (!device->driver) {
    device->playerid=1;
    return;
  }
  
  struct eh_inmgr_player *player=inmgr->playerv;
  struct eh_inmgr_player *loneliest=player;
  int i=0; for (;i<inmgr->playerc;i++,player++) {
    if (!player->devicec) {
      player->devicec=1;
      device->playerid=player->playerid;
      return;
    }
    if (player->devicec<loneliest->devicec) loneliest=player;
  }
  loneliest->devicec++;
  device->playerid=loneliest->playerid;
}

/* Split normalized dpad (0..7) into two axes (-1..1).
 */
 
static inline void eh_inmgr_split_dpad(int *x,int *y,int dpad) {
  switch (dpad) {
    case 0:  *x= 0; *y=-1; break;
    case 1:  *x= 1; *y=-1; break;
    case 2:  *x= 1; *y= 0; break;
    case 3:  *x= 1; *y= 1; break;
    case 4:  *x= 0; *y= 1; break;
    case 5:  *x=-1; *y= 1; break;
    case 6:  *x=-1; *y= 0; break;
    case 7:  *x=-1; *y=-1; break;
    default: *x= 0; *y= 0; break;
  }
}

/* Update one button record and return >0 if dstvalue changed.
 */
 
static inline int eh_inmgr_button_update(struct eh_inmgr_button *button,int srcvalue) {
  /**
  fprintf(stderr,
    "%s srcvalue=%d pv=%d pvdst=%d range=%d..%d\n",
    __func__,srcvalue,button->srcvalue,button->dstvalue,button->srclo,button->srchi
  );
  /**/
  if (button->srcvalue==srcvalue) return 0;
  button->srcvalue=srcvalue;
  int dstvalue;
  if (button->dstbtnid&0xffff0000) {
    dstvalue=((srcvalue>=button->srclo)&&(srcvalue<=button->srchi))?1:0;
  } else if (button->dstbtnid==EH_BTN_DPAD) {
    if (button->srclo<button->srchi) {
      dstvalue=srcvalue-button->srclo;
    } else {
      dstvalue=srcvalue-button->srchi;
    }
  } else if ((button->dstbtnid==EH_BTN_HORZ)||(button->dstbtnid==EH_BTN_VERT)) {
    if (button->srclo<button->srchi) {
      dstvalue=(srcvalue<=button->srclo)?-1:(srcvalue>=button->srchi)?1:0;
    } else {
      dstvalue=(srcvalue<=button->srchi)?1:(srcvalue>=button->srclo)?-1:0;
    }
  } else {
    dstvalue=((srcvalue>=button->srclo)&&(srcvalue<=button->srchi))?1:0;
  }
  if (button->dstvalue==dstvalue) return 0;
  button->dstvalue=dstvalue;
  return 1;
}

/* Ordinary input event.
 */
 
int eh_inmgr_button(struct eh_inmgr *inmgr,int devid,int btnid,int value) {
  //fprintf(stderr,"%s %d.0x%x=%d\n",__func__,devid,btnid,value);
  if (!devid||!btnid) return 0;
  int result=0;
  
  inmgr->srcdevid=devid;
  inmgr->srcbtnid=btnid;
  inmgr->srcvalue=value;
  if ((result=eh_inmgr_output_callbacks(inmgr,0,0,0))<0) goto _done_;
  
  if (!eh_inmgr_device_is_enabled(inmgr,devid)) goto _done_;
  
  struct eh_inmgr_device *device=eh_inmgr_device_by_id(inmgr,devid);
  if (!device) goto _done_;
  
  struct eh_inmgr_button *button=eh_inmgr_device_get_button(device,btnid);
  if (!button) goto _done_;
  
  int pvdstvalue=button->dstvalue;
  int changed=eh_inmgr_button_update(button,value);
  if (!changed) goto _done_;
  
  // Notify of the source event.
  if ((result=eh_inmgr_output_callbacks(inmgr,0,0,0))<0) goto _done_;
  
  // Stateless events are the same as 2-state, but we only report the ON values.
  if (button->dstbtnid&0xffff0000) {
    if (button->dstvalue) {
      eh_inmgr_require_playerid(inmgr,device);
      result=eh_inmgr_update_player(inmgr,device->playerid,button->dstbtnid,1);
    }
    
  // Button ID zero shouldn't exist, but easily handled (do nothing).
  } else if (!button->dstbtnid) {
  
  // Dpad is messy. dstvalue is normalized: 0..7 == N,NE,E,SE,S,SW,W,NW
  } else if (button->dstbtnid==EH_BTN_DPAD) {
    eh_inmgr_require_playerid(inmgr,device);
    int pvx=0,pvy=0,nx=0,ny=0;
    eh_inmgr_split_dpad(&pvx,&pvy,pvdstvalue);
    eh_inmgr_split_dpad(&nx,&ny,button->dstvalue);
    if (pvy!=ny) {
      if (pvy<0) {
        if ((result=eh_inmgr_update_player(inmgr,device->playerid,EH_BTN_UP,0))<0) goto _done_;
      } else if (pvy>0) {
        if ((result=eh_inmgr_update_player(inmgr,device->playerid,EH_BTN_DOWN,0))<0) goto _done_;
      }
      if (ny<0) {
        if ((result=eh_inmgr_update_player(inmgr,device->playerid,EH_BTN_UP,1))<0) goto _done_;
      } else if (ny>0) {
        if ((result=eh_inmgr_update_player(inmgr,device->playerid,EH_BTN_DOWN,1))<0) goto _done_;
      }
    }
    if (pvx!=nx) {
      if (pvx<0) {
        if ((result=eh_inmgr_update_player(inmgr,device->playerid,EH_BTN_LEFT,0))<0) goto _done_;
      } else if (pvx>0) {
        if ((result=eh_inmgr_update_player(inmgr,device->playerid,EH_BTN_RIGHT,0))<0) goto _done_;
      }
      if (nx<0) {
        if ((result=eh_inmgr_update_player(inmgr,device->playerid,EH_BTN_LEFT,1))<0) goto _done_;
      } else if (nx>0) {
        if ((result=eh_inmgr_update_player(inmgr,device->playerid,EH_BTN_RIGHT,1))<0) goto _done_;
      }
    }
  
  // Three-way axes are not as bad. (dstvalue) is -1,0,1.
  } else if ((button->dstbtnid==EH_BTN_HORZ)||(button->dstbtnid==EH_BTN_VERT)) {
    eh_inmgr_require_playerid(inmgr,device);
    int btnidlo,btnidhi;
    if (button->dstbtnid==EH_BTN_HORZ) {
      btnidlo=EH_BTN_LEFT;
      btnidhi=EH_BTN_RIGHT;
    } else {
      btnidlo=EH_BTN_UP;
      btnidhi=EH_BTN_DOWN;
    }
    if (pvdstvalue<0) {
      if ((result=eh_inmgr_update_player(inmgr,device->playerid,btnidlo,0))<0) goto _done_;
    } else if (pvdstvalue>0) {
      if ((result=eh_inmgr_update_player(inmgr,device->playerid,btnidhi,0))<0) goto _done_;
    }
    if (button->dstvalue<0) {
      if ((result=eh_inmgr_update_player(inmgr,device->playerid,btnidlo,1))<0) goto _done_;
    } else if (button->dstvalue>0) {
      if ((result=eh_inmgr_update_player(inmgr,device->playerid,btnidhi,1))<0) goto _done_;
    }
  
  // Two-state buttons, nice and simple.
  } else {
    eh_inmgr_require_playerid(inmgr,device);
    result=eh_inmgr_update_player(inmgr,device->playerid,button->dstbtnid,button->dstvalue);
  }
  
 _done_:;
  inmgr->srcdevid=0;
  inmgr->srcbtnid=0;
  inmgr->srcvalue=0;
  return result;
}

/* Artificial event, straight to output.
 */

int eh_inmgr_artificial_event(struct eh_inmgr *inmgr,int playerid,int btnid,int value) {
  return eh_inmgr_update_player(inmgr,playerid,btnid,value);
}
