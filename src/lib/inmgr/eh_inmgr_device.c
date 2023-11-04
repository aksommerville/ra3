#include "eh_inmgr_internal.h"

/* Cleanup.
 */
 
void eh_inmgr_device_cleanup(struct eh_inmgr_device *device) {
  if (device->buttonv) free(device->buttonv);
}

/* Button list.
 */
 
static int eh_inmgr_device_search_button(const struct eh_inmgr_device *device,int srcbtnid) {
  int lo=0,hi=device->buttonc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (srcbtnid<device->buttonv[ck].srcbtnid) hi=ck;
    else if (srcbtnid>device->buttonv[ck].srcbtnid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

struct eh_inmgr_button *eh_inmgr_device_add_button(struct eh_inmgr_device *device,int srcbtnid) {
  int p=eh_inmgr_device_search_button(device,srcbtnid);
  if (p>=0) return 0;
  p=-p-1;
  
  if (device->buttonc>=device->buttona) {
    int na=device->buttona+16;
    if (na>INT_MAX/sizeof(struct eh_inmgr_button)) return 0;
    void *nv=realloc(device->buttonv,sizeof(struct eh_inmgr_button)*na);
    if (!nv) return 0;
    device->buttonv=nv;
    device->buttona=na;
  }
  
  struct eh_inmgr_button *button=device->buttonv+p;
  memmove(button+1,button,sizeof(struct eh_inmgr_button)*(device->buttonc-p));
  device->buttonc++;
  memset(button,0,sizeof(struct eh_inmgr_button));
  button->srcbtnid=srcbtnid;
  return button;
}

struct eh_inmgr_button *eh_inmgr_device_get_button(struct eh_inmgr_device *device,int srcbtnid) {
  int p=eh_inmgr_device_search_button(device,srcbtnid);
  if (p<0) return 0;
  return device->buttonv+p;
}

/* Apply config optimistically.
 * No driver available, so assume every button exists and has a sensible range.
 */
 
static int eh_inmgr_device_apply_config_optimistically(struct eh_inmgr_device *device) {
  if (!(device->buttonv=calloc(sizeof(struct eh_inmgr_button),device->config->rulec))) return -1;
  device->buttonc=0;
  device->buttona=device->config->rulec;
  const struct eh_inmgr_rule *rule=device->config->rulev;
  struct eh_inmgr_button *button=device->buttonv;
  int i=device->config->rulec;
  for (;i-->0;rule++) {
  
    if (rule->dstbtnid==0) continue;
  
    button->srcbtnid=rule->srcbtnid;
    button->dstbtnid=rule->dstbtnid;
    
    switch (button->dstbtnid) {
      case EH_BTN_DPAD: button->srclo=0; button->srchi=7; button->srcvalue=button->dstvalue=-1; break; // really a gamble. is 1..8 more common?
      case EH_BTN_HORZ: // HORZ/VERT will be useless if the range ends up like -32768..32767 with even a little noise.
      case EH_BTN_VERT: button->srclo=-1; button->srchi=1; break;
      default: button->srclo=1; button->srchi=INT_MAX; // two-states should be pretty reliable tho
    }
    
    button++;
    device->buttonc++;
  }
  return 0;
}

/* Apply config with driver present.
 * Query driver for the buttons actually present, and their ranges.
 */
 
static int eh_inmgr_device_apply_cb(int srcbtnid,uint32_t usage,int lo,int hi,int value,void *userdata) {
  struct eh_inmgr_device *device=userdata;
  const struct eh_inmgr_rule *rule=eh_inmgr_config_get_rule(device->config,srcbtnid);
  if (!rule) return 0;
  if (rule->dstbtnid==0) return 0;
  
  int srclo,srchi;
  switch (rule->dstbtnid) {
    case EH_BTN_DPAD: {
        if (lo>hi-7) return 0;
        srclo=lo;
        srchi=hi;
      } break;
    case EH_BTN_HORZ:
    case EH_BTN_VERT: {
        if (lo>hi-2) return 0;
        int mid=(lo+hi)>>1;
        srclo=(lo+mid)>>1;
        srchi=(hi+mid)>>1;
        if (srclo>=mid) srclo--;
        if (srchi<=mid) srchi++;
        if (srclo<lo) srclo=lo;
        if (srchi>hi) srchi=hi;
      } break;
    default: {
        if (lo>=hi) return 0;
        srclo=lo+1;
        srchi=INT_MAX;
      }
  }
  struct eh_inmgr_button *button=eh_inmgr_device_add_button(device,srcbtnid);
  if (!button) return 0;
  
  button->dstbtnid=rule->dstbtnid;
  button->srclo=srclo;
  button->srchi=srchi;
  button->srcvalue=value;
  button->dstvalue=0; // 0 even if srcvalue says it should be on
  
  return 0;
}
 
static int eh_inmgr_device_apply_config_with_driver(struct eh_inmgr_device *device) {
  if (!device->driver||!device->driver->type->list_buttons) return 0;
  return device->driver->type->list_buttons(device->driver,device->devid,eh_inmgr_device_apply_cb,device);
}

/* Apply config.
 */
 
int eh_inmgr_device_apply_config(struct eh_inmgr_device *device) {
  if (!device) return -1;
  device->buttonc=0;
  if (!device->config||(device->config->rulec<1)) return 0;
  if (device->buttonv) free(device->buttonv);
  device->buttonv=0;
  device->buttonc=0;
  device->buttona=0;
  if (device->driver) {
    if (eh_inmgr_device_apply_config_with_driver(device)<0) return -1;
  } else {
    if (eh_inmgr_device_apply_config_optimistically(device)<0) return -1;
  }
  return 0;
}
