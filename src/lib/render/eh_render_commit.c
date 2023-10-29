#include "eh_render_internal.h"

/* Recalculate output bounds if needed.
 */
 
static inline void eh_require_output_bounds(struct eh_render *render) {
  if (!render->dstr_dirty) return;
  render->dstr_dirty=0;
  int fbw=eh.delegate.video_width;
  int fbh=eh.delegate.video_height;
  int winw=eh.video->w;
  int winh=eh.video->h;
  
  if ((fbw<1)||(fbh<1)||(winw<1)||(winh<1)) {
    render->dstr_left=-1.0f;
    render->dstr_right=1.0f;
    render->dstr_top=1.0f;
    render->dstr_bottom=-1.0f;
    render->dstr_clear=0;
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
  
  render->dstr_right=(float)dstw/(float)winw;
  render->dstr_top=(float)dsth/(float)winh;
  render->dstr_left=-render->dstr_right;
  render->dstr_bottom=-render->dstr_top;
  render->dstr_clear=(dstw<winw)||(dsth<winh);
}

/* Upload texture.
 */
 
static inline void eh_render_upload_texture(struct eh_render *render,const void *src) {
  glTexImage2D(
    GL_TEXTURE_2D,0,GL_RGB,
    eh.delegate.video_width,eh.delegate.video_height,
    0,render->fb_gl_format,render->fb_gl_type,src
  );
}

/* Commit framebuffer, main entry point.
 */
 
void eh_render_commit(struct eh_render *render) {
  if (!render->srcfb) return;
  if (!render->texid) return;
  
  eh_require_output_bounds(render);
  glViewport(0,0,eh.video->w,eh.video->h);
  
  glBindTexture(GL_TEXTURE_2D,render->texid);
  if (render->fbrgb) {
    render->fbcvt(render->fbrgb,render->srcfb,render);
    eh_render_upload_texture(render,render->fbrgb);
  } else {
    eh_render_upload_texture(render,render->srcfb);
  }
  
  if (render->dstr_clear) {
    glClearColor(0.0f,0.0f,0.0f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  
  struct { GLfloat x,y; } positionv[]={
    {render->dstr_left,render->dstr_top},
    {render->dstr_left,render->dstr_bottom},
    {render->dstr_right,render->dstr_top},
    {render->dstr_right,render->dstr_bottom},
  };
  struct { GLubyte x,y; } texcoordv[]={
    {0,0},{0,1},{1,0},{1,1},
  };
  
  glEnable(GL_TEXTURE_2D);
  glUseProgram(render->programid);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_FLOAT,0,sizeof(positionv[0]),positionv);
  glVertexAttribPointer(1,2,GL_UNSIGNED_BYTE,0,sizeof(texcoordv[0]),texcoordv);
  glDrawArrays(GL_TRIANGLE_STRIP,0,4);
}
