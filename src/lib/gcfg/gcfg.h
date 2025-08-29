/* gcfg.h
 * Global configuration files support.
 * Uses: fs
 */
 
#ifndef GCFG_H
#define GCFG_H

/* A typical gcfg path is "/home/andy/.config/aksomm/MyAwesomeGame/settings.cfg",
 * where (app) is "MyAwesomeGame" and (base) is "settings.cfg".
 * (app) and (base) are both optional. Either may contain slashes.
 * Fails if it doesn't fit.
 */
int gcfg_compose_path(char *dst,int dsta,const char *app,int appc,const char *base,int basec);

/* Convenience.
 * Compose the path, then read the file.
 * Unlike file_read(), if the length is zero, we guarantee (*dstpp) will be null.
 * And missing file (or I/O errors) are not reported as errors.
 * We assume that a missing config file is no problem.
 */
int gcfg_read_file(void *dstpp,const char *app,int appc,const char *base,int basec);

/* Convenience.
 * Compose the path, make directories recursively, and write the file.
 * We do write empty files if you tell us to, and make the directories and everything in that case.
 */
int gcfg_write_file(const char *app,int appc,const char *base,int basec,const void *src,int srcc);

/* Input device definitions.
 * See further down for a more convenient form.
 * Following the strategy I employed for Romassist and Egg (and I intend to convert those to use gcfg eventually).
 * We fill a gap that HID left: Devices should map to a generic shape, the "Standard Mapping" from Web Gamepad API.
 * This is not the venue for mapping to like "jump", "change weapon", etc; that stuff belongs to the app.
 *
 * Config file is zero or more device blocks, eg:
 *   >>> 1234 0001 0000 "Acme Widget Co. Heavy Duty Gamepad"
 *   30010 HORZ
 *   30011 VERT reverse
 *   10120 SOUTH "A"
 *   10121 WEST "B"
 * Each button can have a quoted name, and loose text commentary.
 * Name must come before commentary.
 * Integers are hexadecimal with no prefix.
 * Device and button names must not contain quotes or escapes.
 * Line comments start with '#', start of line only.
 * I don't recommend it, but you may continue reading either of these iterators after an error; the offending line is skipped.
 * This can read existing Romassist config files if you add quotes to the device names.
 */
struct gcfg_input_device_header {
  int vid,pid,version;
  const char *name; // Quotes dumbly stripped.
  int namec;
};
struct gcfg_input_button {
  int srcbtnid; // Recommend ((type<<16)|code) for evdev.
  int btnid; // <0 if failed to evaluate; check (btnname) in that case, if you support nonstandard names.
  const char *btnname;
  int btnnamec;
  const char *name; // Quotes dumbly stripped.
  int namec;
  const char *comment;
  int commentc;
};
#define GCFG_BTNID_NONE        0 /* Unknown, no appropriate symbol, or deliberately unused. */
#define GCFG_BTNID_DPAD        1 /* Entire dpad as a hat. Assume 8 values, clockwise from Up, and OOB if idle. */
#define GCFG_BTNID_HORZ        2 /* Dpad X axis. */
#define GCFG_BTNID_VERT        3 /* Dpad Y axis. */
#define GCFG_BTNID_LEFT        4 /* Dpad... */
#define GCFG_BTNID_RIGHT       5
#define GCFG_BTNID_UP          6
#define GCFG_BTNID_DOWN        7
#define GCFG_BTNID_SOUTH       8 /* Right thumb buttons... */
#define GCFG_BTNID_WEST        9
#define GCFG_BTNID_EAST       10
#define GCFG_BTNID_NORTH      11
#define GCFG_BTNID_L1         12 /* Triggers... */
#define GCFG_BTNID_R1         13
#define GCFG_BTNID_L2         14
#define GCFG_BTNID_R2         15
#define GCFG_BTNID_AUX1       16 /* Start */
#define GCFG_BTNID_AUX2       17 /* Select */
#define GCFG_BTNID_AUX3       18 /* Heart */
#define GCFG_BTNID_LP         19 /* Left stick plunger. */
#define GCFG_BTNID_RP         20 /* Right stick plunger. */
#define GCFG_BTNID_LX         21 /* Analogue sticks... */
#define GCFG_BTNID_LY         22
#define GCFG_BTNID_RX         23
#define GCFG_BTNID_RY         24
#define GCFG_FOR_EACH_BTNID \
  _(NONE) _(DPAD) _(HORZ) _(VERT) _(LEFT) _(RIGHT) _(UP) _(DOWN) \
  _(SOUTH) _(WEST) _(EAST) _(NORTH) \
  _(L1) _(R1) _(L2) _(R2) \
  _(AUX1) _(AUX2) _(AUX3) \
  _(LP) _(RP) _(LX) _(LY) _(RX) _(RY)

/* Initialize (v,c) and the rest zero. No cleanup necessary.
 * next_device() will skip any button definitions and return >0 if a device header was consumed.
 * next_button() will stop at device headers and return zero.
 */
struct gcfg_input_reader {
  const char *v;
  int c;
  int p;
};
int gcfg_input_next_device(struct gcfg_input_device_header *dst,struct gcfg_input_reader *reader);
int gcfg_input_next_button(struct gcfg_input_button *dst,struct gcfg_input_reader *reader);

/* Produce one line of text from the loose object, including a newline.
 */
int gcfg_input_device_header_repr(char *dst,int dsta,const struct gcfg_input_device_header *src);
int gcfg_input_button_repr(char *dst,int dsta,const struct gcfg_input_button *src);
int gcfg_input_device_header_eval(struct gcfg_input_device_header *dst,const char *src,int srcc);
int gcfg_input_button_eval(struct gcfg_input_button *dst,const char *src,int srcc);

int gcfg_hexuint_repr(char *dst,int dsta,int src);
int gcfg_btnid_repr(char *dst,int dsta,int src);
int gcfg_hexuint_eval(int *dst,const char *src,int srcc);
int gcfg_btnid_eval(int *dst,const char *src,int srcc);

/* Input device definitions, convenient form.
 */
struct gcfg_input_device {
  struct gcfg_input_device_header header;
  struct gcfg_input_button *buttonv;
  int buttonc,buttona;
  int mapid; // Arbitrary unique integer.
};
struct gcfg_input {
  char *src; // Borrowed at decode. We will free at cleanup if not null.
  int srcc;  // All other text points into this.
  struct gcfg_input_device *devicev;
  int devicec,devicea;
};
void gcfg_input_device_cleanup(struct gcfg_input_device *device);
void gcfg_input_del(struct gcfg_input *input);
struct gcfg_input_device *gcfg_input_add_device(struct gcfg_input *input);
struct gcfg_input_device *gcfg_input_get_device(const struct gcfg_input *input,int vid,int pid,int version,const char *name,int namec);
struct gcfg_input_button *gcfg_input_device_get_button(struct gcfg_input_device *device,int srcbtnid,int add_if_missing);

/* Read input device definitions from the standard global location.
 * Missing or malformed file does not prevent us from creating the input manifold, basically it never fails.
 * Also, all of these high-level calls are null-safe. So you don't need to check for errors if you don't want to.
 */
struct gcfg_input *gcfg_input_get();

#endif
