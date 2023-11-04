#include "eh_internal.h"
#include "opt/serial/serial.h"

/* Quit.
 */
 
void eh_drivers_quit() {
  if (eh.audio) eh.audio->type->play(eh.audio,0);
  eh_render_del(eh.render);
  eh_inmgr_del(eh.inmgr);
  eh_audio_driver_del(eh.audio);
  eh_video_driver_del(eh.video);
  if (eh.inputv) {
    while (eh.inputc-->0) eh_input_driver_del(eh.inputv[eh.inputc]);
    free(eh.inputv);
  }
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
    .w=eh.delegate.video_width,
    .h=eh.delegate.video_height,
    .rate=eh.delegate.video_rate,
    .title=eh.delegate.name,
    .iconrgba=eh.delegate.iconrgba,
    .iconw=eh.delegate.iconw,
    .iconh=eh.delegate.iconh,
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
  
  // Apportion an input device id for the keyboard if there is one.
  if (eh.video->type->provides_keyboard) {
    eh.devid_keyboard=eh_input_devid_next();
    eh_cb_connect(eh.devid_keyboard,0);
  }
  
  return 0;
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
  };
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
  fprintf(stderr,"%s: Using audio driver '%s'.\n",eh.exename,eh.audio->type->name);
  
  //TODO prep resampler
  
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
  struct eh_inmgr_delegate delegate={
    .userdata=0,
    .cb_event=eh_cb_digested_event,
    .cb_config_dirty=eh_cb_inmgr_config_dirty,
  };
  int playerc=eh.delegate.playerc;
  if ((playerc<1)||(playerc>30)) playerc=1;
  if (!(eh.inmgr=eh_inmgr_new(&delegate,playerc))) {
    fprintf(stderr,"%s: Failed to instantiate input manager.\n",eh.exename);
    return -2;
  }
  if (eh.input_config_path) {
    int err=eh_inmgr_load_config(eh.inmgr,eh.input_config_path);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Failed to process input config.\n",eh.input_config_path);
      return -2;
    }
  }

  int err;
  if (
    ((err=eh_drivers_init_video())<0)||
    ((err=eh_drivers_init_audio())<0)||
    ((err=eh_drivers_init_input())<0)
  ) return err;
  return 0;
}

/* Accessors to audio.
 */
 
int eh_audio_get_rate() {
  if (eh.delegate.audio_rate) return eh.delegate.audio_rate;
  return eh.audio->rate;
}

int eh_audio_get_chanc() {
  if (eh.delegate.audio_chanc) return eh.delegate.audio_chanc;
  return eh.audio->chanc;
}

int eh_audio_get_format() {
  if (eh.delegate.audio_format) return eh.delegate.audio_format;
  return eh.audio->format;
}

void eh_audio_write(const void *v,int framec) {
  //TODO
}

int eh_audio_guess_framec() {
  return 123;//TODO
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
  return eh_inmgr_get_player_state(eh.inmgr,plrid);
}

// The public video API is at lib/render/eh_render_obj.c
