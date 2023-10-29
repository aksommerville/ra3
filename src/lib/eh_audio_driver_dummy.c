#include "eh_internal.h"

struct eh_audio_driver_dummy {
  struct eh_audio_driver hdr;
};

#define DRIVER ((struct eh_audio_driver_dummy*)driver)

/* Cleanup.
 */
 
static void _dummy_del(struct eh_audio_driver *driver) {
}

/* Init.
 */
 
static int _dummy_init(struct eh_audio_driver *driver,const struct eh_audio_setup *setup) {
  driver->rate=setup->rate;
  driver->chanc=setup->chanc;
  return 0;
}

/* Play/stop.
 */
 
static void _dummy_play(struct eh_audio_driver *driver,int play) {
  driver->playing=play?1:0;
}

/* Update.
 */
 
static int _dummy_update(struct eh_audio_driver *driver) {
  //TODO We do need to keep track of time and pump the callback.
  return 0;
}

/* Type definition.
 */
 
const struct eh_audio_type eh_audio_type_dummy={
  .name="dummy",
  .desc="Fake audio driver that discards output.",
  .objlen=sizeof(struct eh_audio_driver_dummy),
  .appointment_only=0, // OK to auto-select dummy audio, as long as it's the last option.
  .del=_dummy_del,
  .init=_dummy_init,
  .play=_dummy_play,
  .update=_dummy_update,
};
