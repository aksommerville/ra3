#ifndef GUI_INTERNAL_H
#define GUI_INTERNAL_H

#include "gui.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

struct gui {
  struct gui_delegate delegate;
  struct gui_widget *root;
  uint16_t pvinput;
  int video_dirty;
  int pvw,pvh;
};

#endif
