/* evdev.h
 * Minimal interface to Linux evdev, for joysticks and such.
 */
 
#ifndef EVDEV_H
#define EVDEV_H

struct evdev;
struct evdev_device;
struct eh_input_delegate;

struct evdev_setup {
  const char *path; // "/dev/input" if you don't specify
  int use_inotify; // If zero we won't automatically detect connections. (why would you want that?)
  int grab_devices; // Normally 1. Should we attempt EVIOCGRAB on connect.
  int guess_hid; // Normally 1, to make up HID usage codes based on Linux type/code where possible.
};

// Since the defaults are not zero, it's wise to start with this:
void evdev_setup_default(struct evdev_setup *setup);

/* Driver context.
 ***********************************************************************/

void evdev_del(struct evdev *evdev);

struct evdev *evdev_new(
  const struct eh_input_delegate *delegate,
  const struct evdev_setup *setup
);

void *evdev_get_userdata(const struct evdev *evdev);

/* Use plain update and we invoke poll() ourselves, that's usually what you want.
 * You can use this API with an external poller too: evdev_update_fd() when one of my files becomes readable.
 * (You must get the inotify fd at startup, and update your fd list on connect and disconnect events).
 */
int evdev_update(struct evdev *evdev,int toms);
int evdev_update_fd(struct evdev *evdev,int fd);

/* Request to manually scan the device directory at next update.
 * This does not perform any IO immediately.
 */
void evdev_scan(struct evdev *evdev);

// <0 if inotify is defunct. In which case you might want to scan periodically.
int evdev_get_inotify_fd(const struct evdev *evdev);

int evdev_count_devices(const struct evdev *evdev);
struct evdev_device *evdev_device_by_index(const struct evdev *evdev,int p);
struct evdev_device *evdev_device_by_fd(const struct evdev *evdev,int fd);
struct evdev_device *evdev_device_by_kid(const struct evdev *evdev,int kid);
struct evdev_device *evdev_device_by_devid(const struct evdev *evdev,int devid);

/* Device.
 ***********************************************************************/
 
struct evdev *evdev_device_get_context(const struct evdev_device *device);
void *evdev_device_get_userdata(const struct evdev_device *device); // context's userdata, just for convenience.
int evdev_device_get_fd(const struct evdev_device *device);
int evdev_device_get_kid(const struct evdev_device *device); // eg 3 for /dev/input/event3
int evdev_device_get_devid(const struct evdev_device *device); // yours to assign; zero by default
int evdev_device_set_devid(struct evdev_device *device,int devid);

/* Name is fetched from the kernel each time you ask for it.
 */
int evdev_device_get_name(char *dst,int dsta,const struct evdev_device *device);

// Hardware IDs are fetched the first time you ask for one, then we cache them on the device.
int evdev_device_get_vid(struct evdev_device *device);
int evdev_device_get_pid(struct evdev_device *device);
int evdev_device_get_version(struct evdev_device *device);
int evdev_device_get_bustype(struct evdev_device *device);

/* List all device capabilities. This does perform IO.
 * (usage) is our best guess at HID usage. It is not supplied by the kernel.
 * Internally, (type) is 8 bits and (code) is 16. So they can safely be combined like ((type<<16)|code).
 */
int evdev_device_enumerate(
  struct evdev_device *device,
  int (*cb)(int type,int code,int usage,int lo,int hi,int value,void *userdata),
  void *userdata
);

/* Odds, ends.
 ***************************************************************************/

int evdev_guess_hid_usage(int type,int code);

#endif
