#ifndef MN_INTERNAL_H
#define MN_INTERNAL_H

#include "lib/emuhost.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

extern struct mn {
  int video_dirty;
  uint16_t pvinput;
} mn;

#endif
