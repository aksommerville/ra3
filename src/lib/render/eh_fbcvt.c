#include "eh_render_internal.h"

/* From I1.
 */
 
void eh_fbcvt_i1(uint8_t *dst,const uint8_t *src,struct eh_render *render) {
  int dststride=eh.delegate.video_width*3;
  int srcstride=(eh.delegate.video_width+7)>>3;
  int yi=eh.delegate.video_height;
  for (;yi-->0;dst+=dststride,src+=srcstride) {
    uint8_t *dstp=dst;
    const uint8_t *srcp=src;
    int xi=eh.delegate.video_width;
    uint8_t srcmask=0x80;
    for (;xi-->0;dstp+=3) {
      const uint8_t *ct=render->ctab;
      if ((*srcp)&srcmask) ct+=3;
      dstp[0]=ct[0];
      dstp[1]=ct[1];
      dstp[2]=ct[2];
      if (srcmask==0x01) { srcmask=0x80; srcp++; }
      else srcmask>>=1;
    }
  }
}

/* From I2.
 */

void eh_fbcvt_i2(uint8_t *dst,const uint8_t *src,struct eh_render *render) {
  int dststride=eh.delegate.video_width*3;
  int srcstride=(eh.delegate.video_width+3)>>2;
  int yi=eh.delegate.video_height;
  for (;yi-->0;dst+=dststride,src+=srcstride) {
    uint8_t *dstp=dst;
    const uint8_t *srcp=src;
    int xi=eh.delegate.video_width;
    int srcshift=6;
    for (;xi-->0;dstp+=3) {
      const uint8_t *ct=render->ctab;
      ct+=(((*srcp)>>srcshift)&3)*3;
      dstp[0]=ct[0];
      dstp[1]=ct[1];
      dstp[2]=ct[2];
      if (srcshift) srcshift>>=2;
      else { srcshift=6; srcp++; }
    }
  }
}

/* From I4.
 */
 
void eh_fbcvt_i4(uint8_t *dst,const uint8_t *src,struct eh_render *render) {
  int dststride=eh.delegate.video_width*3;
  int srcstride=(eh.delegate.video_width+1)>>2;
  int yi=eh.delegate.video_height;
  for (;yi-->0;dst+=dststride,src+=srcstride) {
    uint8_t *dstp=dst;
    const uint8_t *srcp=src;
    int xi=eh.delegate.video_width;
    int srcshift=4;
    for (;xi-->0;dstp+=3) {
      const uint8_t *ct=render->ctab+(*srcp)*3;
      ct+=(((*srcp)>>srcshift)&0x0f)*3;
      dstp[0]=ct[0];
      dstp[1]=ct[1];
      dstp[2]=ct[2];
      if (srcshift) srcshift=0;
      else { srcshift=4; srcp++; }
    }
  }
}

/* From I8.
 */
 
void eh_fbcvt_i8(uint8_t *dst,const uint8_t *src,struct eh_render *render) {
  int dststride=eh.delegate.video_width*3;
  int srcstride=eh.delegate.video_width;
  int yi=eh.delegate.video_height;
  for (;yi-->0;dst+=dststride,src+=srcstride) {
    uint8_t *dstp=dst;
    const uint8_t *srcp=src;
    int xi=eh.delegate.video_width;
    for (;xi-->0;dstp+=3,srcp++) {
      const uint8_t *ct=render->ctab+(*srcp)*3;
      dstp[0]=ct[0];
      dstp[1]=ct[1];
      dstp[2]=ct[2];
    }
  }
}

/* From 16-bit RGB.
 * TODO Calling three virtual functions per pixel is batshit crazy. Can we at least pick off realistic cases and hard-code those?
 * Defer until we onboard an emulator with 16-bit pixels. SNES? That one at least should get special treatment.
 */
 
void eh_fbcvt_rgb16(uint8_t *dst,const uint8_t *src,struct eh_render *render) {
  int dststride=eh.delegate.video_width*3;
  int srcstride=eh.delegate.video_width<<1;
  int yi=eh.delegate.video_height;
  for (;yi-->0;dst+=dststride,src+=srcstride) {
    uint8_t *dstp=dst;
    const uint16_t *srcp=(uint16_t*)src;
    int xi=eh.delegate.video_width;
    for (;xi-->0;dstp+=3,srcp++) {
      dstp[0]=render->chr_r(*srcp);
      dstp[1]=render->chr_g(*srcp);
      dstp[2]=render->chr_b(*srcp);
    }
  }
}

void eh_fbcvt_rgb16swap(uint8_t *dst,const uint8_t *src,struct eh_render *render) {
  int dststride=eh.delegate.video_width*3;
  int srcstride=eh.delegate.video_width<<1;
  int yi=eh.delegate.video_height;
  for (;yi-->0;dst+=dststride,src+=srcstride) {
    uint8_t *dstp=dst;
    const uint16_t *srcp=(uint16_t*)src;
    int xi=eh.delegate.video_width;
    for (;xi-->0;dstp+=3,srcp++) {
      uint16_t pixel=((*srcp)>>8)|((*srcp)<<8);
      dstp[0]=render->chr_r(pixel);
      dstp[1]=render->chr_g(pixel);
      dstp[2]=render->chr_b(pixel);
    }
  }
}

/* From 24-bit RGB.
 * (NB If input is in the same arrangement as output, we've short-circuited elsewhere and won't call this).
 */
 
void eh_fbcvt_rgb24(uint8_t *dst,const uint8_t *src,struct eh_render *render) {
  int dststride=eh.delegate.video_width*3;
  int srcstride=eh.delegate.video_width*3;
  int yi=eh.delegate.video_height;
  
  #define _(rp,gp,bp) { \
    for (;yi-->0;dst+=dststride,src+=srcstride) { \
      uint8_t *dstp=dst; \
      const uint8_t *srcp=src; \
      int xi=eh.delegate.video_width; \
      for (;xi-->0;dstp+=3,srcp+=3) { \
        dstp[0]=srcp[rp]; \
        dstp[1]=srcp[gp]; \
        dstp[2]=srcp[bp]; \
      } \
    } \
  } break;
  
  switch (render->rshift) {
    case EH_RGB24_RGB: _(0,1,2)
    case EH_RGB24_RBG: _(0,2,1)
    case EH_RGB24_GBR: _(1,2,0)
    case EH_RGB24_GRB: _(1,0,2)
    case EH_RGB24_BGR: _(2,1,0)
    case EH_RGB24_BRG: _(2,0,1)
  }
  #undef _
}

/* From 32-bit RGB.
 */
 
void eh_fbcvt_rgb32(uint8_t *dst,const uint8_t *src,struct eh_render *render) {
  int dststride=eh.delegate.video_width*3;
  int srcstride=eh.delegate.video_width<<2;
  int yi=eh.delegate.video_height;
  for (;yi-->0;dst+=dststride,src+=srcstride) {
    uint8_t *dstp=dst;
    const uint32_t *srcp=(uint32_t*)src;
    int xi=eh.delegate.video_width;
    for (;xi-->0;dstp+=3,srcp++) {
      dstp[0]=(*srcp)>>render->rshift;
      dstp[1]=(*srcp)>>render->gshift;
      dstp[2]=(*srcp)>>render->bshift;
    }
  }
}

/* 16-bit channel readers.
 */
 
static uint8_t eh_render_chr16_top5(uint16_t v) {
  return ((v>>8)&0xf8)|(v>>13);
}

static uint8_t eh_render_chr16_14x5(uint16_t v) {
  return ((v>>7)&0xf8)|((v>>12)&0x07);
}

static uint8_t eh_render_chr16_mid6(uint16_t v) {
  return ((v>>3)&0xfc)|((v>>9)&0x03);
}

static uint8_t eh_render_chr16_mid5(uint16_t v) {
  return ((v>>2)&0xf8)|((v>>7)&0x07);
}

static uint8_t eh_render_chr16_low5(uint16_t v) {
  return (v<<3)|((v>>2)&0x1f);
}

static uint8_t eh_render_chr16_default(uint16_t v) {
  return v;
}

void *eh_render_chr16(uint16_t mask) {
  switch (mask) {
    case 0xf800: return eh_render_chr16_top5;
    case 0x7c00: return eh_render_chr16_14x5;
    case 0x07e0: return eh_render_chr16_mid6;
    case 0x03e0: return eh_render_chr16_mid5;
    case 0x001f: return eh_render_chr16_low5;
  }
  return eh_render_chr16_default;
}
