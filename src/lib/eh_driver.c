#include "eh_driver.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Global registry primitives.
 * This should be the only place we need to name drivers individually.
 */
 
extern const struct eh_video_type eh_video_type_glx;
extern const struct eh_video_type eh_video_type_drm;
extern const struct eh_video_type eh_video_type_macwm;
extern const struct eh_video_type eh_video_type_mswm;
extern const struct eh_video_type eh_video_type_dummy;

extern const struct eh_audio_type eh_audio_type_pulse;
extern const struct eh_audio_type eh_audio_type_alsa;
extern const struct eh_audio_type eh_audio_type_macaudio;
extern const struct eh_audio_type eh_audio_type_msaudio;
extern const struct eh_audio_type eh_audio_type_dummy;

extern const struct eh_input_type eh_input_type_evdev;
extern const struct eh_input_type eh_input_type_machid;
extern const struct eh_input_type eh_input_type_mshid;
 
static const struct eh_video_type *eh_video_typev[]={
#if USE_glx
  &eh_video_type_glx,
#endif
#if USE_drm
  &eh_video_type_drm,
#endif
#if USE_macwm
  &eh_video_type_macwm,
#endif
#if USE_mswm
  &eh_video_type_mswm,
#endif
  &eh_video_type_dummy,
};

static const struct eh_audio_type *eh_audio_typev[]={
#if USE_pulse
  &eh_audio_type_pulse,
#endif
#if USE_alsa
  &eh_audio_type_alsa,
#endif
#if USE_macaudio
  &eh_audio_type_macaudio,
#endif
#if USE_msaudio
  &eh_audio_type_msaudio,
#endif
  &eh_audio_type_dummy,
};

static const struct eh_input_type *eh_input_typev[]={
#if USE_evdev
  &eh_input_type_evdev,
#endif
#if USE_machid
  &eh_input_type_machid,
#endif
#if USE_mshid
  &eh_input_type_mshid,
#endif
};

/* Registry accessors.
 */
 
#define RACC(tag) \
  const struct eh_##tag##_type *eh_##tag##_type_by_index(int p) { \
    if (p<0) return 0; \
    int c=sizeof(eh_##tag##_typev)/sizeof(void*); \
    if (p>=c) return 0; \
    return eh_##tag##_typev[p]; \
  } \
  const struct eh_##tag##_type *eh_##tag##_type_by_name(const char *name,int namec) { \
    if (!name) return 0; \
    if (namec<0) { namec=0; while (name[namec]) namec++; } \
    const struct eh_##tag##_type **p=eh_##tag##_typev; \
    int i=sizeof(eh_##tag##_typev)/sizeof(void*); \
    for (;i-->0;p++) { \
      if (memcmp(name,(*p)->name,namec)) continue; \
      if ((*p)->name[namec]) continue; \
      return *p; \
    } \
    return 0; \
  }
  
RACC(video)
RACC(audio)
RACC(input)

#undef RACC

static int eh_devid_next=1;

int eh_input_devid_next() {
  if (eh_devid_next==INT_MAX) return -1;
  return eh_devid_next++;
}

/* Driver instance lifecycle.
 */

void eh_video_driver_del(struct eh_video_driver *driver) {
  if (!driver) return;
  if (driver->type->del) driver->type->del(driver);
  free(driver);
}

struct eh_video_driver *eh_video_driver_new(
  const struct eh_video_type *type,
  const struct eh_video_delegate *delegate,
  const struct eh_video_setup *setup
) {
  if (!type||!delegate||!setup) return 0;
  struct eh_video_driver *driver=calloc(1,type->objlen);
  if (!driver) return 0;
  driver->type=type;
  driver->delegate=*delegate;
  if (type->init(driver,setup)<0) {
    eh_video_driver_del(driver);
    return 0;
  }
  return driver;
}

void eh_audio_driver_del(struct eh_audio_driver *driver) {
  if (!driver) return;
  if (driver->type->del) driver->type->del(driver);
  free(driver);
}

struct eh_audio_driver *eh_audio_driver_new(
  const struct eh_audio_type *type,
  const struct eh_audio_delegate *delegate,
  const struct eh_audio_setup *setup
) {
  if (!type||!delegate||!setup) return 0;
  struct eh_audio_driver *driver=calloc(1,type->objlen);
  if (!driver) return 0;
  driver->type=type;
  driver->delegate=*delegate;
  if (type->init(driver,setup)<0) {
    eh_audio_driver_del(driver);
    return 0;
  }
  return driver;
}

void eh_input_driver_del(struct eh_input_driver *driver) {
  if (!driver) return;
  if (driver->type->del) driver->type->del(driver);
  free(driver);
}

struct eh_input_driver *eh_input_driver_new(
  const struct eh_input_type *type,
  const struct eh_input_delegate *delegate
) {
  if (!type||!delegate) return 0;
  struct eh_input_driver *driver=calloc(1,type->objlen);
  if (!driver) return 0;
  driver->type=type;
  driver->delegate=*delegate;
  if (type->init(driver)<0) {
    eh_input_driver_del(driver);
    return 0;
  }
  return driver;
}
