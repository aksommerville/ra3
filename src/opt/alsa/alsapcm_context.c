#include "alsapcm_internal.h"

/* Optional master level check.
 * For troubleshooting and audio design. Don't leave enabled!
 */

#define MASTER_LEVEL_CHECK_ENABLE 0

/* Check period in seconds, but we treat the signal as mono.
 * So if it's stereo, we will report twice as fast.
 */
#define MASTER_LEVEL_CHECK_PERIOD 1.0

#define MASTER_LEVEL_CHECK_REPORT_WIDTH 75 /* in terminal columns */

#if MASTER_LEVEL_CHECK_ENABLE
  #include <math.h>
  
  static float master_peak=0.0f;
  static float master_sqsum=0.0f;
  static int master_count=0;
  static int master_period=0;
  
  // Finalize and dump current state, then reset for the next cycle.
  static void master_level_check_report() {
    float rms=sqrtf(master_sqsum/master_count);
    
    char report[MASTER_LEVEL_CHECK_REPORT_WIDTH];
    int rmsp=rms*MASTER_LEVEL_CHECK_REPORT_WIDTH;
    int peakp=master_peak*MASTER_LEVEL_CHECK_REPORT_WIDTH;
    if (rmsp<0) rmsp=0;
    if (peakp>=MASTER_LEVEL_CHECK_REPORT_WIDTH) peakp=MASTER_LEVEL_CHECK_REPORT_WIDTH-1;
    else if (peakp<1) peakp=1;
    if (rmsp>=peakp) rmsp=peakp-1;
    memset(report,'X',rmsp);
    memset(report+rmsp,'-',peakp-rmsp);
    memset(report+peakp,' ',sizeof(report)-peakp);
    report[MASTER_LEVEL_CHECK_REPORT_WIDTH-1]='|';
    report[rmsp]='R';
    report[peakp]='!';
    fprintf(stderr,"%.*s\n",MASTER_LEVEL_CHECK_REPORT_WIDTH,report);
    
    master_count=0;
    master_peak=0.0f;
    master_sqsum=0.0f;
  }
  
  // Read the signal and update counts. Caller ensures it's within one period.
  static void master_level_check_update(const int16_t *v,int c) {
    master_count+=c;
    for (;c-->0;v++) {
      float fv=(float)*v;
      if (fv<0.0f) fv=-fv;
      fv/=32768.0f;
      if (fv>master_peak) master_peak=fv;
      master_sqsum+=fv*fv;
    }
  }

  static void master_level_check(const int16_t *v,int c,int rate,int chanc) {
    if (!master_period) {
      master_period=(MASTER_LEVEL_CHECK_PERIOD*rate)/chanc;
      if (master_period<1) {
        master_period=1000;
        fprintf(stderr,
          "%s:%d: MASTER_LEVEL_CHECK_PERIOD too small (%f, with main rate %d and chanc %d). Raised to %d samples.\n",
          __FILE__,__LINE__,MASTER_LEVEL_CHECK_PERIOD,rate,chanc,master_period
        );
      } else {
        fprintf(stderr,"%s:%d: MASTER_LEVEL_CHECK_ENABLE set, will dump audio output levels.\n",__FILE__,__LINE__);
      }
    }
    while (c>0) {
      int updc=master_period-master_count;
      if (updc>c) updc=c;
      master_level_check_update(v,updc);
      v+=updc;
      c-=updc;
      if (master_count>=master_period) {
        master_level_check_report();
      }
    }
  }
#endif

/* I/O thread.
 */
 
static void *alsapcm_iothd(void *arg) {
  struct alsapcm *alsapcm=arg;
  while (1) {
    pthread_testcancel();
    
    if (pthread_mutex_lock(&alsapcm->iomtx)) {
      usleep(1000);
      continue;
    }
    if (alsapcm->running) {
      alsapcm->delegate.pcm_out(alsapcm->buf,alsapcm->bufa,alsapcm->delegate.userdata);
    } else {
      memset(alsapcm->buf,0,alsapcm->bufa<<1);
    }
    pthread_mutex_unlock(&alsapcm->iomtx);
    
    #if MASTER_LEVEL_CHECK_ENABLE
      master_level_check(alsapcm->buf,alsapcm->bufa,alsapcm->rate,alsapcm->chanc);
    #endif
    
    const uint8_t *src=(uint8_t*)alsapcm->buf;
    int srcc=alsapcm->bufa<<1; // bytes (from samples)
    int srcp=0;
    while (srcp<srcc) {
      pthread_testcancel();
      int pvcancel;
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&pvcancel);
      int err=write(alsapcm->fd,src+srcp,srcc-srcp);
      pthread_setcancelstate(pvcancel,0);
      if (err<0) {
        if (errno==EPIPE) {
          if (
            (ioctl(alsapcm->fd,SNDRV_PCM_IOCTL_DROP)<0)||
            (ioctl(alsapcm->fd,SNDRV_PCM_IOCTL_DRAIN)<0)||
            (ioctl(alsapcm->fd,SNDRV_PCM_IOCTL_PREPARE)<0)
          ) {
            alsapcm_error(alsapcm,"write","Failed to recover from underrun: %m");
            alsapcm->ioerror=-1;
            return 0;
          }
          alsapcm_error(alsapcm,"io","Recovered from underrun");
        } else {
          alsapcm_error(alsapcm,"write",0);
          alsapcm->ioerror=-1;
          return 0;
        }
      } else {
        srcp+=err;
      }
    }
  }
}

/* Delete context.
 */
 
void alsapcm_del(struct alsapcm *alsapcm) {
  if (!alsapcm) return;
  
  if (alsapcm->iothd) {
    pthread_cancel(alsapcm->iothd);
    pthread_join(alsapcm->iothd,0);
  }
  
  if (alsapcm->fd>=0) close(alsapcm->fd);
  if (alsapcm->device) free(alsapcm->device);
  if (alsapcm->buf) free(alsapcm->buf);
  
  free(alsapcm);
}

/* Init: Populate (alsapcm->device) with path to the device file.
 * Chooses a device if necessary.
 */
 
static int alsapcm_select_device_path(
  struct alsapcm *alsapcm,
  const struct alsapcm_setup *setup
) {
  if (alsapcm->device) return 0;
  
  // If caller supplied a device path or basename, that trumps all.
  if (setup&&setup->device&&setup->device[0]) {
    if (setup->device[0]=='/') {
      if (!(alsapcm->device=strdup(setup->device))) return -1;
      return 0;
    }
    char tmp[1024];
    int tmpc=snprintf(tmp,sizeof(tmp),"/dev/snd/%s",setup->device);
    if ((tmpc<1)||(tmpc>=sizeof(tmp))) return -1;
    if (!(alsapcm->device=strdup(tmp))) return -1;
    return 0;
  }
  
  // Use some sensible default (rate,chanc) for searching, unless the caller specifies.
  int ratelo=22050,ratehi=48000;
  int chanclo=1,chanchi=2;
  if (setup) {
    if (setup->rate>0) ratelo=ratehi=setup->rate;
    if (setup->chanc>0) chanclo=chanchi=setup->chanc;
  }
  
  // Searching explicitly in "/dev/snd" forces return of an absolute path.
  // If we passed null instead, we'd get the same result, but the basename only.
  if (!(alsapcm->device=alsapcm_find_device("/dev/snd",ratelo,ratehi,chanclo,chanchi))) {
    //fprintf(stderr,"alsapcm_find_device failed (%d..%d,%d..%d)\n",ratelo,ratehi,chanclo,chanchi);
    // If it failed with the exact setup params, that's ok, try again with the default ranges.
    // It's normal for the first try to fail, since it asks for mono and some devices, eg mine, will only allow stereo.
    if (setup) {
      if (alsapcm->device=alsapcm_find_device("/dev/snd",22050,48000,1,2)) return 0;
    }
    return -1;
  }
  return 0;
}

/* Init: Open the device file. (alsapcm->device) must be set.
 */
 
static int alsapcm_open_device(struct alsapcm *alsapcm) {
  if (alsapcm->fd>=0) return 0;
  if (!alsapcm->device) return -1;
  
  if ((alsapcm->fd=open(alsapcm->device,O_WRONLY))<0) {
    return alsapcm_error(alsapcm,"open",0);
  }
  
  // Request ALSA version, really just confirming that the ioctl works, ie it's an ALSA PCM device.
  if (ioctl(alsapcm->fd,SNDRV_PCM_IOCTL_PVERSION,&alsapcm->protocol_version)<0) {
    alsapcm_error(alsapcm,"SNDRV_PCM_IOCTL_PVERSION",0);
    close(alsapcm->fd);
    alsapcm->fd=-1;
    return -1;
  }
  
  fprintf(stderr,"alsapcm: Using device '%s'\n",alsapcm->device);

  return 0;
}

/* Init: With device open, send the handshake ioctls to configure it.
 */
 
static int alsapcm_configure_device(
  struct alsapcm *alsapcm,
  const struct alsapcm_setup *setup
) {

  /* Refine hw params against the broadest set of criteria, anything we can technically handle.
   * (we impose a hard requirement for s16 interleaved; that's about it).
   */
  struct snd_pcm_hw_params hwparams;
  alsapcm_hw_params_none(&hwparams);
  hwparams.flags=SNDRV_PCM_HW_PARAMS_NORESAMPLE;
  alsapcm_hw_params_set_mask(&hwparams,SNDRV_PCM_HW_PARAM_ACCESS,SNDRV_PCM_ACCESS_RW_INTERLEAVED,1);
  alsapcm_hw_params_set_mask(&hwparams,SNDRV_PCM_HW_PARAM_FORMAT,SNDRV_PCM_FORMAT_S16,1);
  alsapcm_hw_params_set_mask(&hwparams,SNDRV_PCM_HW_PARAM_SUBFORMAT,SNDRV_PCM_SUBFORMAT_STD,1);
  alsapcm_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_SAMPLE_BITS,16,16);
  alsapcm_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_FRAME_BITS,0,UINT_MAX);
  alsapcm_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_CHANNELS,ALSAPCM_CHANC_MIN,ALSAPCM_CHANC_MAX);
  alsapcm_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_RATE,ALSAPCM_RATE_MIN,ALSAPCM_RATE_MAX);
  alsapcm_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_PERIOD_TIME,0,UINT_MAX); // us between interrupts
  alsapcm_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_PERIOD_SIZE,0,UINT_MAX); // frames between interrupts
  alsapcm_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_PERIOD_BYTES,0,UINT_MAX); // bytes between interrupts
  alsapcm_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_PERIODS,0,UINT_MAX); // interrupts per buffer
  alsapcm_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_BUFFER_TIME,0,UINT_MAX); // us
  alsapcm_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_BUFFER_SIZE,ALSAPCM_BUF_MIN,ALSAPCM_BUF_MAX); // frames
  alsapcm_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_BUFFER_BYTES,0,UINT_MAX);
  alsapcm_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_TICK_TIME,0,UINT_MAX); // us
              
  if (ioctl(alsapcm->fd,SNDRV_PCM_IOCTL_HW_REFINE,&hwparams)<0) {
    return alsapcm_error(alsapcm,"SNDRV_PCM_IOCTL_HW_REFINE",0);
  }

  if (setup) {
    if (setup->rate>0) alsapcm_hw_params_set_nearest_interval(&hwparams,SNDRV_PCM_HW_PARAM_RATE,setup->rate);
    if (setup->chanc>0) alsapcm_hw_params_set_nearest_interval(&hwparams,SNDRV_PCM_HW_PARAM_CHANNELS,setup->chanc);
    if (setup->buffersize>0) alsapcm_hw_params_set_nearest_interval(&hwparams,SNDRV_PCM_HW_PARAM_BUFFER_SIZE,setup->buffersize);
  }

  if (ioctl(alsapcm->fd,SNDRV_PCM_IOCTL_HW_PARAMS,&hwparams)<0) {
    return alsapcm_error(alsapcm,"SNDRV_PCM_IOCTL_HW_PARAMS",0);
  }

  if (ioctl(alsapcm->fd,SNDRV_PCM_IOCTL_PREPARE)<0) {
    return alsapcm_error(alsapcm,"SNDRV_PCM_IOCTL_PREPARE",0);
  }
  
  // Read the final agreed values off hwparams.
  if (!hwparams.rate_den) return -1;
  alsapcm->rate=hwparams.rate_num/hwparams.rate_den;
  if (alsapcm_hw_params_assert_exact_interval(&alsapcm->chanc,&hwparams,SNDRV_PCM_HW_PARAM_CHANNELS)<0) return -1;
  if (alsapcm_hw_params_assert_exact_interval(&alsapcm->hwbufframec,&hwparams,SNDRV_PCM_HW_PARAM_BUFFER_SIZE)<0) return -1;
  
  // Validate.
  if ((alsapcm->rate<ALSAPCM_RATE_MIN)||(alsapcm->rate>ALSAPCM_RATE_MAX)) {
    return alsapcm_error(alsapcm,"","Rejecting rate %d, limit %d..%d.",alsapcm->rate,ALSAPCM_RATE_MIN,ALSAPCM_RATE_MAX);
  }
  if ((alsapcm->chanc<ALSAPCM_CHANC_MIN)||(alsapcm->chanc>ALSAPCM_CHANC_MAX)) {
    return alsapcm_error(alsapcm,"","Rejecting chanc %d, limit %d..%d",alsapcm->chanc,ALSAPCM_CHANC_MIN,ALSAPCM_CHANC_MAX);
  }
  if ((alsapcm->hwbufframec<ALSAPCM_BUF_MIN)||(alsapcm->hwbufframec>ALSAPCM_BUF_MAX)) {
    return alsapcm_error(alsapcm,"","Rejecting buffer size %d, limit %d..%d",alsapcm->hwbufframec,ALSAPCM_BUF_MIN,ALSAPCM_BUF_MAX);
  }
  
  alsapcm->bufa=(alsapcm->hwbufframec*alsapcm->chanc)>>1;
  if (alsapcm->buf) free(alsapcm->buf);
  if (!(alsapcm->buf=malloc(alsapcm->bufa<<1))) return -1;
  
  /* Now set some driver software parameters.
   * The main thing is we want avail_min to be half of the hardware buffer size.
   * We will send half-hardware-buffers at a time, and this arrangement should click nicely, let us sleep as much as possible.
   * (in limited experimentation so far, I have found this to be so, and it makes a big impact on overall performance).
   * I've heard that swparams can be used to automatically recover from xrun, but haven't seen that work yet. Not trying here.
   */
  struct snd_pcm_sw_params swparams={
    .tstamp_mode=SNDRV_PCM_TSTAMP_NONE,
    .sleep_min=0,
    .avail_min=alsapcm->hwbufframec>>1,
    .xfer_align=1,
    .start_threshold=0,
    .stop_threshold=alsapcm->hwbufframec,
    .silence_threshold=alsapcm->hwbufframec,
    .silence_size=0,
    .boundary=alsapcm->hwbufframec,
    // these two fields aren't in my pi's alsa headers. TODO how to detect?
    //.proto=alsapcm->protocol_version,
    //.tstamp_type=SNDRV_PCM_TSTAMP_NONE,
  };
  if (ioctl(alsapcm->fd,SNDRV_PCM_IOCTL_SW_PARAMS,&swparams)<0) {
    return alsapcm_error(alsapcm,"SNDRV_PCM_IOCTL_SW_PARAMS",0);
  }
  
  /* And finally, reset the driver and confirm that it enters PREPARED state.
   */
  if (ioctl(alsapcm->fd,SNDRV_PCM_IOCTL_RESET)<0) {
    return alsapcm_error(alsapcm,"SNDRV_PCM_IOCTL_RESET",0);
  }
  struct snd_pcm_status status={0};
  if (ioctl(alsapcm->fd,SNDRV_PCM_IOCTL_STATUS,&status)<0) {
    return alsapcm_error(alsapcm,"SNDRV_PCM_IOCTL_STATUS",0);
  }
  if (status.state!=SNDRV_PCM_STATE_PREPARED) {
    return alsapcm_error(alsapcm,"","State not PREPARED after setup. state=%d",status.state);
  }
  
  return 0;
}

/* Init: Open and prepare the device.
 */
 
static int alsapcm_init_alsa(
  struct alsapcm *alsapcm,
  const struct alsapcm_setup *setup
) {
  if (alsapcm_select_device_path(alsapcm,setup)<0) return -1;
  if (alsapcm_open_device(alsapcm)<0) return -1;
  if (alsapcm_configure_device(alsapcm,setup)<0) return -1;
  return 0;
}

/* Prepare mutex and thread.
 */
 
static int alsapcm_init_thread(struct alsapcm *alsapcm) {
  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&alsapcm->iomtx,&mattr)) return -1;
  pthread_mutexattr_destroy(&mattr);
  if (pthread_create(&alsapcm->iothd,0,alsapcm_iothd,alsapcm)) return -1;
  return 0;
}

/* New.
 */

struct alsapcm *alsapcm_new(
  const struct alsapcm_delegate *delegate,
  const struct alsapcm_setup *setup
) {
  if (!delegate||!delegate->pcm_out) return 0;
  struct alsapcm *alsapcm=calloc(1,sizeof(struct alsapcm));
  if (!alsapcm) return 0;
  
  alsapcm->fd=-1;
  alsapcm->delegate=*delegate;
  
  if (alsapcm_init_alsa(alsapcm,setup)<0) {
    alsapcm_del(alsapcm);
    return 0;
  }
  
  if (alsapcm_init_thread(alsapcm)<0) {
    alsapcm_del(alsapcm);
    return 0;
  }
  
  return alsapcm;
}

/* Trivial accessors.
 */
 
void *alsapcm_get_userdata(const struct alsapcm *alsapcm) {
  if (!alsapcm) return 0;
  return alsapcm->delegate.userdata;
}

int alsapcm_get_rate(const struct alsapcm *alsapcm) {
  if (!alsapcm) return 0;
  return alsapcm->rate;
}

int alsapcm_get_chanc(const struct alsapcm *alsapcm) {
  if (!alsapcm) return 0;
  return alsapcm->chanc;
}

int alsapcm_get_running(const struct alsapcm *alsapcm) {
  if (!alsapcm) return 0;
  return alsapcm->running;
}

void alsapcm_set_running(struct alsapcm *alsapcm,int run) {
  if (!alsapcm) return;
  alsapcm->running=run?1:0;
}

/* Update.
 */
 
int alsapcm_update(struct alsapcm *alsapcm) {
  if (!alsapcm) return 0;
  if (alsapcm->ioerror) return -1;
  return 0;
}

/* Lock.
 */
 
int alsapcm_lock(struct alsapcm *alsapcm) {
  if (!alsapcm) return -1;
  if (pthread_mutex_lock(&alsapcm->iomtx)) return -1;
  return 0;
}

void alsapcm_unlock(struct alsapcm *alsapcm) {
  if (!alsapcm) return;
  pthread_mutex_unlock(&alsapcm->iomtx);
}

/* Log errors.
 */
 
int alsapcm_error(struct alsapcm *alsapcm,const char *context,const char *fmt,...) {
  const int log_enabled=1;
  if (log_enabled) {
    if (!context) context="";
    fprintf(stderr,"%s:%s: ",alsapcm->device?alsapcm->device:"alsapcm",context);
    if (fmt&&fmt[0]) {
      va_list vargs;
      va_start(vargs,fmt);
      vfprintf(stderr,fmt,vargs);
      fprintf(stderr,"\n");
    } else {
      fprintf(stderr,"%m\n");
    }
  }
  return -1;
}
