#include "gui_internal.h"
#include <GL/gl.h>

//TODO Rewrite with GL 2. We definitely need to run on ES systems, that generally don't support GL 1.

/* Prepare context.
 */
 
void gui_prepare_render(struct gui *gui) {
  glViewport(0,0,gui->root->w,gui->root->h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0,gui->root->w,gui->root->h,0,0,1);
}

/* Flat rectangle.
 */
 
void gui_draw_rect(struct gui *gui,int x,int y,int w,int h,uint32_t rgba) {
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBegin(GL_TRIANGLE_STRIP);
    glColor4ub(rgba>>24,rgba>>16,rgba>>8,rgba);
    glVertex2i(x,y);
    glVertex2i(x,y+h);
    glVertex2i(x+w,y);
    glVertex2i(x+w,y+h);
  glEnd();
}

/* Line.
 */
 
void gui_draw_line(struct gui *gui,int ax,int ay,int bx,int by,uint32_t rgba) {
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBegin(GL_LINES);
    glColor4ub(rgba>>24,rgba>>16,rgba>>8,rgba);
    glVertex2i(ax,ay);
    glVertex2i(bx,by);
  glEnd();
}

/* Outline rectangle.
 */
 
void gui_frame_rect(struct gui *gui,int x,int y,int w,int h,uint32_t rgba) {
  if ((w<1)||(h<1)) return;
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBegin(GL_LINE_STRIP);
    glColor4ub(rgba>>24,rgba>>16,rgba>>8,rgba);
    glVertex2i(x,y);
    glVertex2i(x,y+h-1);
    glVertex2i(x+w-1,y+h-1);
    glVertex2i(x+w-1,y);
    glVertex2i(x-1,y);
  glEnd();
}

/* Textured quad.
 */
 
void gui_draw_texture(struct gui *gui,int x,int y,struct gui_texture *texture) {
  if (!texture) return;
  glEnable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_DST_ALPHA);
  gui_texture_use(texture);
  glBegin(GL_TRIANGLE_STRIP);
    glColor4ub(0xff,0xff,0xff,0xff);
    glTexCoord2i(0,0); glVertex2i(x,y);
    glTexCoord2i(0,1); glVertex2i(x,y+texture->h);
    glTexCoord2i(1,0); glVertex2i(x+texture->w,y);
    glTexCoord2i(1,1); glVertex2i(x+texture->w,y+texture->h);
  glEnd();
}
