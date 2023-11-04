/* eh_driver.h
 * Generic driver implementations for audio, video, and input.
 * These are not exposed to the client, but I guess could be if we ever want to.
 */
 
#ifndef EH_DRIVER_H
#define EH_DRIVER_H

#include <stdint.h>

struct eh_video_driver;
struct eh_video_type;
struct eh_video_delegate;
struct eh_video_setup;
struct eh_audio_driver;
struct eh_audio_type;
struct eh_audio_delegate;
struct eh_audio_setup;
struct eh_input_driver;
struct eh_input_type;
struct eh_input_delegate;

/* Video.
 *******************************************************************************/
 
struct eh_video_delegate {
  void *userdata;
  void (*cb_close)(void *userdata);
  void (*cb_resize)(int w,int h,void *userdata);
  void (*cb_focus)(int focus,void *userdata);
  int (*cb_key)(int keycode,int value,void *userdata); // USB-HID; nonzero to acknowledge.
  void (*cb_text)(int codepoint,void *userdata); // Unicode; only for unacknowledged key events.
  // No mouse events; I doubt we'll ever want them.
};

struct eh_video_setup {
  int w,h; // Size hint. OK to pass the framebuffer size, driver should big it up if warranted. And (0,0) is just fine.
  int rate; // hz
  int fullscreen;
  const char *title; // utf-8
  const void *iconrgba;
  int iconw,iconh;
};
 
struct eh_video_driver {
  const struct eh_video_type *type;
  struct eh_video_delegate delegate;
  int w,h; // Full output size.
  int rate; // hz
  int fullscreen;
};

struct eh_video_type {
  const char *name;
  const char *desc;
  int objlen;
  int appointment_only;
  int provides_keyboard;
  // Required:
  void (*del)(struct eh_video_driver *driver);
  int (*init)(struct eh_video_driver *driver,const struct eh_video_setup *setup);
  int (*begin)(struct eh_video_driver *driver);
  int (*end)(struct eh_video_driver *driver);
  // Optional:
  int (*update)(struct eh_video_driver *driver);
  void (*set_fullscreen)(struct eh_video_driver *driver,int fullscreen);
  void (*suppress_screensaver)(struct eh_video_driver *driver);
};

void eh_video_driver_del(struct eh_video_driver *driver);

struct eh_video_driver *eh_video_driver_new(
  const struct eh_video_type *type,
  const struct eh_video_delegate *delegate,
  const struct eh_video_setup *setup
);

/* Audio.
 ************************************************************************************/
 
struct eh_audio_delegate {
  void *userdata;
  void (*cb_pcm)(int16_t *v,int c,void *userdata);
};

struct eh_audio_setup {
  int rate;
  int chanc;
  int buffersize;
};

struct eh_audio_driver {
  const struct eh_audio_type *type;
  struct eh_audio_delegate delegate;
  int rate;
  int chanc;
  int format;
  int playing;
};

struct eh_audio_type {
  const char *name;
  const char *desc;
  int objlen;
  int appointment_only;
  // Required:
  void (*del)(struct eh_audio_driver *driver);
  int (*init)(struct eh_audio_driver *driver,const struct eh_audio_setup *setup);
  void (*play)(struct eh_audio_driver *driver,int play);
  // Optional:
  int (*update)(struct eh_audio_driver *driver);
  int (*lock)(struct eh_audio_driver *driver);
  void (*unlock)(struct eh_audio_driver *driver);
};

void eh_audio_driver_del(struct eh_audio_driver *driver);

struct eh_audio_driver *eh_audio_driver_new(
  const struct eh_audio_type *type,
  const struct eh_audio_delegate *delegate,
  const struct eh_audio_setup *setup
);

/* Input.
 ***************************************************************************/
 
struct eh_input_delegate {
  void *userdata;
  void (*cb_connect)(int devid,void *userdata);
  void (*cb_disconnect)(int devid,void *userdata);
  void (*cb_button)(int devid,int btnid,int value,void *userdata);
};

struct eh_input_driver {
  const struct eh_input_type *type;
  struct eh_input_delegate delegate;
};

struct eh_input_type {
  const char *name;
  const char *desc;
  int objlen;
  int appointment_only;
  // Required:
  void (*del)(struct eh_input_driver *driver);
  int (*init)(struct eh_input_driver *driver);
  int (*update)(struct eh_input_driver *driver);
  const char *(*get_ids)(uint16_t *vid,uint16_t *pid,struct eh_input_driver *driver,int devid);
  int (*list_buttons)(
    struct eh_input_driver *driver,
    int devid,
    int (*cb)(int btnid,uint32_t usage,int lo,int hi,int value,void *userdata),
    void *userdata
  );
  int (*has_device)(struct eh_input_driver *driver,int devid);
};

void eh_input_driver_del(struct eh_input_driver *driver);

struct eh_input_driver *eh_input_driver_new(
  const struct eh_input_type *type,
  const struct eh_input_delegate *delegate
);

/* Global registries.
 **************************************************************************/
 
const struct eh_video_type *eh_video_type_by_index(int p);
const struct eh_video_type *eh_video_type_by_name(const char *name,int namec);
const struct eh_audio_type *eh_audio_type_by_index(int p);
const struct eh_audio_type *eh_audio_type_by_name(const char *name,int namec);
const struct eh_input_type *eh_input_type_by_index(int p);
const struct eh_input_type *eh_input_type_by_name(const char *name,int namec);

int eh_input_devid_next();

#endif
