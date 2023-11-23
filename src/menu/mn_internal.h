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
extern const struct gui_widget_type mn_widget_type_edit;
extern const struct gui_widget_type mn_widget_type_flags;
extern const struct gui_widget_type mn_widget_type_lists;

int mn_widget_daterange_setup(
  struct gui_widget *widget,
  void (*cb)(struct gui_widget *daterange,void *userdata,int lovalue,int hivalue),
  void *userdata,
  int lovalue,int hivalue,
  int lolimit,int hilimit
);
int mn_widget_daterange_setup_single(
  struct gui_widget *widget,
  void (*cb)(struct gui_widget *daterange,void *userdata,int lovalue,int hivalue), // (lovalue==hivalue) when we call
  void *userdata,
  int value
  // limits are implicitly 1970..9999
);

int mn_widget_rating_setup(
  struct gui_widget *widget,
  void (*cb)(struct gui_widget *rating,int v,void *userdata),
  void *userdata,
  int v
);

int mn_widget_edit_setup(
  struct gui_widget *widget,
  int gameid
);

// You may provide (v) as a JSON string, or raw text.
int mn_widget_flags_setup(
  struct gui_widget *widget,
  void (*cb)(struct gui_widget *flags,const char *v,int c,void *userdata),
  void *userdata,
  const char *v,int c
);

/* You provide "joined" the lists this game is already in, and "all" the full set of lists.
 * Both must be JSON arrays, and can be either {id,name} or just the name as a string.
 * We callback for each change immediately, with (add>0) if adding, 0 if removing, or <0 to create a new list.
 */
int mn_widget_lists_setup(
  struct gui_widget *widget,
  const char *joined,int joinedc,
  const char *all,int allc,
  void (*cb)(struct gui_widget *lists,int add,const char *name,int namec,void *userdata),
  void *userdata
);
int mn_widget_lists_encode(char *dst,int dsta,struct gui_widget *widget);

void mn_cb_sound_effect(int sfxid,void *userdata);

#endif
