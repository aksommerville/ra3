#include "evdev_internal.h"
#include <dirent.h>

/* Consider one file under the device directory.
 */
 
static int evdev_check_file(struct evdev *evdev,const char *base,int basec) {
  
  // First confirm the name looks like evdev, has a kid, and is not already open.
  if ((basec<6)||memcmp(base,"event",5)) return 0;
  int kid=0,basep=5;
  for (;basep<basec;basep++) {
    if ((base[basep]<'0')||(base[basep]>'9')) return 0;
    kid*=10;
    kid+=base[basep]-'0';
    // prevent overflow weirdness, when you've connected the billionth device.
    if (kid>999999999) return 0;
  }
  if (evdev_device_by_kid(evdev,kid)) return 0;
  
  // Grow device list if needed.
  // Errors here don't need to be fatal, but let's do it since it indicates a pretty big systemic problem.
  if (evdev->devicec>=evdev->devicea) {
    int na=evdev->devicea+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(evdev->devicev,sizeof(void*)*na);
    if (!nv) return -1;
    evdev->devicev=nv;
    evdev->devicea=na;
  }
  
  // Open the file. No worries if we can't.
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s/%.*s",evdev->path,basec,base);
  if ((pathc<1)||(pathc>=sizeof(path))) return 0;
  int fd=open(path,O_RDONLY);
  if (fd<0) return 0;
  
  // Validate evdev version, and implicitly assert that it really is an evdev file.
  int evdev_version=0;
  if ((ioctl(fd,EVIOCGVERSION,&evdev_version)<0)||(evdev_version<0x00010000)) {
    fprintf(stderr,"%s: Unsupported evdev version 0x%08x. Compiled for 0x%08x.\n",path,evdev_version,EV_VERSION);
    close(fd);
    return 0;
  }
  
  // Try to grab the device. (prevent other programs from using it while we're running)
  if (evdev->grab_devices) {
    int grab=1;
    ioctl(fd,EVIOCGRAB,&grab);
  }
  
  // I'd like to use EVIOCSMASK to disable SYN_REPORT and MSC_SCAN, but I only ever get EINVAL from it.
  
  // Create a new device.
  struct evdev_device *device=calloc(1,sizeof(struct evdev_device));
  if (!device) {
    close(fd);
    return -1;
  }
  evdev->devicev[evdev->devicec++]=device;
  device->evdev=evdev;
  device->fd=fd;
  device->kid=kid;
  
  // Tell our owner, and we're done.
  if (evdev->delegate.cb_connect) evdev->delegate.cb_connect(device->devid,evdev->delegate.userdata);
  return 1;
}

/* Scan device directory.
 */
 
static int evdev_scan_now(struct evdev *evdev) {
  DIR *dir=opendir(evdev->path);
  if (!dir) return -1;
  struct dirent *de;
  while (de=readdir(dir)) {
    
    // Can safely drop this bit if you don't have d_type; it's just a wee optimization.
    if (de->d_type!=DT_CHR) continue;
    
    const char *base=de->d_name;
    int basec=0;
    while (base[basec]) basec++;
    int err=evdev_check_file(evdev,base,basec);
    if (err<0) {
      closedir(dir);
      return err;
    }
  }
  closedir(dir);
  return 0;
}

/* Update inotify.
 */
 
static int evdev_update_inotify(struct evdev *evdev) {
  char buf[1024];
  int bufc=read(evdev->infd,buf,sizeof(buf));
  if (bufc<=0) {
    close(evdev->infd);
    evdev->infd=-1;
    return 0;
  }
  int bufp=0;
  while (bufp<bufc) {
    struct inotify_event *event=(struct inotify_event*)(buf+bufp);
    bufp+=sizeof(struct inotify_event);
    const char *base=event->name;
    int basec=0;
    while ((basec<event->len)&&base[basec]) basec++;
    bufp+=event->len;
    int err=evdev_check_file(evdev,base,basec);
    if (err<0) return err;
  }
  return 0;
}

/* Update device.
 */
 
static int evdev_update_device(struct evdev *evdev,struct evdev_device *device) {
  struct input_event eventv[16];
  int eventc=read(device->fd,eventv,sizeof(eventv));
  if (eventc<=0) {
    evdev_drop_device(evdev,device,1);
    return 0;
  }
  if (evdev->delegate.cb_button) {
    eventc/=sizeof(struct input_event);
    const struct input_event *event=eventv;
    for (;eventc-->0;event++) {
    
      // Would be preferable to filter these out with the event mask, EVIOCSMASK, but I can't seem to get that working.
      if (event->type==EV_SYN) {
        // These are noisy and not interesting.
        continue;
      }
      if ((event->type==EV_MSC)&&(event->code==MSC_SCAN)) {
        // Everything useful we can get from these, we got it during enumeration.
        continue;
      }
    
      evdev->delegate.cb_button(device->devid,(event->type<<16)|event->code,event->value,evdev->delegate.userdata);
    }
  }
  return 0;
}

/* Update one file.
 */
 
int evdev_update_fd(struct evdev *evdev,int fd) {
  if (!evdev) return -1;
  if (fd<0) return -1;
  if (fd==evdev->infd) return evdev_update_inotify(evdev);
  struct evdev_device *device=evdev_device_by_fd(evdev,fd);
  if (device) return evdev_update_device(evdev,device);
  return 0;
}

/* Rebuild pollfdv.
 */
 
static int evdev_rebuild_pollfdv(struct evdev *evdev) {
  evdev->pollfdc=0;
  
  int na=evdev->devicec;
  if (evdev->infd>=0) na++;
  if (na>evdev->pollfda) {
    if (na<INT_MAX-16) na+=16;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(evdev->pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    evdev->pollfdv=nv;
    evdev->pollfda=na;
  }
  
  if (evdev->infd>=0) {
    struct pollfd *pollfd=evdev->pollfdv+evdev->pollfdc++;
    pollfd->fd=evdev->infd;
    pollfd->events=POLLIN|POLLERR|POLLHUP;
    pollfd->revents=0;
  }
  
  struct evdev_device **v=evdev->devicev;
  int i=evdev->devicec;
  for (;i-->0;v++) {
    struct pollfd *pollfd=evdev->pollfdv+evdev->pollfdc++;
    pollfd->fd=(*v)->fd;
    pollfd->events=POLLIN|POLLERR|POLLHUP;
    pollfd->revents=0;
  }
  
  return 0;
}

/* Update with poll.
 */

int evdev_update(struct evdev *evdev,int toms) {
  if (!evdev) return 0;
  if (evdev->rescan) {
    evdev->rescan=0;
    if (evdev_scan_now(evdev)<0) return -1;
  }
  if (evdev_rebuild_pollfdv(evdev)<0) return -1;
  if (evdev->pollfdc<1) {
    if (toms>0) usleep(toms*1000);
    return 0;
  }
  int err=poll(evdev->pollfdv,evdev->pollfdc,toms);
  if (err<=0) return 0;
  const struct pollfd *pollfd=evdev->pollfdv;
  int i=evdev->pollfdc;
  for (;i-->0;pollfd++) {
    if (!pollfd->revents) continue;
    if (evdev_update_fd(evdev,pollfd->fd)<0) return -1;
  }
  return err;
}
