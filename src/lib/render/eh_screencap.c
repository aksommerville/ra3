#include "eh_render_internal.h"
#include "eh_screencap.h"
#include "opt/png/png.h"

/* High-level conveniences using global state.
 */
 
int eh_screencap_send_from_fb(const void *fb) {
  if (!fb) return -1;
  if (!fakews_is_connected(eh.fakews)) return -1;
  struct eh_screencap_format format={
    .w=eh.delegate.video_width,
    .h=eh.delegate.video_height,
    .format=eh.delegate.video_format,
    .rmask=eh.delegate.rmask,
    .gmask=eh.delegate.gmask,
    .bmask=eh.delegate.bmask,
    .ctab=eh.render->ctab,
  };
  void *serial=0;
  int serialc=eh_screencap_from_fb(&serial,fb,&format);
  if (serialc<0) return serialc;
  int err=fakews_send(eh.fakews,2,serial,serialc);
  free(serial);
  return err;
}

int eh_screencap_send_from_opengl() {
  if (!fakews_is_connected(eh.fakews)) return -1;
  void *serial=0;
  int serialc=eh_screencap_from_opengl(&serial,eh.video->w,eh.video->h);
  if (serialc<0) return serialc;
  int err=fakews_send(eh.fakews,2,serial,serialc);
  free(serial);
  return err;
}

/* Stride from format.
 */
 
static int eh_screencap_calculate_stride(const struct eh_screencap_format *format) {
  switch (format->format) {
    case EH_VIDEO_FORMAT_I1:
      return (format->w+7)>>3;
    case EH_VIDEO_FORMAT_I2:
      return (format->w+3)>>2;
    case EH_VIDEO_FORMAT_I4:
      return (format->w+1)>>1;
    case EH_VIDEO_FORMAT_I8:
      return format->w;
    case EH_VIDEO_FORMAT_RGB16:
    case EH_VIDEO_FORMAT_RGB16SWAP:
      return format->w<<1;
    case EH_VIDEO_FORMAT_RGB24:
      return format->w*3;
    case EH_VIDEO_FORMAT_RGB32:
      return format->w<<2;
  }
  return 0;
}

/* Check whether this format is already PNG-compatible and return nonzero if it is, and populate (depth,colortype).
 * In false cases, also report (depth,colortype) with the preferred format to convert to.
 */
 
static int eh_screencap_format_is_pngable(uint8_t *depth,uint8_t *colortype,const struct eh_screencap_format *format) {
  switch (format->format) {
    case EH_VIDEO_FORMAT_I1: *depth=1; *colortype=3; return 1;
    case EH_VIDEO_FORMAT_I2: *depth=2; *colortype=3; return 1;
    case EH_VIDEO_FORMAT_I4: *depth=4; *colortype=3; return 1;
    case EH_VIDEO_FORMAT_I8: *depth=8; *colortype=3; return 1;
    case EH_VIDEO_FORMAT_RGB16: *depth=8; *colortype=2; return 0;
    case EH_VIDEO_FORMAT_RGB24: {
        *depth=8;
        *colortype=2;
        if ((format->rmask==0xff0000)&&(format->gmask==0x00ff00)&&(format->bmask==0x0000ff)) return 1;
        return 0;
      }
    case EH_VIDEO_FORMAT_RGB32: *depth=8; *colortype=2; return 0;
    case EH_VIDEO_FORMAT_RGB16SWAP: *depth=8; *colortype=2; return 0;
    default: *depth=8; *colortype=2; return 0;
  }
}

/* If a color table is needed and available, add it to the PNG image object.
 * Otherwise, if it declares colortype index, change that to luma.
 */
 
static int eh_screencap_add_ctab_if_needed(struct png_image *image,const struct eh_screencap_format *format) {
  if (image->colortype!=3) return 0;
  if (format->ctab) {
    int len=(1<<image->depth)*3;
    if ((len<6)||(len>768)) return -1;
    if (png_image_add_chunk(image,PNG_CHUNKID_PLTE,format->ctab,len)<0) return -1;
  } else {
    image->colortype=0;
  }
  return 0;
}

/* Generate PNG from framebuffer, the main event.
 */
 
static struct png_image *eh_screencap_convert_pngable(const void *fb,const struct eh_screencap_format *format) {

  /* Select PNG pixel format. If it matches the framebuffer exactly, awesome, no need to copy pixels.
   * I reckon exact matches should be pretty likely.
   * We enforce rules identical to PNG's: 1-byte alignment in all cases, and layout is always big-endian LRTB.
   */
  uint8_t depth=0,colortype=0;
  if (eh_screencap_format_is_pngable(&depth,&colortype,format)) {
    struct png_image *image=png_image_new(0,0,0,0);
    if (!image) return 0;
    image->depth=depth;
    image->colortype=colortype;
    image->w=format->w;
    image->h=format->h;
    image->stride=eh_screencap_calculate_stride(format);
    image->pixels=(void*)fb;
    image->ownpixels=0;
    if (eh_screencap_add_ctab_if_needed(image,format)<0) {
      png_image_del(image);
      return 0;
    }
    return image;
  }
  
  /* If the global renderer is set up to produce canonical RGB, we can borrow that.
   * This is quite likely.
   */
  if (eh.render->fbcvt&&(eh.render->fb_gl_format==GL_RGB)&&(eh.render->fb_gl_type==GL_UNSIGNED_BYTE)) {
    struct png_image *image=png_image_new(format->w,format->h,8,2);
    if (!image) return 0;
    eh.render->fbcvt(image->pixels,fb,eh.render);
    return image;
  }
  
  /* Any other situation is unlikely, I think bordering on impossible.
   * If this comes up, we'll need some painful generic image conversion.
   */
  fprintf(stderr,"%s:%d:%s: No easy framebuffer=>PNG conversion. Implement generic conversion.\n",__FILE__,__LINE__,__func__);
  return 0;
}

/* Generate PNG from framebuffer, outer layer.
 */

int eh_screencap_from_fb(void *dstpp,const void *fb,const struct eh_screencap_format *format) {
  if (!dstpp||!fb||!format) return -1;
  struct png_image *image=eh_screencap_convert_pngable(fb,format);
  if (!image) return -1;
  int dstc=png_encode(dstpp,image);
  png_image_del(image);
  return dstc;
}

/* Generate PNG from OpenGL.
 * This only manages the OpenGL bit, then defers to eh_screencap_from_fb.
 */

int eh_screencap_from_opengl(void *dstpp,int w,int h) {
  if ((w<1)||(h<1)) return -1;
  // I don't currently have a client to test this with, and not confident to do it blindly.
  fprintf(stderr,"TODO %s:%d:%s Pull framebuffer off OpenGL and deliver to eh_screencap_from_fb.\n",__FILE__,__LINE__,__func__);
  return -1;
}
