/* inmgr.h
 * Uses: gcfg
 * Generic input manager.
 * We'll load the global input configuration and map devices accordingly.
 * We don't require hostio, but do express things in a form to glue into it easily.
 * Our output is 16 standard-mapping buttons, two-state only, for an arbitrary count of players.
 * You won't see devices until they're interacted with.
 * We could expose them earlier, but this way is convenient for us, and it's consistent with Web Gamepad API.
 */
 
#ifndef INMGR_H
#define INMGR_H

/* INMGR_BTN_* and INMGR_SIGNAL_* are exactly the same as EH_BTN_*.
 * They were written separately and combined after the fact, why they exist with two different prefixes.
 */
#define INMGR_BTN_LEFT     0x0001
#define INMGR_BTN_RIGHT    0x0002
#define INMGR_BTN_UP       0x0004
#define INMGR_BTN_DOWN     0x0008
#define INMGR_BTN_SOUTH    0x0010
#define INMGR_BTN_WEST     0x0020
#define INMGR_BTN_EAST     0x0040
#define INMGR_BTN_NORTH    0x0080
#define INMGR_BTN_L1       0x0100
#define INMGR_BTN_R1       0x0200
#define INMGR_BTN_L2       0x0400
#define INMGR_BTN_R2       0x0800
#define INMGR_BTN_AUX1     0x1000
#define INMGR_BTN_AUX2     0x2000
#define INMGR_BTN_AUX3     0x4000
#define INMGR_BTN_CD       0x8000

#define INMGR_FOR_EACH_BUTTON \
  _(LEFT) _(RIGHT) _(UP) _(DOWN) \
  _(SOUTH) _(WEST) _(EAST) _(NORTH) \
  _(L1) _(R1) _(L2) _(R2) \
  _(AUX1) _(AUX2) _(AUX3) \
  _(CD)

#define INMGR_SIGNAL_QUIT          0x10001
#define INMGR_SIGNAL_FULLSCREEN    0x10002
#define INMGR_SIGNAL_MUTE          0x10003
#define INMGR_SIGNAL_PAUSE         0x10004
#define INMGR_SIGNAL_SCREENCAP     0x10005
#define INMGR_SIGNAL_SAVESTATE     0x10006
#define INMGR_SIGNAL_LOADSTATE     0x10007
#define INMGR_SIGNAL_MENU          0x10008
#define INMGR_SIGNAL_RESET         0x10009
#define INMGR_SIGNAL_DEBUG         0x1000a
#define INMGR_SIGNAL_STEP          0x1000b
#define INMGR_SIGNAL_FASTFWD       0x1000c

#define INMGR_FOR_EACH_SIGNAL \
  _(QUIT) _(FULLSCREEN) _(MUTE) _(PAUSE) _(SCREENCAP) \
  _(SAVESTATE) _(LOADSTATE) _(MENU) _(RESET) \
  _(DEBUG) _(STEP) _(FASTFWD)

void inmgr_quit();
int inmgr_init();

int inmgr_save();

/* Setup.
 * These are only valid after init.
 * set_buttons and set_signal should only happen once, right after init.
 * It's fine to change player count on the fly, though beware that state can be dropped during transitions.
 */
void inmgr_set_player_count(int playerc);
void inmgr_set_buttons(int btnmask);
void inmgr_set_signal(int btnid,void (*cb)());

/* (playerid) zero is the aggregate of all player states.
 */
int inmgr_get_player(int playerid);

/* Provide inputs as you receive them.
 * (devid) must be unique across all connected devices. It can be zero, negative, whatever.
 * If you didn't previously connect (devid), this quickly noops.
 */
void inmgr_event(int devid,int btnid,int value);

/* Update states as if this mapped event occurred.
 */
void inmgr_artificial_event(int playerid,int btnid,int value);

/* When a new device becomes available, give us its ids and we return a temporary opaque configuration context.
 * Call inmgr_connect_more() on that context for each button the device can report,
 * then inmgr_connect_end() when finished. It returns >0 if the device is mapped and ready.
 * It is an error to begin a new connection before ending a previous one.
 */
void *inmgr_connect_begin(int devid,int vid,int pid,int version,const char *name,int namec);
void inmgr_connect_more(void *ctx,int btnid,int hidusage,int lo,int hi,int value);
void inmgr_connect_keyboard(void *ctx); // Instead of a hundred "more", call this once to assume a sensible HID keyboard.
int inmgr_connect_end(void *ctx);

/* Notify us that a device is no longer available.
 * We'll drop any state associated with it.
 */
void inmgr_disconnect(int devid);

/* Extra features for Romassist, not in the original inmgr.
 *******************************************************************************/

/* Call (cb) with every input event after we process it.
 * (state) is the digested state of logical bits, after processing this raw event.
 * (devid,0,0) means connect. Reported after successful configuration; unconfigured devices do not report.
 * (devid,-1,-1) means disconnect.
 * Returns >0 spyid.
 * It's safe to remove a spy during its own callback, but not others.
 * Artificial events are not reported.
 */
int inmgr_spy(void (*cb)(int devid,int btnid,int value,int state,void *userdata),void *userdata);
void inmgr_unspy(int spyid);

/* Usually in conjunction with a spy, temporarily suppress events from one device.
 * A disabled device will still map as usual but will stop delivering its state to the common player outputs.
 */
void inmgr_device_enable(int devid,int enable);

int inmgr_devid_by_index(int p);

int inmgr_get_dstbtnid(int devid,int srcbtnid);
int inmgr_remap_button(int devid,int srcbtnid,int dstbtnid,int hidusage,int srclo,int srchi,int value);

const char *inmgr_btnid_name(int btnid); // Short canned names.

#endif
