#include "eh_internal.h"
#include "opt/serial/serial.h"
#include <unistd.h>

/* Quit.
 */
 
void eh_drivers_quit() {
  if (eh.audio) {
    eh.audio->type->play(eh.audio,0);
    eh_audio_lock();
  }
  eh_render_del(eh.render);
  eh_audio_driver_del(eh.audio);
  eh_video_driver_del(eh.video);
  if (eh.inputv) {
    while (eh.inputc-->0) eh_input_driver_del(eh.inputv[eh.inputc]);
    free(eh.inputv);
  }
  eh_aucvt_cleanup(&eh.aucvt);
  inmgr_quit();
  fakews_del(eh.fakews);
}

/* Choose and init video driver.
 */
 
static int eh_drivers_init_video_type(const struct eh_video_type *type) {
  if (!type) return -1;
  if (!eh.video_drivers&&type->appointment_only) return 0;
  struct eh_video_delegate delegate={
    .userdata=0,
    .cb_close=eh_cb_close,
    .cb_resize=eh_cb_resize,
    .cb_focus=eh_cb_focus,
    .cb_key=eh_cb_key,
    .cb_text=eh_cb_text,
  };
  struct eh_video_setup setup={
    .w=eh.fbcrop.w,
    .h=eh.fbcrop.h,
    .rate=eh.delegate.video_rate,
    .fullscreen=eh.fullscreen,
    .title=eh.delegate.name,
    .iconrgba=eh.delegate.iconrgba,
    .iconw=eh.delegate.iconw,
    .iconh=eh.delegate.iconh,
    .screen=eh.prefer_screen,
    .device=eh.video_device,
  };
  if (!(eh.video=eh_video_driver_new(type,&delegate,&setup))) {
    fprintf(stderr,"%s: Failed to instantiate video driver '%s'.\n",eh.exename,type->name);
    return 0;
  }
  return 1;
}
 
static int eh_drivers_init_video_name(const char *name,int namec,void *dummy) {
  return eh_drivers_init_video_type(eh_video_type_by_name(name,namec));
}
 
static int eh_drivers_init_video_index(int p) {
  return eh_drivers_init_video_type(eh_video_type_by_index(p));
}
 
static int eh_drivers_init_video() {
  
  // Instantiate video driver.
  int err=-1;
  if (eh.video_drivers) {
    err=sr_string_split(eh.video_drivers,-1,',',eh_drivers_init_video_name,0);
  } else {
    int p=0; for (;;p++) {
      if (err=eh_drivers_init_video_index(p)) break;
    }
  }
  if ((err<1)||!eh.video) {
    if (err!=-2) fprintf(stderr,"%s: Failed to instantiate video driver.\n",eh.exename);
    return -2;
  }
  fprintf(stderr,"%s: Using video driver '%s'.\n",eh.exename,eh.video->type->name);
  
  // Instantiate renderer.
  if (!(eh.render=eh_render_new())) {
    fprintf(stderr,"%s: Failed to instantiate renderer.\n",eh.exename);
    return -2;
  }
  eh_render_set_pixel_refresh(eh.render,eh.pixel_refresh);
  
  // Apportion an input device id for the keyboard if there is one.
  if (eh.video->type->provides_keyboard) {
    eh.devid_keyboard=eh_input_devid_next();
    inmgr_connect_keyboard(eh.devid_keyboard);
  }
  
  return 0;
}

/* Reinitialize audio driver, in flight.
 */
 
struct eh_audio_driver *eh_audio_reinit(const struct eh_audio_type *type,const struct eh_audio_setup *setup) {
  if (!type||!setup) return 0;
  if (eh.audio) {
    if (eh.audio->type->play) eh.audio->type->play(eh.audio,0);
    if (eh.audio->type->lock) eh.audio->type->lock(eh.audio);
    eh_audio_driver_del(eh.audio);
  }
  struct eh_audio_delegate delegate={
    .userdata=0,
    .cb_pcm=eh_cb_pcm,
  };
  struct eh_audio_setup adjsetup=*setup;
  adjsetup.buffersize=1024;
  if (!(eh.audio=eh_audio_driver_new(type,&delegate,&adjsetup))) {
    fprintf(stderr,
      "%s: !!! Failed to reinitialize audio !!! driver=%s device=%s rate=%d chanc=%d format=%d\n",
      eh.exename,type->name,setup->device,setup->rate,setup->chanc,setup->format
    );
    return 0;
  }
  eh.audio_rate=setup->rate;
  eh.audio_chanc=setup->chanc;
  eh_config_set_string(&eh.audio_drivers,type->name,-1);
  eh_config_set_string(&eh.audio_device,setup->device,-1);
  eh_config_save();
  return eh.audio;
}

/* Choose and init audio driver.
 */
 
static int eh_drivers_init_audio_type(const struct eh_audio_type *type) {
  if (!type) return -1;
  if (!eh.audio_drivers&&type->appointment_only) return 0;
  struct eh_audio_delegate delegate={
    .userdata=0,
    .cb_pcm=eh_cb_pcm,
  };
  struct eh_audio_setup setup={
    .rate=eh.audio_rate,
    .chanc=eh.audio_chanc,
    .buffersize=2048,
  };
  if (!setup.rate&&!(setup.rate=eh.delegate.audio_rate)) setup.rate=44100;
  if (!setup.chanc&&!(setup.chanc=eh.delegate.audio_chanc)) setup.chanc=1;
  if (!(eh.audio=eh_audio_driver_new(type,&delegate,&setup))) {
    fprintf(stderr,"%s: Failed to instantiate audio driver '%s'.\n",eh.exename,type->name);
    return 0;
  }
  return 1;
}
 
static int eh_drivers_init_audio_name(const char *name,int namec,void *dummy) {
  return eh_drivers_init_audio_type(eh_audio_type_by_name(name,namec));
}

static int eh_drivers_init_audio_index(int p) {
  return eh_drivers_init_audio_type(eh_audio_type_by_index(p));
}
 
static int eh_drivers_init_audio() {
  int err=-1;
  if (eh.audio_drivers) {
    err=sr_string_split(eh.audio_drivers,-1,',',eh_drivers_init_audio_name,0);
  } else {
    int p=0; for (;;p++) {
      if (err=eh_drivers_init_audio_index(p)) break;
    }
  }
  if ((err<1)||!eh.audio) {
    if (err!=-2) fprintf(stderr,"%s: Failed to instantiate audio driver.\n",eh.exename);
    return -2;
  }
  fprintf(stderr,"%s: Using audio driver '%s' rate=%d chanc=%d.\n",eh.exename,eh.audio->type->name,eh.audio->rate,eh.audio->chanc);
  
  if (!eh.delegate.generate_pcm) {
    int srcrate=eh.delegate.audio_rate;
    int srcchanc=eh.delegate.audio_chanc;
    int srcfmt=eh.delegate.audio_format;
    if ((err=eh_aucvt_init(&eh.aucvt,
      eh.audio->rate,eh.audio->chanc,eh.audio->format,
      srcrate,srcchanc,srcfmt
    ))<0) {
      if (err!=-2) fprintf(stderr,
        "%s: Failed to initialize audio resampler. in=(%d,%d,%d) out=(%d,%d,%d)\n",
        eh.exename,srcrate,srcchanc,srcfmt,
        eh.audio->rate,eh.audio->chanc,eh.audio->format
      );
      return -2;
    }
    fprintf(stderr,
      "%s: Initialized audio resampler. in=(%d,%d,%d) out=(%d,%d,%d)\n",
      eh.exename,srcrate,srcchanc,srcfmt,
      eh.audio->rate,eh.audio->chanc,eh.audio->format
    );
  }
  
  return 0;
}

/* Choose and init input drivers.
 */
 
static int eh_drivers_init_input_type(const struct eh_input_type *type) {
  if (!type) return -1;
  if (!eh.input_drivers&&type->appointment_only) return 0;
  if (eh.inputc>=eh.inputa) {
    int na=eh.inputa+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(eh.inputv,sizeof(void*)*na);
    if (!nv) return -1;
    eh.inputv=nv;
    eh.inputa=na;
  }
  struct eh_input_delegate delegate={
    .userdata=0,
    .cb_connect=eh_cb_connect,
    .cb_disconnect=eh_cb_disconnect,
    .cb_button=eh_cb_button,
  };
  struct eh_input_driver *driver=eh_input_driver_new(type,&delegate);
  if (!driver) {
    fprintf(stderr,"%s: Failed to instantiate input driver '%s'.\n",eh.exename,type->name);
    return 0;
  }
  fprintf(stderr,"%s: Using input driver '%s'.\n",eh.exename,type->name);
  eh.inputv[eh.inputc++]=driver;
  return 0;
}
 
static int eh_drivers_init_input_name(const char *name,int namec,void *dummy) {
  return eh_drivers_init_input_type(eh_input_type_by_name(name,namec));
}

static int eh_drivers_init_input_index(int p) {
  return eh_drivers_init_input_type(eh_input_type_by_index(p));
}
 
static int eh_drivers_init_input() {

  // Instantiate drivers.
  int err=0;
  if (eh.input_drivers) {
    sr_string_split(eh.input_drivers,-1,',',eh_drivers_init_input_name,0);
  } else {
    int p=0; for (;;p++) {
      if (eh_drivers_init_input_index(p)<0) break;
    }
  }
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Fatal error instantiating input drivers.\n",eh.exename);
    return -2;
  }
  
  // You'd expect to see inmgr instantiation here, but we do that earlier, so it exists before video initializes.
  
  return 0;
}

/* Init.
 */
 
int eh_drivers_init() {

  // Instantiate and configure inmgr.
  // This would be part of eh_driver_init_input(), but video uses it too.
  if (inmgr_init()<0) {
    fprintf(stderr,"%s: Failed to initialize input manager.\n",eh.exename);
    return -2;
  }
  inmgr_set_player_count(eh.delegate.playerc);
  #define _(tag) inmgr_set_signal(INMGR_BTN_##tag,eh_cb_##tag);
  EH_FOR_EACH_SIGNAL
  #undef _

  int err;
  if (
    ((err=eh_drivers_init_video())<0)||
    ((err=eh_drivers_init_audio())<0)||
    ((err=eh_drivers_init_input())<0)
  ) return err;
  
  if (eh.romassist_port) {
    if (!(eh.fakews=fakews_new(
      eh.romassist_host,-1,
      eh.romassist_port,
      eh.delegate.use_menu_role?"/ws/menu":"/ws/game",8,
      eh_cb_ws_connect,
      eh_cb_ws_disconnect,
      eh_cb_ws_message,
      0
    ))) return -1;
  }
  
  return 0;
}

/* Accessors to audio.
 */
 
int eh_audio_get_rate() {
  if (eh.delegate.generate_pcm) return eh.audio->rate;
  if (eh.delegate.audio_rate) return eh.delegate.audio_rate;
  return eh.audio->rate;
}

int eh_audio_get_chanc() {
  if (eh.delegate.generate_pcm) return eh.audio->chanc;
  if (eh.delegate.audio_chanc) return eh.delegate.audio_chanc;
  return eh.audio->chanc;
}

int eh_audio_get_format() {
  if (eh.delegate.generate_pcm) return eh.audio->format;
  if (eh.delegate.audio_format) return eh.delegate.audio_format;
  return eh.audio->format;
}

void eh_audio_write(const void *v,int framec) {
  if (eh.delegate.generate_pcm) return;
  int samplec=framec*eh.delegate.audio_chanc;
  int samplesize;
  switch (eh.delegate.audio_format) {
    case EH_AUDIO_FORMAT_S8:
      samplesize=1;
      break;
    case EH_AUDIO_FORMAT_S16N:
    case EH_AUDIO_FORMAT_S16LE:
    case EH_AUDIO_FORMAT_S16BE:
      samplesize=2;
      break;
    case EH_AUDIO_FORMAT_S32N:
    case EH_AUDIO_FORMAT_S32LE:
    case EH_AUDIO_FORMAT_S32BE:
    case EH_AUDIO_FORMAT_F32N:
    case EH_AUDIO_FORMAT_S32N_LO16:
      samplesize=4;
      break;
    default: return;
  }
  int64_t busylooptime=0;
  while (samplec>0) {
    int err=eh_aucvt_input(&eh.aucvt,v,samplec);
    if (err<0) return;
    samplec-=err;
    if (samplec<=0) break;
    v=(char*)v+err*samplesize;
    // If aucvt didn't consume it all, busy-loop until it does.
    // There's some glitch in PulseAudio (?), on my Nuc if the headphones are disconnected, it implicitly is using some dummy device,
    // and we land here often. I've seen it take >2 seconds for the driver to catch up.
    // So... if we get into this busy-loop for more than say 20 ms let's drop the input and move on.
    if (!busylooptime) {
      busylooptime=eh_now_real_us();
    } else if (eh_now_real_us()-busylooptime>=20000) {
      //fprintf(stderr,"aucvt panic, discarding %d samples [%s:%d]\n",samplec,__FILE__,__LINE__);
      break;
    }
  }
}

int eh_audio_guess_framec() {
  if (eh.delegate.generate_pcm) return 0;
  return (eh.aucvt.bufa-eh.aucvt.bufc)/eh.delegate.audio_chanc;
}

int eh_audio_lock() {
  if (!eh.audio->type->lock) return 0;
  return eh.audio->type->lock(eh.audio);
}

void eh_audio_unlock() {
  if (!eh.audio->type->unlock) return;
  eh.audio->type->unlock(eh.audio);
}

/* Accessors to input.
 */
 
uint16_t eh_input_get(uint8_t plrid) {
  return inmgr_get_player(plrid);
}

const char *eh_input_device_name(int devid) {
  if (devid&&(devid==eh.devid_keyboard)) return "System Keyboard";
  int driveri=eh.inputc;
  while (driveri-->0) {
    struct eh_input_driver *driver=eh.inputv[driveri];
    if (!driver->type->has_device||!driver->type->has_device(driver,devid)) continue;
    if (!driver->type->get_ids) return 0;
    int vid,pid,version;
    return driver->type->get_ids(&vid,&pid,&version,driver,devid);
  }
  return 0;
}

struct eh_input_driver *eh_input_driver_for_device(int devid) {
  int driveri=eh.inputc;
  while (driveri-->0) {
    struct eh_input_driver *driver=eh.inputv[driveri];
    if (!driver->type->has_device||!driver->type->has_device(driver,devid)) continue;
    return driver;
  }
  return 0;
}

/* Trivial global accessors.
 */
 
struct fakews *eh_get_fakews() {
  return eh.fakews;
}

struct eh_video_driver *eh_get_video_driver() {
  return eh.video;
}

struct eh_audio_driver *eh_get_audio_driver() {
  return eh.audio;
}

struct eh_input_driver *eh_get_input_driver_by_index(int p) {
  if (p<0) return 0;
  if (p>=eh.inputc) return 0;
  return eh.inputv[p];
}

const char *eh_get_video_device() {
  return eh.video_device;
}

const char *eh_get_audio_device() {
  return eh.audio_device;
}

// The public video API is at lib/render/eh_render_obj.c
