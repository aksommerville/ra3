#ifndef EVDEV_INTERNAL_H
#define EVDEV_INTERNAL_H

#include "evdev.h"
#include "lib/eh_driver.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/inotify.h>

struct evdev {
  char *path;
  int infd;
  int rescan;
  struct eh_input_delegate delegate;
  int grab_devices;
  int guess_hid;
  
  // Not sorted. (well technically, sorted by age old to new)
  struct evdev_device **devicev;
  int devicec,devicea;
  
  struct pollfd *pollfdv;
  int pollfdc,pollfda;
};

struct evdev_device {
  int fd;
  int kid;
  int devid;
  int vid,pid,version,bustype;
  struct evdev *evdev; // WEAK
};

void evdev_device_del(struct evdev_device *device);
void evdev_drop_device(struct evdev *evdev,struct evdev_device *device,int fire_event);

#endif
