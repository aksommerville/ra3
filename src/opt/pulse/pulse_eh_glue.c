#include "pulse_internal.h"
#include "lib/eh_driver.h"
#include "lib/emuhost.h"

/* Instance definition.
 */
 
struct eh_audio_driver_pulse {
  struct eh_audio_driver hdr;
  struct pulse *pulse;
};

#define DRIVER ((struct eh_audio_driver_pulse*)driver)

/* Hooks.
 */
 
static void _pulse_del(struct eh_audio_driver *driver) {
  pulse_del(DRIVER->pulse);
}

//TODO Change the pulse core so we can use its callback directly. This extra step adds nothing.
static void _pulse_cb(int16_t *v,int c,struct pulse *pulse) {
  struct eh_audio_driver *driver=pulse_get_userdata(pulse);
  driver->delegate.cb_pcm(v,c,driver->delegate.userdata);
}

static int _pulse_init(
  struct eh_audio_driver *driver,
  const struct eh_audio_setup *config
) {
  //TODO Can we get appname from the caller?
  if (config) {
    driver->rate=config->rate;
    driver->chanc=config->chanc;
  }
  if (!driver->rate) driver->rate=44100;
  if (!driver->chanc) driver->chanc=2;
  if (!(DRIVER->pulse=pulse_new(driver->rate,driver->chanc,_pulse_cb,driver,""))) return -1;
  driver->rate=pulse_get_rate(DRIVER->pulse);
  driver->chanc=pulse_get_chanc(DRIVER->pulse);
  driver->format=EH_AUDIO_FORMAT_S16N;
  return 0;
}

static void _pulse_play(struct eh_audio_driver *driver,int play) {
  pulse_play(DRIVER->pulse,play);
  driver->playing=play?1:0;
}

static int _pulse_lock(struct eh_audio_driver *driver) {
  return pulse_lock(DRIVER->pulse);
}

static void _pulse_unlock(struct eh_audio_driver *driver) {
  pulse_unlock(DRIVER->pulse);
}

/* Type definition.
 */
 
const struct eh_audio_type eh_audio_type_pulse={
  .name="pulse",
  .desc="Audio for Linux via PulseAudio, preferred for desktop systems.",
  .objlen=sizeof(struct eh_audio_driver_pulse),
  .del=_pulse_del,
  .init=_pulse_init,
  .play=_pulse_play,
  .lock=_pulse_lock,
  .unlock=_pulse_unlock,
};
