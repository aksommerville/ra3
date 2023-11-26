#include "eh_inmgr_internal.h"

/* Delete.
 */

void eh_inmgr_del(struct eh_inmgr *inmgr) {
  if (!inmgr) return;
  if (inmgr->refc-->1) return;
  
  if (inmgr->listenerv) free(inmgr->listenerv);
  if (inmgr->suspendv) free(inmgr->suspendv);
  if (inmgr->playerv) free(inmgr->playerv);
  
  if (inmgr->devicev) {
    while (inmgr->devicec-->0) eh_inmgr_device_cleanup(inmgr->devicev+inmgr->devicec);
    free(inmgr->devicev);
  }
  
  if (inmgr->configv) {
    while (inmgr->configc-->0) eh_inmgr_config_cleanup(inmgr->configv+inmgr->configc);
    free(inmgr->configv);
  }
  
  free(inmgr);
}

/* Retain.
 */
 
int eh_inmgr_ref(struct eh_inmgr *inmgr) {
  if (!inmgr) return -1;
  if (inmgr->refc<1) return -1;
  if (inmgr->refc==INT_MAX) return -1;
  inmgr->refc++;
  return 0;
}

/* Set player count.
 * For now at least, this is only mutable at init.
 * If we want to allow it after, we have to be mindful of state.
 */
 
static int eh_inmgr_set_player_count(struct eh_inmgr *inmgr,int playerc) {
  if (playerc<0) playerc=0;
  else if (playerc>EH_INMGR_PLAYER_LIMIT) playerc=EH_INMGR_PLAYER_LIMIT;
  if (!(inmgr->playerv=calloc(playerc,sizeof(struct eh_inmgr_player)))) return -1;
  inmgr->playerc=playerc;
  struct eh_inmgr_player *player=inmgr->playerv;
  int i=0; for (;i<playerc;i++,player++) player->playerid=i+1;
  return 0;
}

/* New.
 */

struct eh_inmgr *eh_inmgr_new(const struct eh_inmgr_delegate *delegate,int playerc) {
  struct eh_inmgr *inmgr=calloc(1,sizeof(struct eh_inmgr));
  if (!inmgr) return 0;
  
  inmgr->refc=1;
  if (delegate) memcpy(&inmgr->delegate,delegate,sizeof(struct eh_inmgr_delegate));
  if (eh_inmgr_set_player_count(inmgr,playerc)<0) {
    eh_inmgr_del(inmgr);
    return 0;
  }
  
  return inmgr;
}

/* Trivial accessors.
 */

void *eh_inmgr_get_userdata(const struct eh_inmgr *inmgr) {
  return inmgr->delegate.userdata;
}

struct eh_inmgr_player *eh_inmgr_player_by_id(const struct eh_inmgr *inmgr,int playerid) {
  if (!inmgr) return 0;
  if ((playerid<1)||(playerid>inmgr->playerc)) return 0;
  return inmgr->playerv+playerid-1;
}

int eh_inmgr_get_player_state(const struct eh_inmgr *inmgr,int playerid) {
  if (!inmgr) return 0;
  if (playerid<0) return 0;
  if (playerid>inmgr->playerc) return 0;
  if (playerid) return inmgr->playerv[playerid-1].state;
  int state=0;
  const struct eh_inmgr_player *player=inmgr->playerv;
  int i=inmgr->playerc;
  for (;i-->0;player++) state|=player->state;
  return state;
}

/* Add listener, private.
 */
 
static struct eh_inmgr_listener *eh_inmgr_add_listener(struct eh_inmgr *inmgr,const struct eh_inmgr_delegate *delegate) {
  
  // IDs are sequential because we only append.
  // IDs reset when the list empties.
  // If they reach INT_MAX, we fail, that seems reasonable.
  int listenerid=1;
  if (inmgr->listenerc>0) {
    listenerid=inmgr->listenerv[inmgr->listenerc-1].listenerid;
    if (listenerid==INT_MAX) return 0;
    listenerid++;
  }
  
  if (inmgr->listenerc>=inmgr->listenera) {
    int na=inmgr->listenera+8;
    if (na>INT_MAX/sizeof(struct eh_inmgr_listener)) return 0;
    void *nv=realloc(inmgr->listenerv,sizeof(struct eh_inmgr_listener)*na);
    if (!nv) return 0;
    inmgr->listenerv=nv;
    inmgr->listenera=na;
  }
  
  struct eh_inmgr_listener *listener=inmgr->listenerv+inmgr->listenerc++;
  memset(listener,0,sizeof(struct eh_inmgr_listener));
  listener->listenerid=listenerid;
  memcpy(&listener->delegate,delegate,sizeof(struct eh_inmgr_delegate));
  
  return listener;
}

/* Add listener, public.
 */

int eh_inmgr_listen_source(struct eh_inmgr *inmgr,int devid,const struct eh_inmgr_delegate *delegate) {
  if (!inmgr||(devid<0)||!delegate) return -1;
  struct eh_inmgr_listener *listener=eh_inmgr_add_listener(inmgr,delegate);
  if (!listener) return -1;
  listener->reqid=devid;
  listener->reqtype=EH_INMGR_REQTYPE_DEVICE;
  
  if (delegate->cb_event) {
    struct eh_inmgr_device *device=inmgr->devicev;
    int i=inmgr->devicec;
    for (;i-->0;device++) {
      if (!devid||(device->devid==devid)) {
        struct eh_inmgr_event event={.srcdevid=device->devid};
        if (delegate->cb_event(delegate->userdata,&event)<0) {
          eh_inmgr_unlisten(inmgr,listener->listenerid);
          return -1;
        }
      }
    }
  }
  
  return listener->listenerid;
}

int eh_inmgr_listen_player(struct eh_inmgr *inmgr,int playerid,const struct eh_inmgr_delegate *delegate) {
  if (!inmgr||(playerid<0)||(playerid>inmgr->playerc)||!delegate) return -1;
  struct eh_inmgr_listener *listener=eh_inmgr_add_listener(inmgr,delegate);
  if (!listener) return -1;
  listener->reqid=playerid;
  listener->reqtype=EH_INMGR_REQTYPE_PLAYER;
  return listener->listenerid;
}

int eh_inmgr_listen(struct eh_inmgr *inmgr,const struct eh_inmgr_delegate *delegate) {
  if (!inmgr||!delegate) return -1;
  struct eh_inmgr_listener *listener=eh_inmgr_add_listener(inmgr,delegate);
  if (!listener) return -1;
  listener->reqid=0;
  listener->reqtype=EH_INMGR_REQTYPE_ALL;
  
  if (delegate->cb_event) {
    struct eh_inmgr_device *device=inmgr->devicev;
    int i=inmgr->devicec;
    for (;i-->0;device++) {
      struct eh_inmgr_event event={.srcdevid=device->devid};
      if (delegate->cb_event(delegate->userdata,&event)<0) {
        eh_inmgr_unlisten(inmgr,listener->listenerid);
        return -1;
      }
    }
  }
  return listener->listenerid;
}

/* Remove listener.
 */
 
int eh_inmgr_unlisten(struct eh_inmgr *inmgr,int listenerid) {
  int i=inmgr->listenerc;
  struct eh_inmgr_listener *listener=inmgr->listenerv+i-1;
  for (;i-->0;listener--) {
    if (listener->listenerid==listenerid) {
      inmgr->listenerc--;
      memmove(listener,listener+1,sizeof(struct eh_inmgr_listener)*(inmgr->listenerc-i));
      return 0;
    }
  }
  return -1;
}

/* Device suspension, private primitives.
 */
 
static int eh_inmgr_suspendv_search(const struct eh_inmgr *inmgr,int devid) {
  int lo=0,hi=inmgr->suspendc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (devid<inmgr->suspendv[ck]) hi=ck;
    else if (devid>inmgr->suspendv[ck]) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int eh_inmgr_suspendv_insert(struct eh_inmgr *inmgr,int p,int devid) {
  if ((p<0)||(p>inmgr->suspendc)) return -1;
  if (p&&(devid<=inmgr->suspendv[p-1])) return -1;
  if ((p<inmgr->suspendc)&&(devid>=inmgr->suspendv[p])) return -1;
  
  if (inmgr->suspendc>=inmgr->suspenda) {
    int na=inmgr->suspenda+8;
    if (na>INT_MAX/sizeof(int)) return -1;
    void *nv=realloc(inmgr->suspendv,sizeof(int)*na);
    if (!nv) return -1;
    inmgr->suspendv=nv;
    inmgr->suspenda=na;
  }
  
  int *dst=inmgr->suspendv+p;
  memmove(dst+1,dst,sizeof(int)*(inmgr->suspendc-p));
  *dst=devid;
  inmgr->suspendc++;
  
  return 0;
}

static void eh_inmgr_suspendv_remove(struct eh_inmgr *inmgr,int p) {
  if ((p<0)||(p>=inmgr->suspendc)) return;
  inmgr->suspendc--;
  memmove(inmgr->suspendv+p,inmgr->suspendv+p+1,sizeof(int)*(inmgr->suspendc-p));
}

/* Suspend list, public.
 */

int eh_inmgr_enable_device(struct eh_inmgr *inmgr,int devid,int enable) {
  int p=eh_inmgr_suspendv_search(inmgr,devid);
  if (p>=0) {
    if (!enable) return 0;
    eh_inmgr_suspendv_remove(inmgr,p);
  } else {
    if (enable) return 0;
    p=-p-1;
    if (eh_inmgr_suspendv_insert(inmgr,p,devid)<0) return -1;
    if (eh_inmgr_zero_outputs_for_device(inmgr,devid)<0) return -1;
  }
  return 0;
}

int eh_inmgr_device_is_enabled(const struct eh_inmgr *inmgr,int devid) {
  int p=eh_inmgr_suspendv_search(inmgr,devid);
  return (p<0)?1:0;
}

/* Device list.
 */
 
int eh_inmgr_devicev_search(const struct eh_inmgr *inmgr,int devid) {
  int lo=0,hi=inmgr->devicec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (devid<inmgr->devicev[ck].devid) hi=ck;
    else if (devid>inmgr->devicev[ck].devid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

struct eh_inmgr_device *eh_inmgr_devicev_insert(struct eh_inmgr *inmgr,int p,int devid) {
  if ((p<0)||(p>inmgr->devicec)) return 0;
  if (p&&(devid<=inmgr->devicev[p-1].devid)) return 0;
  if ((p<inmgr->devicec)&&(devid>=inmgr->devicev[p].devid)) return 0;
  
  if (inmgr->devicec>=inmgr->devicea) {
    int na=inmgr->devicea+8;
    if (na>INT_MAX/sizeof(struct eh_inmgr_device)) return 0;
    void *nv=realloc(inmgr->devicev,sizeof(struct eh_inmgr_device)*na);
    if (!nv) return 0;
    inmgr->devicev=nv;
    inmgr->devicea=na;
  }
  
  struct eh_inmgr_device *device=inmgr->devicev+p;
  memmove(device+1,device,sizeof(struct eh_inmgr_device)*(inmgr->devicec-p));
  inmgr->devicec++;
  memset(device,0,sizeof(struct eh_inmgr_device));
  device->devid=devid;
  
  return device;
}

void eh_inmgr_devicev_remove(struct eh_inmgr *inmgr,int p) {
  if ((p<0)||(p>=inmgr->devicec)) return;
  eh_inmgr_device_cleanup(inmgr->devicev+p);
  inmgr->devicec--;
  memmove(inmgr->devicev+p,inmgr->devicev+p+1,sizeof(struct eh_inmgr_device)*(inmgr->devicec-p));
}
 
/* Device list, public conveniences.
 */
 
struct eh_inmgr_device *eh_inmgr_device_by_id(const struct eh_inmgr *inmgr,int devid) {
  if (!inmgr) return 0;
  int p=eh_inmgr_devicev_search(inmgr,devid);
  if (p<0) return 0;
  return inmgr->devicev+p;
}
 
struct eh_input_driver *eh_inmgr_driver_for_devid(const struct eh_inmgr *inmgr,int devid) {
  if (!inmgr) return 0;
  int p=eh_inmgr_devicev_search(inmgr,devid);
  if (p<0) return 0;
  return inmgr->devicev[p].driver;
}

int eh_inmgr_devids_for_driver(
  const struct eh_inmgr *inmgr,
  const struct eh_input_driver *driver,
  int (*cb)(int devid,void *userdata),
  void *userdata
) {
  if (!inmgr||!cb) return -1;
  const struct eh_inmgr_device *device=inmgr->devicev;
  int i=inmgr->devicec,err;
  for (;i-->0;device++) {
    if (device->driver!=driver) continue;
    if (err=cb(device->devid,userdata)) return err;
  }
  return 0;
}

const char *eh_inmgr_device_name(const struct eh_inmgr *inmgr,int devid) {
  if (!inmgr) return 0;
  int p=eh_inmgr_devicev_search(inmgr,devid);
  if (p<0) return 0;
  struct eh_input_driver *driver=inmgr->devicev[p].driver;
  if (!driver) return "System Keyboard"; // No driver, hopefully means it's supplied by the video driver.
  if (!driver->type->get_ids) return 0;
  uint16_t vid,pid;
  return driver->type->get_ids(&vid,&pid,driver,devid);
}

int eh_inmgr_device_ids(int *vendor,int *product,int *version,const struct eh_inmgr *inmgr,int devid) {
  if (!inmgr) return -1;
  int p=eh_inmgr_devicev_search(inmgr,devid);
  if (p<0) return -1;
  struct eh_input_driver *driver=inmgr->devicev[p].driver;
  if (!driver) return -1;
  if (!driver->type->get_ids) return -1;
  uint16_t vid=0,pid=0;
  driver->type->get_ids(&vid,&pid,driver,devid);
  if (vendor) *vendor=vid;
  if (product) *product=pid;
  if (version) *version=0; // v3 drivers don't supply this
  return 0;
}

/* Enumerate capabilities.
 */
 
static int eh_inmgr_device_enumerate_keyboard(
  int (*cb)(void *userdata,int btnid,int value,int lo,int hi,int hidusage),
  void *userdata
) {
  // Window managers that supply a system keyboard are expected to use HID page 7 as btnid.
  // It's important that we enumerate these, since sometimes there's USB keyboards delivering generically, and they should map the same way.
  int err,i;
  #define RANGE(first,last) for (i=first;i<=last;i++) if (err=cb(userdata,0x00070000|i,0,0,2,0x00070000|i)) return err;
  RANGE(0x04,0x63) // letters, arrows, keypad, most of the keyboard
  RANGE(0xe0,0xe7) // modifiers
  #undef RANGE
  return 0;
}

struct eh_inmgr_enumerate_adapter {
  int (*cb)(void *userdata,int btnid,int value,int lo,int hi,int hidusage);
  void *userdata;
};

static int eh_inmgr_enumerate_cb(int btnid,uint32_t usage,int lo,int hi,int value,void *userdata) {
  struct eh_inmgr_enumerate_adapter *ctx=userdata;
  return ctx->cb(ctx->userdata,btnid,value,lo,hi,usage);
}

int eh_inmgr_device_enumerate(
  const struct eh_inmgr *inmgr,
  int devid,
  int (*cb)(void *userdata,int btnid,int value,int lo,int hi,int hidusage),
  void *userdata
) {
  if (!inmgr) return 0;
  int p=eh_inmgr_devicev_search(inmgr,devid);
  if (p<0) return 0;
  struct eh_input_driver *driver=inmgr->devicev[p].driver;
  if (!driver) {
    return eh_inmgr_device_enumerate_keyboard(cb,userdata);
  }
  if (!driver->type->list_buttons) return 0;
  struct eh_inmgr_enumerate_adapter ctx={ // v2 had a different shape for the enumerate callback, and inmgr is copied from v2.
    .cb=cb,
    .userdata=userdata,
  };
  return driver->type->list_buttons(driver,devid,eh_inmgr_enumerate_cb,&ctx);
}

/* Config list.
 */
 
static void eh_inmgr_repoint_device_configs(void *newconfigv,void *oldconfigv,struct eh_inmgr_device *device,int devicec) {
  int d=((char*)newconfigv)-((char*)oldconfigv);
  for (;devicec-->0;device++) {
    device->config=(struct eh_inmgr_config*)(((char*)device->config)+d);
  }
}
 
struct eh_inmgr_config *eh_inmgr_configv_append(struct eh_inmgr *inmgr) {
  if (inmgr->configc>=inmgr->configa) {
    int na=inmgr->configa+8;
    if (na>INT_MAX/sizeof(struct eh_inmgr_config)) return 0;
    void *nv=realloc(inmgr->configv,sizeof(struct eh_inmgr_config)*na);
    if (!nv) return 0;
    eh_inmgr_repoint_device_configs(nv,inmgr->configv,inmgr->devicev,inmgr->devicec);
    inmgr->configv=nv;
    inmgr->configa=na;
  }
  struct eh_inmgr_config *config=inmgr->configv+inmgr->configc++;
  memset(config,0,sizeof(struct eh_inmgr_config));
  return config;
}

void eh_inmgr_configv_remove(struct eh_inmgr *inmgr,struct eh_inmgr_config *config) {
  if (!inmgr||!config) return;
  int p=config-inmgr->configv;
  if ((p<0)||(p>=inmgr->configc)) return;
  eh_inmgr_config_cleanup(config);
  inmgr->configc--;
  memmove(config,config+1,sizeof(struct eh_inmgr_config)*(inmgr->configc-p));
  
  struct eh_inmgr_device *device=inmgr->devicev;
  int i=inmgr->devicec;
  for (;i-->0;device++) {
    if (device->config==config) {
      device->config=0;
    } else if (device->config>config) {
      device->config--;
    }
  }
}
