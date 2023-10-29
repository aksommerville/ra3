/* akdrm_glue.c
 * Connects the pre-written 'akdrm' unit to Emuhost.
 */

#include "eh_drm_internal.h"
#include "lib/eh_driver.h"
#include <stdio.h>

struct eh_video_driver_drm {
  struct eh_video_driver hdr;
};

#define DRIVER ((struct eh_video_driver_drm*)driver)

static void _drm_del(struct eh_video_driver *driver) {
  eh_drm_quit();
}

static int _drm_init(struct eh_video_driver *driver,const struct eh_video_setup *config) {

  if (eh_drm_init()<0) {
    fprintf(stderr,"eh_drm_init failed\n");
    return -1;
  }
  
  driver->w=eh_drm.w;
  driver->h=eh_drm.h;

  return 0;
}

static int _drm_begin(struct eh_video_driver *driver) {
  return 0;
}

static int _drm_end(struct eh_video_driver *driver) {
  eh_drm_swap();
  return 0;
}

const struct eh_video_type eh_video_type_drm={
  .name="drm",
  .desc="Linux Direct Rendering Manager, for systems without an X server.",
  .objlen=sizeof(struct eh_video_driver_drm),
  .appointment_only=0,
  .provides_keyboard=0,
  .del=_drm_del,
  .init=_drm_init,
  .begin=_drm_begin,
  .end=_drm_end,
};
