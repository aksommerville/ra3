#include "evdev_internal.h"

/* Delete.
 */

void evdev_del(struct evdev *evdev) {
  if (!evdev) return;
  if (evdev->infd>=0) close(evdev->infd);
  if (evdev->devicev) {
    while (evdev->devicec-->0) evdev_device_del(evdev->devicev[evdev->devicec]);
    free(evdev->devicev);
  }
  if (evdev->pollfdv) free(evdev->pollfdv);
  free(evdev);
}

/* Set path.
 */
 
static int evdev_set_path(struct evdev *evdev,const char *src) {
  if (evdev->path) free(evdev->path);
  if (src) {
    if (!(evdev->path=strdup(src))) return -1;
  } else {
    evdev->path=0;
  }
  return 0;
}

/* Initialize inotify.
 */
 
static int evdev_init_inotify(struct evdev *evdev) {
  if ((evdev->infd=inotify_init())<0) return -1;
  if (inotify_add_watch(evdev->infd,evdev->path,IN_ATTRIB|IN_CREATE|IN_MOVED_TO)<0) return -1;
  return 0;
}

/* New.
 */

struct evdev *evdev_new(
  const struct eh_input_delegate *delegate,
  const struct evdev_setup *setup
) {
  struct evdev *evdev=calloc(1,sizeof(struct evdev));
  if (!evdev) return 0;
  evdev->infd=-1;
  evdev->rescan=1;
  
  if (delegate) evdev->delegate=*delegate;
  
  if (setup) {
    evdev->grab_devices=setup->grab_devices;
    evdev->guess_hid=setup->guess_hid;
  } else {
    evdev->grab_devices=1;
    evdev->guess_hid=1;
  }
  
  int err;
  if (setup&&setup->path&&setup->path[0]) err=evdev_set_path(evdev,setup->path);
  else err=evdev_set_path(evdev,"/dev/input");
  if (err<0) {
    evdev_del(evdev);
    return 0;
  }
  
  if (setup&&!setup->use_inotify) {
    // No inotify. Whatever you say...
  } else {
    if (evdev_init_inotify(evdev)<0) {
      evdev_del(evdev);
      return 0;
    }
  }
  
  return evdev;
}

/* Trivial accessors.
 */

void *evdev_get_userdata(const struct evdev *evdev) {
  if (!evdev) return 0;
  return evdev->delegate.userdata;
}

void evdev_scan(struct evdev *evdev) {
  if (!evdev) return;
  evdev->rescan=1;
}

int evdev_get_inotify_fd(const struct evdev *evdev) {
  if (!evdev) return -1;
  return evdev->infd;
}

int evdev_count_devices(const struct evdev *evdev) {
  if (!evdev) return 0;
  return evdev->devicec;
}

/* Get device from list.
 */

struct evdev_device *evdev_device_by_index(const struct evdev *evdev,int p) {
  if (!evdev) return 0;
  if (p<0) return 0;
  if (p>=evdev->devicec) return 0;
  return evdev->devicev[p];
}

struct evdev_device *evdev_device_by_fd(const struct evdev *evdev,int fd) {
  if (!evdev) return 0;
  struct evdev_device **v=evdev->devicev;
  int i=evdev->devicec;
  for (;i-->0;v++) if ((*v)->fd==fd) return *v;
  return 0;
}

struct evdev_device *evdev_device_by_kid(const struct evdev *evdev,int kid) {
  if (!evdev) return 0;
  struct evdev_device **v=evdev->devicev;
  int i=evdev->devicec;
  for (;i-->0;v++) if ((*v)->kid==kid) return *v;
  return 0;
}

struct evdev_device *evdev_device_by_devid(const struct evdev *evdev,int devid) {
  if (!evdev) return 0;
  struct evdev_device **v=evdev->devicev;
  int i=evdev->devicec;
  for (;i-->0;v++) if ((*v)->devid==devid) return *v;
  return 0;
}

/* Remove device from list and delete it.
 */
 
void evdev_drop_device(struct evdev *evdev,struct evdev_device *device,int fire_event) {
  int p=-1,i=evdev->devicec;
  while (i-->0) {
    if (evdev->devicev[i]==device) {
      p=i;
      break;
    }
  }
  if (p<0) return;
  evdev->devicec--;
  memmove(evdev->devicev+p,evdev->devicev+p+1,sizeof(void*)*(evdev->devicec-p));
  if (fire_event&&evdev->delegate.cb_disconnect) {
    evdev->delegate.cb_disconnect(device->devid,evdev->delegate.userdata);
  }
  evdev_device_del(device);
}

/* Default setup.
 */
 
void evdev_setup_default(struct evdev_setup *setup) {
  memset(setup,0,sizeof(struct evdev_setup));
  setup->path="/dev/input";
  setup->use_inotify=1;
  setup->grab_devices=1;
  setup->guess_hid=1;
}
