#ifndef MN_INTERNAL_H
#define MN_INTERNAL_H

#include "lib/emuhost.h"
#include "opt/gui/gui.h"
#include "db_service.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

extern struct mn {
  struct gui *gui;
  struct db_service dbs;
} mn;

extern const struct gui_widget_type mn_widget_type_home;
extern const struct gui_widget_type mn_widget_type_menubar;
extern const struct gui_widget_type mn_widget_type_carousel;
extern const struct gui_widget_type mn_widget_type_gamedetails;
extern const struct gui_widget_type mn_widget_type_daterange;
extern const struct gui_widget_type mn_widget_type_rating;

int mn_widget_daterange_setup(
  struct gui_widget *widget,
  void (*cb)(struct gui_widget *daterange,void *userdata,int lovalue,int hivalue),
  void *userdata,
  int lovalue,int hivalue,
  int lolimit,int hilimit
);

int mn_widget_rating_setup(
  struct gui_widget *widget,
  void (*cb)(struct gui_widget *rating,int v,void *userdata),
  void *userdata,
  int v
);

void mn_cb_sound_effect(int sfxid,void *userdata);

#endif
