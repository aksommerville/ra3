#include "evdev_internal.h"

/* Delete.
 */
 
void evdev_device_del(struct evdev_device *device) {
  if (!device) return;
  if (device->fd>=0) close(device->fd);
  free(device);
}

/* Trivial accessors.
 */
 
struct evdev *evdev_device_get_context(const struct evdev_device *device) {
  if (!device) return 0;
  return device->evdev;
}

void *evdev_device_get_userdata(const struct evdev_device *device) {
  if (!device) return 0;
  return device->evdev->delegate.userdata;
}

int evdev_device_get_fd(const struct evdev_device *device) {
  if (!device) return -1;
  return device->fd;
}

int evdev_device_get_kid(const struct evdev_device *device) {
  if (!device) return -1;
  return device->kid;
}

int evdev_device_get_devid(const struct evdev_device *device) {
  if (!device) return 0;
  return device->devid;
}

int evdev_device_set_devid(struct evdev_device *device,int devid) {
  if (!device) return -1;
  device->devid=devid;
  return 0;
}

/* Get device name.
 */

int evdev_device_get_name(char *dst,int dsta,const struct evdev_device *device) {
  if (!device) return -1;
  if (!dst||(dsta<0)) dsta=0;
  if (!dsta) {
    char scratch[256];
    return ioctl(device->fd,EVIOCGNAME(sizeof(scratch)),scratch);
  }
  int dstc=ioctl(device->fd,EVIOCGNAME(dsta),dst);
  if (dstc<0) return -1;
  if (dstc>=dsta) return dstc;
  // When we get something valid and it fits, clean it up a little.
  int spcc=0; while ((spcc<dstc)&&((unsigned char)dst[spcc]<=0x20)) spcc++;
  if (spcc) {
    dstc-=spcc;
    memmove(dst,dst+spcc,dstc);
  }
  while (dstc&&((unsigned char)dst[dstc-1]<=0x20)) dstc--;
  int i=0; for (;i<dstc;i++) {
    if ((dst[i]<0x20)||(dst[i]>0x7e)) dst[i]='?';
  }
  dst[dstc]=0;
  return dstc;
}

/* Get IDs.
 */
 
static void evdev_device_require_ids(struct evdev_device *device) {
  if (device->vid||device->pid||device->version||device->bustype) return;
  struct input_id id={0};
  ioctl(device->fd,EVIOCGID,&id);
  device->vid=id.vendor;
  device->pid=id.product;
  device->version=id.version;
  device->bustype=id.bustype;
}

int evdev_device_get_vid(struct evdev_device *device) {
  if (!device) return 0;
  evdev_device_require_ids(device);
  return device->vid;
}

int evdev_device_get_pid(struct evdev_device *device) {
  if (!device) return 0;
  evdev_device_require_ids(device);
  return device->pid;
}

int evdev_device_get_version(struct evdev_device *device) {
  if (!device) return 0;
  evdev_device_require_ids(device);
  return device->version;
}

int evdev_device_get_bustype(struct evdev_device *device) {
  if (!device) return 0;
  evdev_device_require_ids(device);
  return device->bustype;
}

/* Enumerate buttons.
 */
 
struct evdev_key_usage {
  int code;
  int usage;
};

static int evdev_key_usage_cmp(const void *a,const void *b) {
  int acode=((struct evdev_key_usage*)a)->code;
  int bcode=((struct evdev_key_usage*)b)->code;
  return acode-bcode;
}

static int evdev_key_usage_search(const struct evdev_key_usage *v,int c,int code) {
  int lo=0,hi=c;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (code<v[ck].code) hi=ck;
    else if (code>v[ck].code) lo=ck+1;
    else return v[ck].usage;
  }
  return 0;
}
 
static int evdev_device_enumerate_type(
  struct evdev_device *device,
  int (*cb)(int type,int code,int usage,int lo,int hi,int value,void *userdata),
  void *userdata,
  int type,int lo,int hi,
  uint8_t *bits,int bitslen,
  const struct evdev_key_usage *keyusagev,int keyusagec
) {
  int bitsc=ioctl(device->fd,EVIOCGBIT(type,bitslen),bits);
  if (bitsc<1) return 0;
  int major=0,err;
  for (;major<bitsc;major++) {
    if (!bits[major]) continue;
    int minor=0;
    for (;minor<8;minor++) {
      if (!(bits[major]&(1<<minor))) continue;
      int code=(major<<3)|minor;
      int usage=0;
      int value=0;
      
      if (type==EV_ABS) {
        struct input_absinfo ai={0};
        if (ioctl(device->fd,EVIOCGABS(code),&ai)>=0) {
          lo=ai.minimum;
          hi=ai.maximum;
          value=ai.value;
        } else {
          lo=-32767;
          hi=32767;
        }

      } else if (type==EV_KEY) {
        usage=evdev_key_usage_search(keyusagev,keyusagec,code);
      }

      if (!usage&&device->evdev->guess_hid) usage=evdev_guess_hid_usage(type,code);
      
      if (err=cb(type,code,usage,lo,hi,value,userdata)) return err;
    }
  }
  return 0;
}

int evdev_device_enumerate(
  struct evdev_device *device,
  int (*cb)(int type,int code,int usage,int lo,int hi,int value,void *userdata),
  void *userdata
) {
  if (!device||!cb) return -1;
  
  /* This is a bit stilted.
   * evdev does supply HID usage codes from the device, and there's two ways to receive them:
   *  - EVIOCGKEYCODE_V2 whenever.
   *  - MSC_SCAN immediately before each event.
   * The second option is not helpful to us.
   * EVIOCGKEYCODE_V2 only allows searching by index, or reverse-searching by scancode (ie HID usage).
   * There's no search-by-event-code, which is what we're really after.
   * So pull the whole map in advance, and we can search it as we detect each key bit.
   * And sadly, this only applies to KEY events. Not ABS or REL.
   */
  struct evdev_key_usage keyusagev[256];
  int keyusagec=0;
  while (keyusagec<256) {
    struct input_keymap_entry entry={
      .flags=INPUT_KEYMAP_BY_INDEX,
      .index=keyusagec,
    };
    if (ioctl(device->fd,EVIOCGKEYCODE_V2,&entry)<0) break;
    if (entry.len==4) {
      struct evdev_key_usage *u=keyusagev+keyusagec++;
      u->code=entry.keycode;
      u->usage=*(uint32_t*)entry.scancode;
    } else break;
  }
  qsort(keyusagev,keyusagec,sizeof(struct evdev_key_usage),evdev_key_usage_cmp);
  
  /* OK, now examine all the potentially interesting event bits.
   */
  uint8_t bits[(KEY_CNT+7)>>3]; // There are way more KEY events than any other type.
  int err;
  if (err=evdev_device_enumerate_type(device,cb,userdata,EV_KEY,0,2,bits,sizeof(bits),keyusagev,keyusagec)) return err;
  if (err=evdev_device_enumerate_type(device,cb,userdata,EV_ABS,0,0,bits,sizeof(bits),keyusagev,keyusagec)) return err;
  if (err=evdev_device_enumerate_type(device,cb,userdata,EV_REL,-32767,32767,bits,sizeof(bits),keyusagev,keyusagec)) return err;
  if (err=evdev_device_enumerate_type(device,cb,userdata,EV_MSC,0,1,bits,sizeof(bits),keyusagev,keyusagec)) return err;
  if (err=evdev_device_enumerate_type(device,cb,userdata,EV_SW,0,1,bits,sizeof(bits),keyusagev,keyusagec)) return err;
  return 0;
}
