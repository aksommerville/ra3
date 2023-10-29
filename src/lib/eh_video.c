#if 0 //XXX make a discrete object for renderer

#include "eh_internal.h"
#include <GL/gl.h> /* TODO Different location on MacOS */

/* Init.
 */
 
int eh_render_init() {

  eh.dstr_dirty=1;

  // Framebuffer texture.
  glGenTextures(1,&eh.texid_fb);
  if (!eh.texid_fb) {
    glGenTextures(1,&eh.texid_fb);
    return -1;
  }
  glBindTexture(GL_TEXTURE_2D,eh.texid_fb);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  //TODO allow GL_LINEAR for texid_fb if the user asks for it. (and change on the fly too)
  // Mind that if we're using a color table, it must be GL_NEAREST.
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  
  // Color table texture.
  glGenTextures(1,&eh.texid_ctab);
  glBindTexture(GL_TEXTURE_2D,eh.texid_ctab);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  eh.ctab_dirty=1;
  uint8_t *dst=eh.ctab;
  int i=0; for (;i<256;i++,dst+=3) dst[0]=dst[1]=dst[2]=i;
  
  // Shader.
  
  return 0;
}

/* Upload framebuffer.
 */
 
static inline int eh_upload_framebuffer(const void *fb) {

  if (eh.ctab_dirty) {
    eh.ctab_dirty=0;
    glBindTexture(GL_TEXTURE_2D,eh.texid_ctab);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,256,1,0,GL_RGB,GL_UNSIGNED_BYTE,eh.ctab);
  }
  
  glBindTexture(GL_TEXTURE_2D,eh.texid_fb);
  //TODO check format
  glTexImage2D(
    GL_TEXTURE_2D,0,GL_RGB,
    eh.delegate.video_width,eh.delegate.video_height,
    0,GL_LUMINANCE,GL_UNSIGNED_BYTE,fb
  );
  
  return 0;
}

/* Recalculate output bounds if needed.
 */
 
static inline void eh_require_dstr() {
  if (!eh.dstr_dirty) return;
  eh.dstr_dirty=0;
  int fbw=eh.delegate.video_width;
  int fbh=eh.delegate.video_height;
  int winw=eh.video->w;
  int winh=eh.video->h;
  
  if ((fbw<1)||(fbh<1)||(winw<1)||(winh<1)) {
    eh.dstr_left=-1.0f;
    eh.dstr_right=1.0f;
    eh.dstr_top=1.0f;
    eh.dstr_bottom=-1.0f;
    eh.dstr_clear=0;
    return;
  }
  
  int dstw,dsth;
  int wforh=(winh*fbw)/fbh;
  if (wforh<=winw) {
    dstw=wforh;
    dsth=winh;
  } else {
    dstw=winw;
    dsth=(winw*fbh)/fbw;
  }
  
  eh.dstr_right=(float)dstw/(float)winw;
  eh.dstr_top=(float)dsth/(float)winh;
  eh.dstr_left=-eh.dstr_right;
  eh.dstr_bottom=-eh.dstr_top;
  eh.dstr_clear=(dstw<winw)||(dsth<winh);
}

/* Commit framebuffer.
 */
 
void eh_commit_framebuffer(const void *fb) {
  eh_video_end(); // Just to be safe.
  
  eh_video_begin();
  if (eh_upload_framebuffer(fb)>=0) {
    eh_require_dstr();
    
    if (eh.dstr_clear) {
      glClearColor(0.0f,0.0f,0.0f,1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
    }
    
    //TODO Use OpenGL 2.x. It's better supported than 1.x.
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_TRIANGLE_STRIP);
      glTexCoord2i(0,0); glVertex2f(eh.dstr_left,eh.dstr_top);
      glTexCoord2i(0,1); glVertex2f(eh.dstr_left,eh.dstr_bottom);
      glTexCoord2i(1,0); glVertex2f(eh.dstr_right,eh.dstr_top);
      glTexCoord2i(1,1); glVertex2f(eh.dstr_right,eh.dstr_bottom);
    glEnd();
  }
  eh_video_end();
}

#endif
