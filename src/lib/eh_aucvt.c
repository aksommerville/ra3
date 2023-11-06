#include "eh_internal.h"
#include "eh_aucvt.h"
#include "eh_driver.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

static int eh_aucvt_output_verbatim(void *dst,int samplec,struct eh_aucvt *aucvt);
static int eh_aucvt_output_formatonly(void *dst,int samplec,struct eh_aucvt *aucvt);
static int eh_aucvt_output_fceu(void *dst,int samplec,struct eh_aucvt *aucvt);

/* Cleanup.
 */
 
void eh_aucvt_cleanup(struct eh_aucvt *aucvt) {
  aucvt->ready=0;
  if (aucvt->buf) free(aucvt->buf);
  memset(aucvt,0,sizeof(struct eh_aucvt));
}

/* Generic stream read/write.
 */
 
static struct eh_auframe eh_aurd_s16_1(const void *src) {
  float v=(*(int16_t*)src)/32768.0f;
  return (struct eh_auframe){v,v};
}
 
static struct eh_auframe eh_aurd_s32_1(const void *src) {
  float v=(*(int32_t*)src)/2147483648.0f;
  return (struct eh_auframe){v,v};
}
 
static struct eh_auframe eh_aurd_float_1(const void *src) {
  float v=*(float*)src;
  return (struct eh_auframe){v,v};
}
 
static struct eh_auframe eh_aurd_s32_lo16_1(const void *src) {
  float v=(*(int32_t*)src)/32768.0f;
  return (struct eh_auframe){v,v};
}
 
static struct eh_auframe eh_aurd_s16_2(const void *src) {
  float l=((int16_t*)src)[0]/32768.0f;
  float r=((int16_t*)src)[1]/32768.0f;
  return (struct eh_auframe){l,r};
}
 
static struct eh_auframe eh_aurd_s32_2(const void *src) {
  float l=((int32_t*)src)[0]/2147483648.0f;
  float r=((int32_t*)src)[1]/2147483648.0f;
  return (struct eh_auframe){l,r};
}
 
static struct eh_auframe eh_aurd_float_2(const void *src) {
  float l=((float*)src)[0];
  float r=((float*)src)[1];
  return (struct eh_auframe){l,r};
}
 
static struct eh_auframe eh_aurd_s32_lo16_2(const void *src) {
  float l=((int32_t*)src)[0]/32768.0f;
  float r=((int32_t*)src)[1]/32768.0f;
  return (struct eh_auframe){l,r};
}

static void eh_auwr_s16_1(void *dst,struct eh_auframe src) {
  *(int16_t*)dst=(src.l*32767.0f);
}

static void eh_auwr_s32_1(void *dst,struct eh_auframe src) {
  *(int32_t*)dst=(src.l*2147483647.0f);
}

static void eh_auwr_float_1(void *dst,struct eh_auframe src) {
  *(float*)dst=src.l;
}

static void eh_auwr_s32_lo16_1(void *dst,struct eh_auframe src) {
  *(int32_t*)dst=(src.l*32767.0f);
}

static void eh_auwr_s16_2(void *dst,struct eh_auframe src) {
  ((int16_t*)dst)[0]=(src.l*32767.0f);
  ((int16_t*)dst)[1]=(src.r*32767.0f);
}

static void eh_auwr_s32_2(void *dst,struct eh_auframe src) {
  ((int32_t*)dst)[0]=(src.l*2147483647.0f);
  ((int32_t*)dst)[1]=(src.r*2147483647.0f);
}

static void eh_auwr_float_2(void *dst,struct eh_auframe src) {
  ((float*)dst)[0]=src.l;
  ((float*)dst)[1]=src.r;
}

static void eh_auwr_s32_lo16_2(void *dst,struct eh_auframe src) {
  ((int32_t*)dst)[0]=(src.l*32767.0f);
  ((int32_t*)dst)[1]=(src.r*32767.0f);
}

static int eh_audio_sample_size(int format) {
  switch (format) {
    case EH_AUDIO_FORMAT_S16N:
    case EH_AUDIO_FORMAT_S16LE:
    case EH_AUDIO_FORMAT_S16BE:
      return 2;
    case EH_AUDIO_FORMAT_S32N:
    case EH_AUDIO_FORMAT_S32LE:
    case EH_AUDIO_FORMAT_S32BE:
    case EH_AUDIO_FORMAT_F32N:
    case EH_AUDIO_FORMAT_S32N_LO16:
      return 4;
  }
  return 0;
}
 
static eh_auframe_read_fn eh_aucvt_get_frame_reader(int fmt,int chanc) {
  switch (chanc) {
    case 1: switch (fmt) {
        case EH_AUDIO_FORMAT_S16N: return eh_aurd_s16_1;
        case EH_AUDIO_FORMAT_S32N: return eh_aurd_s32_1;
        case EH_AUDIO_FORMAT_F32N: return eh_aurd_float_1;
        case EH_AUDIO_FORMAT_S32N_LO16: return eh_aurd_s32_lo16_1;
        //TODO byte-order-specific formats
      } break;
    case 2: switch (fmt) {
        case EH_AUDIO_FORMAT_S16N: return eh_aurd_s16_2;
        case EH_AUDIO_FORMAT_S32N: return eh_aurd_s32_2;
        case EH_AUDIO_FORMAT_F32N: return eh_aurd_float_2;
        case EH_AUDIO_FORMAT_S32N_LO16: return eh_aurd_s32_lo16_2;
      } break;
  }
  return 0;
}

static eh_auframe_write_fn eh_aucvt_get_frame_writer(int fmt,int chanc) {
  switch (chanc) {
    case 1: switch (fmt) {
        case EH_AUDIO_FORMAT_S16N: return eh_auwr_s16_1;
        case EH_AUDIO_FORMAT_S32N: return eh_auwr_s32_1;
        case EH_AUDIO_FORMAT_F32N: return eh_auwr_float_1;
        case EH_AUDIO_FORMAT_S32N_LO16: return eh_auwr_s32_lo16_1;
      } break;
    case 2: switch (fmt) {
        case EH_AUDIO_FORMAT_S16N: return eh_auwr_s16_2;
        case EH_AUDIO_FORMAT_S32N: return eh_auwr_s32_2;
        case EH_AUDIO_FORMAT_F32N: return eh_auwr_float_2;
        case EH_AUDIO_FORMAT_S32N_LO16: return eh_auwr_s32_lo16_2;
      } break;
  }
  return 0;
}

/* Look for simple cases that we can prepare a better conversion implementation than the default.
 */
 
static void eh_aucvt_check_optimizations(struct eh_aucvt *aucvt) {

  if (
    (aucvt->dstrate==aucvt->srcrate)&&
    (aucvt->dstchanc==aucvt->srcchanc)&&
    (aucvt->dstfmt==aucvt->srcfmt)
  ) {
    fprintf(stderr,"aucvt: Verbatim output.\n");
    aucvt->verbatim=1;
    aucvt->output=eh_aucvt_output_verbatim;
    return;
  }
  
  if (aucvt->srcrate==aucvt->dstrate) {
    fprintf(stderr,"aucvt: Same rate, convert format only.\n");
    aucvt->output=eh_aucvt_output_formatonly;
    return;
  }
  
  if (
    (aucvt->srcrate<<1==aucvt->dstrate)&&
    (aucvt->srcchanc==1)&&
    (aucvt->dstchanc==2)&&
    (aucvt->srcfmt==EH_AUDIO_FORMAT_S32N_LO16)&&
    (aucvt->dstfmt==EH_AUDIO_FORMAT_S16N)
  ) {
    fprintf(stderr,"aucvt: FCEU quick-up possible\n");
    aucvt->output=eh_aucvt_output_fceu;
    return;
  }
  
  fprintf(stderr,"aucvt: Will use generic conversion.\n");
}

/* Init.
 */

int eh_aucvt_init(
  struct eh_aucvt *aucvt,
  int dstrate,int dstchanc,int dstfmt,
  int srcrate,int srcchanc,int srcfmt
) {
  memset(aucvt,0,sizeof(struct eh_aucvt));
  aucvt->dstrate=dstrate;
  aucvt->dstchanc=dstchanc;
  aucvt->dstfmt=dstfmt;
  aucvt->srcrate=srcrate;
  aucvt->srcchanc=srcchanc;
  aucvt->srcfmt=srcfmt;
  
  if (!(aucvt->dstsamplesize=eh_audio_sample_size(aucvt->dstfmt))) return -1;
  if (!(aucvt->srcsamplesize=eh_audio_sample_size(aucvt->srcfmt))) return -1;
  aucvt->dstframesize=aucvt->dstsamplesize*aucvt->dstchanc;
  aucvt->srcframesize=aucvt->srcsamplesize*aucvt->srcchanc;
  
  // We only need generic accessors if converting, but no harm in acquiring them anyway.
  if (!(aucvt->rdframe=eh_aucvt_get_frame_reader(aucvt->srcfmt,aucvt->srcchanc))) return -1;
  if (!(aucvt->wrframe=eh_aucvt_get_frame_writer(aucvt->dstfmt,aucvt->dstchanc))) return -1;
  
  aucvt->bufa=1024; //TODO configurable?
  if (!(aucvt->buf=calloc(aucvt->bufa,aucvt->srcsamplesize))) {
    aucvt->bufa=0;
    return -1;
  }
  
  aucvt->fbufp=0.0;
  aucvt->fbufpd=(double)aucvt->srcrate/(double)aucvt->dstrate;
  eh_aucvt_check_optimizations(aucvt);
  
  aucvt->ready=1;
  return 0;
}

/* Issue a warning if we have to convert audio.
 */
 
void eh_aucvt_warn_if_converting(const struct eh_aucvt *aucvt) {
  if (aucvt->verbatim) {
  } else if (aucvt->srcrate!=aucvt->dstrate) {
    fprintf(stderr,
      "Audio rate mismatch. We will convert and do a bad job of it. in=(%d ch @ %d Hz of %d) out=(%d ch @ %d Hz of %d)\n",
      aucvt->srcchanc,aucvt->srcrate,aucvt->srcfmt,
      aucvt->dstchanc,aucvt->dstrate,aucvt->dstfmt
    );
  } else {
    fprintf(stderr,
      "Audio channel or format mismatch. Like rate %d Hz. in=(%d ch of %d) out=(%d ch of %d)\n",
      aucvt->srcrate,
      aucvt->srcchanc,aucvt->srcfmt,
      aucvt->dstchanc,aucvt->dstfmt
    );
  }
}
      

/* Receive input.
 */
 
int eh_aucvt_input(struct eh_aucvt *aucvt,const void *src,int samplec) {
  
  // Eliminate all audio while fast-forwarding.
  if (eh.fastfwd) return samplec;
  
  int okc=aucvt->bufa-aucvt->bufc;
  if (okc>=samplec) okc=samplec;
  else aucvt->overframec=samplec-okc;
  if (okc<1) return 0;
  
  int headc=0,tailc=0;
  int dstp=aucvt->bufp+aucvt->bufc;
  if (dstp>=aucvt->bufa) {
    dstp-=aucvt->bufa;
    headc=aucvt->bufp-dstp;
    if (headc>okc) headc=okc;
    else tailc=okc-headc;
  } else {
    headc=aucvt->bufa-dstp;
    if (headc>okc) headc=okc;
    else tailc=okc-headc;
  }
  
  dstp*=aucvt->srcsamplesize;
  memcpy((char*)aucvt->buf+dstp,src,headc*aucvt->srcsamplesize);
  if (tailc) {
    src=(char*)src+headc*aucvt->srcsamplesize;
    memcpy(aucvt->buf,src,tailc*aucvt->srcsamplesize);
  }
  aucvt->bufc+=okc;
  
  return okc;
}

/* verbatim: Ideal output case.
 */
 
static int eh_aucvt_output_verbatim(void *dst,int samplec,struct eh_aucvt *aucvt) {
  while (samplec>0) {
    int cpc=aucvt->bufa-aucvt->bufp;
    if (cpc>samplec) cpc=samplec;
    memcpy(dst,(char*)aucvt->buf+aucvt->bufp*aucvt->srcsamplesize,cpc*aucvt->srcsamplesize);
    if ((aucvt->bufp+=cpc)>=aucvt->bufa) aucvt->bufp=0;
    if ((aucvt->bufc-=cpc)<0) aucvt->bufc=0;
    dst=(char*)dst+cpc*aucvt->dstsamplesize;
    samplec-=cpc;
  }
  return 0;
}

/* formatonly: Rate not changing.
 */
 
static int eh_aucvt_output_formatonly(void *dst,int samplec,struct eh_aucvt *aucvt) {
  char *srcp=(char*)aucvt->buf+aucvt->bufp*aucvt->srcsamplesize;
  int dstframec=samplec/aucvt->dstchanc;
  for (;dstframec-->0;dst=(char*)dst+aucvt->dstframesize) {
  
    struct eh_auframe frame=aucvt->rdframe(srcp);
    srcp+=aucvt->srcframesize;
    aucvt->bufp+=aucvt->srcchanc;
    if (aucvt->bufp>=aucvt->bufa) {
      aucvt->bufp=0;
      srcp=aucvt->buf;
    }
    if (aucvt->bufc>0) aucvt->bufc--; // but we keep reading, even if zero
    else aucvt->badframec++;
    
    aucvt->wrframe(dst,frame);
  }
  return 0;
}

/* fceu: Rate and channel count both double. Output is S16N, and input S32N_LO16.
 */
 
static int eh_aucvt_output_fceu(void *dst,int samplec,struct eh_aucvt *aucvt) {
  int srcsamplec=samplec>>2;
  if (srcsamplec<1) return 0;
  if (srcsamplec>aucvt->bufc) { // if we overrun input, just keep going, but make a note of it.
    aucvt->badframec+=srcsamplec-aucvt->bufc;
    aucvt->bufc=0;
  } else {
    aucvt->bufc-=srcsamplec;
  }
  int32_t *srcp=(int32_t*)aucvt->buf+aucvt->bufp;
  int16_t *dstp=dst;
  for (;srcsamplec-->0;dstp+=4) {
    int16_t sample=*srcp++;
    dstp[0]=dstp[1]=dstp[2]=dstp[3]=sample;
    if ((aucvt->bufp+=1)>=aucvt->bufa) {
      aucvt->bufp=0;
      srcp=aucvt->buf;
    }
  }
  switch (samplec&3) {
    case 1: dstp[0]=dstp[-1]; break;
    case 2: dstp[0]=dstp[1]=dstp[-1]; break;
    case 3: dstp[0]=dstp[1]=dstp[2]=dstp[-1]; break;
  }
  return 0;
}

/* Produce output.
 */
 
int eh_aucvt_output(void *dst,int samplec,struct eh_aucvt *aucvt) {
  if (!aucvt->ready) return 0;
  
  // While fast-forwarding, there's no sense even trying to keep up with audio.
  if (eh.fastfwd) {
    memset(dst,0,samplec*aucvt->dstsamplesize);
    return 0;
  }
  
  if (aucvt->output) return aucvt->output(dst,samplec,aucvt);
  
  // Different rates, need to resample.
  // I guess the nicest way would be to take the DFT of the input, then IFFT it back to the output rate.
  // Too complicated of course. We're going to do a straight nearest-neighbor resample.
  // (fbufp) is the source position in *frames*.
  double bufframec=aucvt->bufa/aucvt->srcchanc;
  int framemask=(1<<(aucvt->srcframesize))-1;
  int dstframec=samplec/aucvt->dstchanc;
  for (;dstframec-->0;dst=(char*)dst+aucvt->dstframesize) {
    int ibufp=(int)(aucvt->fbufp)*aucvt->srcchanc; // samples
    while (aucvt->bufp!=ibufp) {
      if ((aucvt->bufp+=aucvt->srcchanc)>=aucvt->bufa) aucvt->bufp=0;
      if ((aucvt->bufc-=aucvt->srcchanc)<0) aucvt->bufc=0;
    }
    struct eh_auframe frame=aucvt->rdframe((char*)aucvt->buf+ibufp*aucvt->srcsamplesize);
    aucvt->fbufp+=aucvt->fbufpd;
    if (aucvt->fbufp>=bufframec) aucvt->fbufp-=bufframec;
    aucvt->wrframe(dst,frame);
  }
    
  return 0;
}
