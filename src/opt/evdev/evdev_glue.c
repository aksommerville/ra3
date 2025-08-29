/* evdev_glue.c
 * Connects the pre-written 'evdev' unit to Emuhost.
 * (I did modify the core a little, to make it use an Emuhost delegate at least).
 */
 
#include "evdev.h"
#include "lib/eh_driver.h"

#define FMN_EVDEV_UPDATE_TIMEOUT_MS 2

struct eh_input_driver_evdev {
  struct eh_input_driver hdr;
  struct evdev *evdev;
  char name_storage[256];
};

#define DRIVER ((struct eh_input_driver_evdev*)driver)

static void _evdev_del(struct eh_input_driver *driver) {
  evdev_del(DRIVER->evdev);
}

static int _evdev_init(struct eh_input_driver *driver) {
  struct evdev_setup setup={0};
  evdev_setup_default(&setup);
  if (!(DRIVER->evdev=evdev_new(&driver->delegate,&setup))) return -1;
  return 0;
}

static int _evdev_update(struct eh_input_driver *driver) {
  return evdev_update(DRIVER->evdev,FMN_EVDEV_UPDATE_TIMEOUT_MS);
}

static const char *_evdev_get_ids(int *vid,int *pid,int *version,struct eh_input_driver *driver,int devid) {
  struct evdev_device *device=evdev_device_by_devid(DRIVER->evdev,devid);
  if (!device) return 0;
  if (vid) *vid=evdev_device_get_vid(device);
  if (pid) *pid=evdev_device_get_pid(device);
  if (version) *version=evdev_device_get_version(device);
  int namec=evdev_device_get_name(DRIVER->name_storage,sizeof(DRIVER->name_storage),device);
  if ((namec<1)||(namec>=sizeof(DRIVER->name_storage))) DRIVER->name_storage[0]=0;
  else DRIVER->name_storage[namec]=0;
  return DRIVER->name_storage;
}

struct evdev_for_each_button_context {
  int (*cb)(int btnid,uint32_t usage,int lo,int hi,int value,void *userdata);
  void *userdata;
};

static int _evdev_for_each_button_cb(int type,int code,int usage,int lo,int hi,int value,void *userdata) {
  struct evdev_for_each_button_context *ctx=userdata;
  return ctx->cb((type<<16)|code,usage,lo,hi,value,ctx->userdata);
}

static int _evdev_for_each_button(
  struct eh_input_driver *driver,
  int devid,
  int (*cb)(int btnid,uint32_t usage,int lo,int hi,int value,void *userdata),
  void *userdata
) {
  struct evdev_device *device=evdev_device_by_devid(DRIVER->evdev,devid);
  if (!device) return 0;
  struct evdev_for_each_button_context ctx={
    .cb=cb,
    .userdata=userdata,
  };
  return evdev_device_enumerate(device,_evdev_for_each_button_cb,&ctx);
}

static int _evdev_has_device(struct eh_input_driver *driver,int devid) {
  return evdev_device_by_devid(DRIVER->evdev,devid)?1:0;
}

const struct eh_input_type eh_input_type_evdev={
  .name="evdev",
  .desc="Linux input via evdev.",
  .objlen=sizeof(struct eh_input_driver_evdev),
  .appointment_only=0,
  .del=_evdev_del,
  .init=_evdev_init,
  .update=_evdev_update,
  .get_ids=_evdev_get_ids,
  .list_buttons=_evdev_for_each_button,
  .has_device=_evdev_has_device,
};
