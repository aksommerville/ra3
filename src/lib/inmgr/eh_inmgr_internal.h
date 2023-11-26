#ifndef EH_INMGR_INTERNAL_H
#define EH_INMGR_INTERNAL_H

#include "lib/emuhost.h"
#include "lib/eh_inmgr.h"
#include "lib/eh_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// Soft limit, we clamp if higher. Can go crazy high if we want, but >4 is already pretty unrealistic.
#define EH_INMGR_PLAYER_LIMIT 31

#define EH_INMGR_REQTYPE_ALL    0
#define EH_INMGR_REQTYPE_DEVICE 1
#define EH_INMGR_REQTYPE_PLAYER 2

struct eh_inmgr {
  int refc;
  struct eh_inmgr_delegate delegate;
  
  struct eh_inmgr_player {
    int playerid; // index+1
    int state;
    int devicec;
  } *playerv;
  int playerc; // One greater than the requested count; includes Player Zero
  
  struct eh_inmgr_listener {
    int listenerid;
    int reqtype;
    int reqid; // devid or playerid depending on (reqtype)
    struct eh_inmgr_delegate delegate;
  } *listenerv;
  int listenerc,listenera;
  
  // Devid of disabled devices.
  int *suspendv;
  int suspendc,suspenda;
  
  struct eh_inmgr_config {
    int vendor,product;
    char *name;
    int namec;
    // Only one rule is permitted per srcbtnid.
    // (dstbtnid) may be NONE in configs; not in devices.
    struct eh_inmgr_rule {
      int srcbtnid,dstbtnid;
    } *rulev;
    int rulec,rulea;
  } *configv;
  int configc,configa;
  
  struct eh_inmgr_device {
    int devid;
    struct eh_input_driver *driver; // WEAK. Null for system keyboard.
    struct eh_inmgr_config *config; // WEAK (all get wiped when configs change)
    int playerid;
    // Device buttons are pretty much 1:1 with config rules.
    // In particular, the compound buttons HORZ, VERT, and DPAD are a single rule and single button.
    // Buttons not reported at device enumeration are not included here.
    struct eh_inmgr_button {
      int srcbtnid,dstbtnid;
      int srcvalue,dstvalue;
      // EH_BUTTON_DPAD: srclo==srchi-7
      // EH_BUTTON_HORZ|VERT: INT_MIN..srclo=lo, srchi..INT_MAX=hi, srclo+1..srchi-1=off (upside-down is ok too)
      // others: srclo..srchi=on
      int srclo,srchi;
    } *buttonv;
    int buttonc,buttona;
  } *devicev;
  int devicec,devicea;
  
  // Source event currently processing.
  int srcdevid;
  int srcbtnid;
  int srcvalue;
};

int eh_inmgr_zero_outputs_for_device(struct eh_inmgr *inmgr,int devid);

// Primitives. These don't trigger callbacks or cascade or anything like that.
int eh_inmgr_devicev_search(const struct eh_inmgr *inmgr,int devid);
struct eh_inmgr_device *eh_inmgr_devicev_insert(struct eh_inmgr *inmgr,int p,int devid);
void eh_inmgr_devicev_remove(struct eh_inmgr *inmgr,int p);
struct eh_inmgr_config *eh_inmgr_configv_append(struct eh_inmgr *inmgr);
void eh_inmgr_configv_remove(struct eh_inmgr *inmgr,struct eh_inmgr_config *config);

void eh_inmgr_device_cleanup(struct eh_inmgr_device *device);
int eh_inmgr_device_apply_config(struct eh_inmgr_device *device);
struct eh_inmgr_button *eh_inmgr_device_add_button(struct eh_inmgr_device *device,int srcbtnid);
struct eh_inmgr_button *eh_inmgr_device_get_button(struct eh_inmgr_device *device,int srcbtnid);

void eh_inmgr_config_cleanup(struct eh_inmgr_config *config);
struct eh_inmgr_config *eh_inmgr_config_for_device(struct eh_inmgr *inmgr,struct eh_inmgr_device *device);
struct eh_inmgr_rule *eh_inmgr_config_add_rule(struct eh_inmgr_config *config,int srcbtnid);
struct eh_inmgr_rule *eh_inmgr_config_get_rule(const struct eh_inmgr_config *config,int srcbtnid);
void eh_inmgr_config_remove_rule(struct eh_inmgr_config *config,struct eh_inmgr_rule *rule);
int eh_inmgr_config_set_name(struct eh_inmgr_config *config,const char *src,int srcc);

/* Query for device capabilities and make up a configuration.
 * If we come up with something reasonable, record it in the config registry, fire dirty callback, and return a WEAK reference.
 */
struct eh_inmgr_config *eh_inmgr_guess_config(struct eh_inmgr *inmgr,struct eh_input_driver *driver,int devid);

struct eh_inmgr_player *eh_inmgr_player_by_id(const struct eh_inmgr *inmgr,int playerid);
struct eh_inmgr_device *eh_inmgr_device_by_id(const struct eh_inmgr *inmgr,int devid);

int eh_inmgr_update_player(struct eh_inmgr *inmgr,int playerid,int btnid,int value);

#endif
