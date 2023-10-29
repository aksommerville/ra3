#ifndef EH_INTERNAL_H
#define EH_INTERNAL_H

#include "emuhost.h"
#include "eh_driver.h"
#include "eh_clock.h"
#include "render/eh_render.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

extern struct eh {

  const char *exename;
  struct eh_delegate delegate;
  volatile int sigc;
  int terminate;
  
  char *rompath;
  char *video_drivers;
  char *audio_drivers;
  char *input_drivers;
  int fullscreen;
  int audio_rate;
  int audio_chanc;
  char *audio_device;
  int glsl_version;
  
  struct eh_clock clock;
  struct eh_video_driver *video;
  struct eh_audio_driver *audio;
  struct eh_input_driver **inputv;
  int inputc,inputa;
  struct eh_render *render;
  
  /*XXX
  const void *fb;
  int gx_in_progress;
  int dstr_dirty;
  float dstr_left,dstr_right,dstr_top,dstr_bottom;
  int dstr_clear;
  unsigned int texid_fb,texid_ctab;
  int glprogram;
  uint8_t ctab[768];
  int ctab_dirty;
  /**/
  
} eh;

int eh_configure(int argc,char **argv);

void eh_drivers_quit();
int eh_drivers_init();

int eh_render_init();
void eh_commit_framebuffer(const void *fb);

void eh_cb_close(void *dummy);
void eh_cb_resize(int w,int h,void *dummy);
void eh_cb_focus(int focus,void *dummy);
int eh_cb_key(int keycode,int value,void *dummy);
void eh_cb_text(int codepoint,void *dummy);
void eh_cb_pcm(int16_t *v,int c,void *dummy);
void eh_cb_connect(int devid,void *dummy);
void eh_cb_disconnect(int devid,void *dummy);
void eh_cb_button(int devid,int btnid,int value,void *dummy);

#endif
