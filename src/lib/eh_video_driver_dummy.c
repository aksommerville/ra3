#include "eh_internal.h"

struct eh_video_driver_dummy {
  struct eh_video_type hdr;
};

#define DRIVER ((struct eh_video_driver_dummy*)driver)

/* Cleanup.
 */
 
static void _dummy_del(struct eh_video_driver *driver) {
}

/* Init.
 */
 
static int _dummy_init(struct eh_video_driver *driver,const struct eh_video_setup *setup) {
  driver->w=setup->w;
  driver->h=setup->h;
  driver->rate=setup->rate;
  driver->fullscreen=1;
  return 0;
}

/* Frame control.
 */
 
static int _dummy_begin(struct eh_video_driver *driver) {
  return 0;
}

static int _dummy_end(struct eh_video_driver *driver) {
  return 0;
}

/* Type definition.
 */
 
const struct eh_video_type eh_video_type_dummy={
  .name="dummy",
  .desc="Fake video driver that discards output.",
  .objlen=sizeof(struct eh_video_driver_dummy),
  .appointment_only=1,
  .del=_dummy_del,
  .init=_dummy_init,
  .begin=_dummy_begin,
  .end=_dummy_end,
};
