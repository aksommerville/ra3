#include "eh_internal.h"

// Any update length longer than this, assume something is broken and clamp it.
// (note that this is frames, without knowing the master rate, it's a vague guess).
#define DUMMY_FRAMEC_LIMIT 2048

// Samples, how much we ask for at each callback.
#define DUMMY_BUFFER_SIZE 1024

struct eh_audio_driver_dummy {
  struct eh_audio_driver hdr;
  int64_t last_update_time;
  int16_t buffer[DUMMY_BUFFER_SIZE];
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
  driver->format=EH_AUDIO_FORMAT_S16N;
  if ((driver->rate<200)||(driver->rate>200000)) driver->rate=44100;
  if (driver->chanc<1) driver->chanc=1;
  else if (driver->chanc>2) driver->chanc=2;
  return 0;
}

/* Play/stop.
 */
 
static void _dummy_play(struct eh_audio_driver *driver,int play) {
  driver->playing=play?1:0;
}

/* Update for a given interval in microseconds.
 */
 
static void dummy_update_us(struct eh_audio_driver *driver,int64_t us) {
  double s=(double)us/1000000.0;
  int framec=(int)(s*driver->rate);
  if (framec<1) framec=1;
  else if (framec>DUMMY_FRAMEC_LIMIT) framec=DUMMY_FRAMEC_LIMIT;
  int samplec=framec*driver->chanc;
  while (samplec>0) {
    // We can get away with simple clamping here because chanc is limited to 2, and the buffer size is always a multiple of 2.
    // If we allowed more channels, we'd have to ensure we clamp to a multiple of chanc.
    int updc=samplec;
    if (updc>DUMMY_BUFFER_SIZE) updc=DUMMY_BUFFER_SIZE;
    driver->delegate.cb_pcm(DRIVER->buffer,updc,driver->delegate.userdata);
    samplec-=updc;
  }
}

/* Update.
 */
 
static int _dummy_update(struct eh_audio_driver *driver) {
  if (!DRIVER->last_update_time) DRIVER->last_update_time=eh_now_real_us();
  int64_t now=eh_now_real_us();
  int64_t elapsedus=now-DRIVER->last_update_time;
  dummy_update_us(driver,elapsedus);
  DRIVER->last_update_time=now;
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
