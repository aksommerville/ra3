#ifndef EH_GLX_INTERNAL_H
#define EH_GLX_INTERNAL_H

#include "eh_glx.h"
#include "lib/eh_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <limits.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include <GL/gl.h>

// Required only for making intelligent initial-size decisions in a multi-monitor setting.
// apt install libxinerama-dev
#if USE_xinerama
  #include <X11/extensions/Xinerama.h>
#endif

#define KeyRepeat (LASTEvent+2)
#define EH_GLX_KEY_REPEAT_INTERVAL 10
#define EH_GLX_ICON_SIZE_LIMIT 64

struct eh_video_driver_glx {
  struct eh_video_driver hdr;
  
  Display *dpy;
  int screen;
  Window win;

  GLXContext ctx;
  int glx_version_minor,glx_version_major;
  
  Atom atom_WM_PROTOCOLS;
  Atom atom_WM_DELETE_WINDOW;
  Atom atom__NET_WM_STATE;
  Atom atom__NET_WM_STATE_FULLSCREEN;
  Atom atom__NET_WM_STATE_ADD;
  Atom atom__NET_WM_STATE_REMOVE;
  Atom atom__NET_WM_ICON;
  Atom atom__NET_WM_ICON_NAME;
  Atom atom__NET_WM_NAME;
  Atom atom_WM_CLASS;
  Atom atom_STRING;
  Atom atom_UTF8_STRING;
  
  int screensaver_suppressed;
  int focus;
};

#define DRIVER ((struct eh_video_driver_glx*)driver)

int eh_glx_init_start(struct eh_video_driver *driver,const struct eh_video_setup *setup);
int eh_glx_init_opengl(struct eh_video_driver *driver,const struct eh_video_setup *setup);
int eh_glx_init_finish(struct eh_video_driver *driver,const struct eh_video_setup *setup);

int eh_glx_update(struct eh_video_driver *driver);

int eh_glx_codepoint_from_keysym(int keysym);
int eh_glx_usb_usage_from_keysym(int keysym);

#endif
