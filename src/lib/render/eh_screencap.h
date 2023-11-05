/* eh_screencap.h
 */
 
#ifndef EH_SCREENCAP_H
#define EH_SCREENCAP_H

/* Generate a PNG file from this framebuffer or the current OpenGL output,
 * and queue it for delivery out the WebSocket.
 * If the global WebSocket is closed, we quickly noop and report failure.
 */
int eh_screencap_send_from_fb(const void *fb);
int eh_screencap_send_from_opengl();

/* Generate a PNG file from a framebuffer, no globals.
 * (format) should come straight off the client delegate, except (ctab), from the renderer.
 */
struct eh_screencap_format {
  int w,h;
  int format;
  uint32_t rmask,gmask,bmask;
  const uint8_t *ctab; // 768 bytes if not null (even for <8-bit pixels)
};
int eh_screencap_from_fb(void *dstpp,const void *fb,const struct eh_screencap_format *format);

/* Generate a PNG file from the OpenGL context.
 * These will always be RGB.
 */
int eh_screencap_from_opengl(void *dstpp,int w,int h);

#endif
