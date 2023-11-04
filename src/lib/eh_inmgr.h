/* eh_inmgr.h
 * Input manager, resident in libemuhost.
 * Clients usually can ignore this, and take the digested state off the global API.
 * If you want to provide dynamic input map editing to the user, you'll need to interact with inmgr.
 */
 
#ifndef EH_INMGR_H
#define EH_INMGR_H

struct eh_inmgr;
struct eh_input_driver;
struct sr_encoder;

struct eh_inmgr_event {

  /* (btnid==0) = disregard this section; source event only.
   * (playerid==0) = aggregate state.
   */
  int playerid;
  int btnid;
  int value;
  int state;
  
  /* (0,0,0) = source not available; output event only.
   * (X,0,0) = connect.
   * (X,-1,-1) = disconnect.
   */
  int srcdevid;
  int srcbtnid;
  int srcvalue;
};

struct eh_inmgr_delegate {
  void *userdata;
  int (*cb_event)(void *userdata,const struct eh_inmgr_event *event);
  
  // For main delegate only, not listeners:
  void (*cb_config_dirty)(void *userdata);
};

void eh_inmgr_del(struct eh_inmgr *inmgr);
int eh_inmgr_ref(struct eh_inmgr *inmgr);

/* We require a player count at startup, and that becomes the highest "playerid" we will assign.
 * If we end up with more mappable devices than players, there will be players with multiple devices.
 * The first mappable event, that device becomes player 1, the second player 2, etc.
 */
struct eh_inmgr *eh_inmgr_new(const struct eh_inmgr_delegate *delegate,int playerc);

void *eh_inmgr_get_userdata(const struct eh_inmgr *inmgr);

/* Decode with (refname) to log detailed errors, or null for silence.
 */
int eh_inmgr_load_config(struct eh_inmgr *inmgr,const char *path);
int eh_inmgr_decode_config(struct eh_inmgr *inmgr,const void *src,int srcc,const char *refname);
int eh_inmgr_save_config(const char *path,const struct eh_inmgr *inmgr);
int eh_inmgr_encode_config(struct sr_encoder *dst,const struct eh_inmgr *inmgr);

// Send events to inmgr in basically the shape they arrive from eh_input_driver.
int eh_inmgr_connect(struct eh_inmgr *inmgr,struct eh_input_driver *driver,int devid);
int eh_inmgr_disconnect(struct eh_inmgr *inmgr,int devid);
int eh_inmgr_button(struct eh_inmgr *inmgr,int devid,int btnid,int value);

int eh_inmgr_get_player_state(const struct eh_inmgr *inmgr,int playerid);

// Also, you can bypass the source mapping and push output events directly.
int eh_inmgr_artificial_event(struct eh_inmgr *inmgr,int playerid,int btnid,int value);

/* In addition to the singleton delegate, anyone can register for event callbacks.
 * You can listen for devid zero to receive everything, or playerid zero to receive all output events.
 * Source listeners will receive synchronously the list of matching devices, events with only (srcdevid) set.
 * Also, we send a special event (srcdevid,srcbtnid=-1,srcvalue=-1) when a matched device disconnects.
 */
int eh_inmgr_listen_source(struct eh_inmgr *inmgr,int devid,const struct eh_inmgr_delegate *delegate);
int eh_inmgr_listen_player(struct eh_inmgr *inmgr,int playerid,const struct eh_inmgr_delegate *delegate);
int eh_inmgr_listen(struct eh_inmgr *inmgr,const struct eh_inmgr_delegate *delegate);
int eh_inmgr_unlisten(struct eh_inmgr *inmgr,int listenerid);

/* Devices are enabled by default.
 * If disabled, mapping is not performed and only source listeners will receive from the device.
 * Disable a device eg when performing interactive re-configuration.
 * When disabling a device, we trigger events simulating any of its ON buttons going OFF.
 */
int eh_inmgr_enable_device(struct eh_inmgr *inmgr,int devid,int enable);
int eh_inmgr_device_is_enabled(const struct eh_inmgr *inmgr,int devid);

/* Each source button may map to only one output button, but as a special consideration,
 * that output button may be one of the compounds (HORZ,VERT,DPAD).
 * Setting the mapping alters both the live state and the permanent config (if you save after).
 * If multiple devices use the same config, eg a set of identical controllers, setting one affects them all.
 */
int eh_inmgr_get_mapping(struct eh_inmgr *inmgr,int devid,int srcbtnid);
int eh_inmgr_set_mapping(struct eh_inmgr *inmgr,int devid,int srcbtnid,int dstbtnid);

/* Text symbols for the output buttons (EH_BUTTON_*).
 * We fall back to hexadecimal integers; everything can repr.
 * repr_canned returns static constant strings with no fallback.
 */
int eh_btnid_eval(const char *src,int srcc);
int eh_btnid_repr(char *dst,int dsta,int btnid);
const char *eh_btnid_repr_canned(int btnid);
const char *eh_btnid_repr_by_index(int p);

/* Inmgr maintains a list of connected devid and the associated driver.
 * (this knowledge doesn't live anywhere else in Emuhost, except in the individual drivers).
 * We supply a few conveniences to look things up by devid, without knowing the driver.
 */
struct eh_input_driver *eh_inmgr_driver_for_devid(const struct eh_inmgr *inmgr,int devid);
int eh_inmgr_devids_for_driver(
  const struct eh_inmgr *inmgr,
  const struct eh_input_driver *driver,
  int (*cb)(int devid,void *userdata),
  void *userdata
);
const char *eh_inmgr_device_name(const struct eh_inmgr *inmgr,int devid);
int eh_inmgr_device_ids(int *vendor,int *product,int *version,const struct eh_inmgr *inmgr,int devid);
int eh_inmgr_device_enumerate(
  const struct eh_inmgr *inmgr,
  int devid,
  int (*cb)(void *userdata,int btnid,int value,int lo,int hi,int hidusage),
  void *userdata
);

#endif
