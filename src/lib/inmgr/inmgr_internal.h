#ifndef INMGR_INTERNAL_H
#define INMGR_INTERNAL_H

#include "inmgr.h"
#include "lib/gcfg/gcfg.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdint.h>

#define INMGR_PLAYER_LIMIT 8
#define INMGR_UNSET_LIMIT 256 /* Devices could report millions of buttons if they wanted to; cut it off somewhere. */

#define INMGR_BUTTON_MODE_SIGNAL 1
#define INMGR_BUTTON_MODE_BUTTON 2
#define INMGR_BUTTON_MODE_AXIS   3
#define INMGR_BUTTON_MODE_HAT    4

extern struct inmgr {
  int init;
  struct gcfg_input *gcfg;
  int btnmask;
  
  struct inmgr_signal {
    int btnid; // >=0x10000
    void (*cb)();
  } *signalv;
  int signalc,signala;
  
  uint16_t playerv[1+INMGR_PLAYER_LIMIT];
  int playerc;
  
  struct inmgr_device {
    int devid;
    int playerid;
    int state;
    int keyboard;
    int enable;
    int gcfg_mapid;
    struct inmgr_button {
      int mode;
      int srcbtnid;
      int srcvalue;
      int srclo,srchi;
      int dstvalue;
      int dstbtnid;
    } *buttonv;
    int buttonc,buttona;
  } *devicev;
  int devicec,devicea;
  
  struct {
    int devid;
    struct gcfg_input_device *gdev;
    struct inmgr_unset {
      int btnid,hidusage,lo,hi,value;
    } *unsetv;
    int unsetc,unseta;
    struct inmgr_device *device;
  } connect;
  void *connect_in_progress;
  
  struct inmgr_spy {
    int spyid;
    void (*cb)(int devid,int btnid,int value,int state,void *userdata);
    void *userdata;
  } *spyv;
  int spyc,spya;
  int spyid_next;
  
} inmgr;

int inmgr_signalv_search(int btnid);
void inmgr_device_cleanup(struct inmgr_device *device);
int inmgr_devicev_search(int devid);
struct inmgr_device *inmgr_device_add(int devid);
struct inmgr_button *inmgr_device_add_button(struct inmgr_device *device,int srcbtnid); // Fails if existing.
struct inmgr_button *inmgr_device_get_button(const struct inmgr_device *device,int srcbtnid);
void inmgr_device_remove(struct inmgr_device *device);

void inmgr_button_init(
  struct inmgr_button *button,
  int dstbtnid,
  const char *comment,int commentc,
  int hidusage,int lo,int hi,int value
);

int inmgr_gcfg_btn_from_btnid(int imbtnid);

#endif
