#include "gui_internal.h"
#include <GL/gl.h>

//TODO Rewrite with GL 2. We definitely need to run on ES systems, that generally don't support GL 1.

/* Raw shader.
 */
 
struct gui_raw_vertex {
  GLshort x,y;
  GLubyte r,g,b,a;
};
 
static const char gui_raw_vshader[]=
  "uniform vec2 screensize;\n"
  "attribute vec2 aposition;\n"
  "attribute vec4 acolor;\n"
  "varying vec4 vcolor;\n"
  "void main() {\n"
    "float dstx=(aposition.x*2.0)/screensize.x-1.0;\n"
    "float dsty=(aposition.y*-2.0)/screensize.y+1.0;\n"
    "gl_Position=vec4(dstx,dsty,0.0,1.0);\n"
    "vcolor=acolor;\n"
  "}\n"
"";

static const char gui_raw_fshader[]=
  "varying vec4 vcolor;\n"
  "void main() {\n"
    "gl_FragColor=vcolor;\n"
  "}\n"
"";

/* Plain texture shader.
 */

struct gui_tex_vertex {
  GLshort x,y;
  GLubyte tx,ty;
};

static const char gui_tex_vshader[]=
  "uniform vec2 screensize;\n"
  "attribute vec2 aposition;\n"
  "attribute vec2 atexcoord;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
    "float dstx=(aposition.x*2.0)/screensize.x-1.0;\n"
    "float dsty=(aposition.y*-2.0)/screensize.y+1.0;\n"
    "gl_Position=vec4(dstx,dsty,0.0,1.0);\n"
    "vtexcoord=atexcoord;\n"
  "}\n"
"";

static const char gui_tex_fshader[]=
  "uniform sampler2D sampler;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
    "gl_FragColor=texture2D(sampler,vtexcoord);\n"
  "}\n"
"";

/* Cleanup.
 */
 
void gui_render_quit(struct gui *gui) {
  gui_program_del(gui->program_raw);
  gui_program_del(gui->program_tex);
}

/* Init.
 */
 
int gui_render_init(struct gui *gui) {
  #define INITSHADER(name) if (!(gui->program_##name=gui_program_new(#name, \
    gui_##name##_vshader,sizeof(gui_##name##_vshader), \
    gui_##name##_fshader,sizeof(gui_##name##_fshader) \
  ))) return -1;
  INITSHADER(raw)
  INITSHADER(tex)
  #undef INITSHADER
  return 0;
}

/* Prepare context.
 */
 
void gui_prepare_render(struct gui *gui) {
  glViewport(0,0,gui->root->w,gui->root->h);
}

/* Flat rectangle.
 */
 
void gui_draw_rect(struct gui *gui,int x,int y,int w,int h,uint32_t rgba) {
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_DST_ALPHA);
  gui_program_use(gui->program_raw);
  gui_program_set_screensize(gui->program_raw,gui->pvw,gui->pvh);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  uint8_t r=rgba>>24,g=rgba>>16,b=rgba>>8,a=rgba;
  struct gui_raw_vertex vtxv[]={
    {x  ,y  ,r,g,b,a},
    {x  ,y+h,r,g,b,a},
    {x+w,y  ,r,g,b,a},
    {x+w,y+h,r,g,b,a},
  };
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(vtxv[0]),&vtxv[0].x);
  glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,1,sizeof(vtxv[0]),&vtxv[0].r);
  glDrawArrays(GL_TRIANGLE_STRIP,0,sizeof(vtxv)/sizeof(vtxv[0]));
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  gui_program_use(0);
}

/* Rectangle with gradient.
 */
 
void gui_draw_gradient(struct gui *gui,int x,int y,int w,int h,uint32_t nw,uint32_t ne,uint32_t sw,uint32_t se) {
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_DST_ALPHA);
  gui_program_use(gui->program_raw);
  gui_program_set_screensize(gui->program_raw,gui->pvw,gui->pvh);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  #define COLOR(name) name>>24,name>>16,name>>8,name
  struct gui_raw_vertex vtxv[]={
    {x  ,y  ,COLOR(nw)},
    {x  ,y+h,COLOR(sw)},
    {x+w,y  ,COLOR(ne)},
    {x+w,y+h,COLOR(se)},
  };
  #undef COLOR
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(vtxv[0]),&vtxv[0].x);
  glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,1,sizeof(vtxv[0]),&vtxv[0].r);
  glDrawArrays(GL_TRIANGLE_STRIP,0,sizeof(vtxv)/sizeof(vtxv[0]));
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  gui_program_use(0);
}

/* Line.
 */
 
void gui_draw_line(struct gui *gui,int ax,int ay,int bx,int by,uint32_t rgba) {
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_DST_ALPHA);
  gui_program_use(gui->program_raw);
  gui_program_set_screensize(gui->program_raw,gui->pvw,gui->pvh);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  uint8_t r=rgba>>24,g=rgba>>16,b=rgba>>8,a=rgba;
  struct gui_raw_vertex vtxv[]={
    {ax,ay,r,g,b,a},
    {bx,by,r,g,b,a},
  };
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(vtxv[0]),&vtxv[0].x);
  glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,1,sizeof(vtxv[0]),&vtxv[0].r);
  glDrawArrays(GL_LINES,0,sizeof(vtxv)/sizeof(vtxv[0]));
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  gui_program_use(0);
}

/* Outline rectangle.
 */
 
void gui_frame_rect(struct gui *gui,int x,int y,int w,int h,uint32_t rgba) {
  if ((w<1)||(h<1)) return;
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_DST_ALPHA);
  gui_program_use(gui->program_raw);
  gui_program_set_screensize(gui->program_raw,gui->pvw,gui->pvh);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  uint8_t r=rgba>>24,g=rgba>>16,b=rgba>>8,a=rgba;
  struct gui_raw_vertex vtxv[]={
    {x  ,y  ,r,g,b,a},
    {x  ,y+h,r,g,b,a},
    {x+w,y+h,r,g,b,a},
    {x+w,y  ,r,g,b,a},
    {x-1,y  ,r,g,b,a},
  };
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(vtxv[0]),&vtxv[0].x);
  glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,1,sizeof(vtxv[0]),&vtxv[0].r);
  glDrawArrays(GL_LINE_STRIP,0,sizeof(vtxv)/sizeof(vtxv[0]));
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  gui_program_use(0);
}

/* Textured quad.
 */
 
void gui_draw_texture(struct gui *gui,int x,int y,struct gui_texture *texture) {
  if (!texture) return;
  glEnable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_DST_ALPHA);
  gui_texture_use(texture);
  gui_program_use(gui->program_tex);
  gui_program_set_screensize(gui->program_tex,gui->pvw,gui->pvh);
  gui_program_set_sampler(gui->program_tex,0);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  struct gui_tex_vertex vtxv[]={
    {x,y,0,0},
    {x,y+texture->h,0,1},
    {x+texture->w,y,1,0},
    {x+texture->w,y+texture->h,1,1},
  };
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(vtxv[0]),&vtxv[0].x);
  glVertexAttribPointer(1,2,GL_UNSIGNED_BYTE,0,sizeof(vtxv[0]),&vtxv[0].tx);
  glDrawArrays(GL_TRIANGLE_STRIP,0,sizeof(vtxv)/sizeof(vtxv[0]));
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  gui_program_use(0);
}
