#ifndef MN_INTERNAL_H
#define MN_INTERNAL_H

#include "lib/emuhost.h"
#include "opt/gui/gui.h"
#include "opt/cheapsynth/cheapsynth.h"
#include "db_service.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

extern struct mn {

  const char *exename;
  int show_invalid; // By default, we ask to strip obscene, faulty, and hardware from all searches. Nonzero to search exactly what's asked for.
  char *data_path;
  int data_pathc;

  struct gui *gui;
  struct cheapsynth *cheapsynth;
  struct db_service dbs;
  int upgrade_in_progress;
  int upgrade_status;
} mn;

/* Convenience: Returns (mn.data_path) prepended to (basename) with the appropriate separator.
 * Uses a shared static buffer. Each call invalidates all prior results.
 */
const char *mn_data_path(const char *basename);

extern const struct gui_widget_type mn_widget_type_home;
extern const struct gui_widget_type mn_widget_type_menubar;
extern const struct gui_widget_type mn_widget_type_carousel;
extern const struct gui_widget_type mn_widget_type_gamedetails;
extern const struct gui_widget_type mn_widget_type_daterange;
extern const struct gui_widget_type mn_widget_type_rating;
extern const struct gui_widget_type mn_widget_type_edit;
extern const struct gui_widget_type mn_widget_type_flags;
extern const struct gui_widget_type mn_widget_type_lists;
extern const struct gui_widget_type mn_widget_type_video;
extern const struct gui_widget_type mn_widget_type_audio;
extern const struct gui_widget_type mn_widget_type_input;
extern const struct gui_widget_type mn_widget_type_network;
extern const struct gui_widget_type mn_widget_type_interface;
extern const struct gui_widget_type mn_widget_type_indev; // Input config for a single device.

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

int mn_widget_indev_setup(
  struct gui_widget *widget,
  int devid
);

void mn_cb_sound_effect(int sfxid,void *userdata);
#define MN_SOUND(tag) mn_cb_sound_effect(GUI_SFXID_##tag,0);

#endif
