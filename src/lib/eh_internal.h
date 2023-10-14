#ifndef EH_INTERNAL_H
#define EH_INTERNAL_H

#include "emuhost.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

extern struct eh {
  const char *exename;
  struct eh_delegate delegate;
  volatile int sigc;
  int terminate;
  const char *rompath;
} eh;

int eh_configure(int argc,char **argv);

void eh_drivers_quit();
int eh_drivers_init();

#endif
