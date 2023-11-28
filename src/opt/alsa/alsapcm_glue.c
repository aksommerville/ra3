/* alsapcm_glue.c
 * Connects the pre-written alsapcm unit to Emuhost.
 */
 
#include "alsapcm.h"
#include "lib/eh_driver.h"
#include "lib/emuhost.h"

struct eh_audio_driver_alsa {
  struct eh_audio_driver hdr;
  struct alsapcm *alsapcm;
};

#define DRIVER ((struct eh_audio_driver_alsa*)driver)

static void _alsa_del(struct eh_audio_driver *driver) {
  alsapcm_del(DRIVER->alsapcm);
}

static int _alsa_init(struct eh_audio_driver *driver,const struct eh_audio_setup *config) {
  
  struct alsapcm_delegate delegate={
    .userdata=driver,
    .pcm_out=(void*)driver->delegate.cb_pcm,
  };
  struct alsapcm_setup setup={
    .rate=config->rate,
    .chanc=config->chanc,
    .device=config->device,
    .buffersize=config->buffersize,
  };
  
  if (!(DRIVER->alsapcm=alsapcm_new(&delegate,&setup))) return -1;
  
  driver->rate=alsapcm_get_rate(DRIVER->alsapcm);
  driver->chanc=alsapcm_get_chanc(DRIVER->alsapcm);
  driver->format=EH_AUDIO_FORMAT_S16N;
  
  return 0;
}

static void _alsa_play(struct eh_audio_driver *driver,int play) {
  alsapcm_set_running(DRIVER->alsapcm,play);
}

static int _alsa_update(struct eh_audio_driver *driver) {
  // 'update' is for error reporting only
  return alsapcm_update(DRIVER->alsapcm);
}

static int _alsa_lock(struct eh_audio_driver *driver) {
  return alsapcm_lock(DRIVER->alsapcm);
}

static void _alsa_unlock(struct eh_audio_driver *driver) {
  return alsapcm_unlock(DRIVER->alsapcm);
}

const struct eh_audio_type eh_audio_type_alsa={
  .name="alsa",
  .desc="Audio for Linux via ALSA.",
  .objlen=sizeof(struct eh_audio_driver_alsa),
  .appointment_only=0,
  .del=_alsa_del,
  .init=_alsa_init,
  .play=_alsa_play,
  .update=_alsa_update,
  .lock=_alsa_lock,
  .unlock=_alsa_unlock,
};
