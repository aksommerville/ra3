#include "eh_inmgr_internal.h"

/* Get mapping for source button.
 */
 
int eh_inmgr_get_mapping(struct eh_inmgr *inmgr,int devid,int srcbtnid) {
  struct eh_inmgr_device *device=eh_inmgr_device_by_id(inmgr,devid);
  if (!device) return 0;
  struct eh_inmgr_button *button=eh_inmgr_device_get_button(device,srcbtnid);
  if (!button) return 0;
  return button->dstbtnid;
}

/* Set mapping for source button.
 */
 
int eh_inmgr_set_mapping(struct eh_inmgr *inmgr,int devid,int srcbtnid,int dstbtnid) {
  struct eh_inmgr_device *device=eh_inmgr_device_by_id(inmgr,devid);
  if (!device) return -1;
  
  // In theory we should be able to build live maps in the device without a config.
  // I don't believe the need would ever arise. We make a fake config at connect if needed.
  if (!device->config) return -1;
  
  // To keep things simple here, modify the config and then wipe and rebuild the device buttons.
  // That's a lot of redundant work in the device, but it would really suck for them to go out of sync,
  // which I'm sure would happen if we tried to do it piecemeal.
  struct eh_inmgr_rule *rule=eh_inmgr_config_get_rule(device->config,srcbtnid);
  if (!rule) {
    if (!dstbtnid) return 0;
    if (!(rule=eh_inmgr_config_add_rule(device->config,srcbtnid))) return -1;
    rule->dstbtnid=dstbtnid;
  } else if (rule->dstbtnid==dstbtnid) {
    return 0;
  } else {
    rule->dstbtnid=dstbtnid;
  }
  
  // Now reinit every device that uses this config.
  struct eh_inmgr_device *dev=inmgr->devicev;
  int i=inmgr->devicec;
  for (;i-->0;dev++) {
    if (dev->config!=device->config) continue;
    if (eh_inmgr_device_apply_config(dev)<0) return -1;
  }
  
  // And tell the delegate.
  if (inmgr->delegate.cb_config_dirty) inmgr->delegate.cb_config_dirty(inmgr->delegate.userdata);
  
  return 1;
}

/* Zero outputs for device.
 */
 
int eh_inmgr_zero_outputs_for_device(struct eh_inmgr *inmgr,int devid) {
  struct eh_inmgr_device *device=eh_inmgr_device_by_id(inmgr,devid);
  if (!device) return 0;
  
  inmgr->srcdevid=devid;
  inmgr->srcbtnid=0;
  inmgr->srcvalue=0;
  
  struct eh_inmgr_button *button=device->buttonv;
  int i=device->buttonc;
  for (;i-->0;button++) {
    if (!button->dstvalue) continue;
    
    switch (button->dstbtnid) {
      case EH_BTN_DPAD: button->srcvalue=button->srclo-1; break;
      case EH_BTN_HORZ:
      case EH_BTN_VERT: button->srcvalue=button->srclo+1; break;
      default: button->srcvalue=button->srclo-1;
    }
    
    button->dstvalue=0;
    inmgr->srcbtnid=button->srcbtnid;
    inmgr->srcvalue=button->srcvalue;
    if (eh_inmgr_update_player(inmgr,device->playerid,button->dstbtnid,0)<0) {
      inmgr->srcdevid=0;
      inmgr->srcbtnid=0;
      inmgr->srcvalue=0;
      return -1;
    }
  }
  
  inmgr->srcdevid=0;
  inmgr->srcbtnid=0;
  inmgr->srcvalue=0;
  
  return 0;
}
