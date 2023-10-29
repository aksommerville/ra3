#ifndef EH_RENDER_INTERNAL_H
#define EH_RENDER_INTERNAL_H

#include "eh_render.h"
#include "../eh_internal.h"

#if USE_macwm
  #define GL_GLEXT_PROTOTYPES 1
  #include <OpenGL/gl.h>
  #define GL_PROGRAM_POINT_SIZE 0x8642
  #define glDeleteFramebuffers glDeleteFramebuffersEXT
  #define glGenFramebuffers glGenFramebuffersEXT
  #define glBindFramebuffer glBindFramebufferEXT
  #define glFramebufferTexture2D glFramebufferTexture2DEXT
  #define glCheckFramebufferStatus glCheckFramebufferStatusEXT
#else
  #define GL_GLEXT_PROTOTYPES 1
  #include <GL/gl.h>
  #include <GL/glext.h> // mswin needs this explicitly included; linux does not
#endif

/* In the special RGB24 case (if it's not in canonical order),
 * (rshift) is actually an enum for the channel orders.
 */
#define EH_RGB24_RGB 0 /* won't get used; this is canonical order */
#define EH_RGB24_RBG 1
#define EH_RGB24_GRB 2
#define EH_RGB24_GBR 3
#define EH_RGB24_BGR 4
#define EH_RGB24_BRG 5

struct eh_render {

  int dstr_dirty;
  int dstr_clear;
  GLfloat dstr_left,dstr_right,dstr_top,dstr_bottom;
  
  GLuint texid;
  uint8_t *fbrgb; // Null if we're not doing software framebuffer conversion. Otherwise 24-bit RGB.
  uint8_t ctab[768];
  void (*fbcvt)(uint8_t *dst,const uint8_t *src,struct eh_render *render); // always present if (fbrgb)
  uint8_t (*chr_r)(uint16_t pixel); // present for 16-bit RGB
  uint8_t (*chr_g)(uint16_t pixel); // present for 16-bit RGB
  uint8_t (*chr_b)(uint16_t pixel); // present for 16-bit RGB
  int rshift,gshift,bshift;
  int fb_gl_format,fb_gl_type;
  const void *srcfb;
  
  GLuint programid;
  
  int gx_in_progress;
};

/* Main entry point for framebuffer conversion and delivery.
 */
void eh_render_commit(struct eh_render *render);

void eh_fbcvt_i1(uint8_t *dst,const uint8_t *src,struct eh_render *render);
void eh_fbcvt_i2(uint8_t *dst,const uint8_t *src,struct eh_render *render);
void eh_fbcvt_i4(uint8_t *dst,const uint8_t *src,struct eh_render *render);
void eh_fbcvt_i8(uint8_t *dst,const uint8_t *src,struct eh_render *render);
void eh_fbcvt_rgb16(uint8_t *dst,const uint8_t *src,struct eh_render *render);
void eh_fbcvt_rgb24(uint8_t *dst,const uint8_t *src,struct eh_render *render);
void eh_fbcvt_rgb32(uint8_t *dst,const uint8_t *src,struct eh_render *render);
void eh_fbcvt_rgb16swap(uint8_t *dst,const uint8_t *src,struct eh_render *render);

void *eh_render_chr16(uint16_t mask); // => uint8_t(*)(uint16_t), channel reader from 16 bits

#endif
