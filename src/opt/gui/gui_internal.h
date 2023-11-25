#ifndef GUI_INTERNAL_H
#define GUI_INTERNAL_H

#include "gui.h"
#include "gui_font.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>

#define GUI_GLSL_VERSION 120 /* TODO configurable */

#define GUI_TEXT_LINE_SANITY_LIMIT 4096 /* Never render text wider than this, fail hard instead. */

struct gui {
  struct gui_delegate delegate;
  struct gui_widget *root;
  uint16_t pvinput;
  int video_dirty;
  int pvw,pvh;
  struct gui_font **fontv;
  int fontc,fonta;
  struct gui_font *font; // WEAK
  char *data_path;
  int data_pathc;
};

struct gui_program {
  int refc;
  GLuint programid;
  const char *name;
  GLuint loc_screensize;
  GLuint loc_texsize;
  GLuint loc_sampler;
  int attrc;
};

struct gui_texture {
  int refc;
  GLuint texid;
  int w,h;
};

void gui_text_quit(struct gui *gui);
int gui_text_init(struct gui *gui);

#endif
