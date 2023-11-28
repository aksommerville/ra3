#ifndef EH_INTERNAL_H
#define EH_INTERNAL_H

#include "emuhost.h"
#include "eh_driver.h"
#include "eh_clock.h"
#include "eh_inmgr.h"
#include "eh_aucvt.h"
#include "render/eh_render.h"
#include "opt/fakews/fakews.h"
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
  char *input_config_path;
  int prefer_screen;
  char *romassist_host;
  int romassist_port;
  struct { int x,y,w,h; } fbcrop; // True dimensions of video output. delegate->width,height are only input from client.
  
  struct eh_clock clock;
  struct eh_video_driver *video;
  struct eh_audio_driver *audio;
  struct eh_input_driver **inputv;
  int inputc,inputa;
  int devid_keyboard; // nonzero if video driver provides a keyboard
  struct eh_render *render;
  struct eh_inmgr *inmgr;
  int inmgr_dirty;
  struct eh_aucvt aucvt;
  struct fakews *fakews;
  
  int screencap_requested;
  int hard_pause;
  int hard_pause_stepc;
  int fastfwd;
  
} eh;

int eh_configure(int argc,char **argv);
int eh_config_set_string(char **dst,const char *src,int srcc);
int eh_config_save();

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
int eh_cb_digested_event(void *userdata,const struct eh_inmgr_event *event);
void eh_cb_inmgr_config_dirty(void *userdata);
void eh_cb_ws_connect(void *userdata);
void eh_cb_ws_disconnect(void *userdata);
void eh_cb_ws_message(int opcode,const void *v,int c,void *userdata);

#endif
