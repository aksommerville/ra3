#include "../gui_internal.h"

/* Delete.
 */

void gui_texture_del(struct gui_texture *texture) {
  if (!texture) return;
  if (texture->refc-->1) return;
  glDeleteTextures(1,&texture->texid);
  free(texture);
}

/* Retain.
 */
 
int gui_texture_ref(struct gui_texture *texture) {
  if (!texture) return -1;
  if (texture->refc<1) return -1;
  if (texture->refc==INT_MAX) return -1;
  texture->refc++;
  return 0;
}

/* New.
 */

struct gui_texture *gui_texture_new() {
  struct gui_texture *texture=calloc(1,sizeof(struct gui_texture));
  if (!texture) return 0;
  texture->refc=1;
  glGenTextures(1,&texture->texid);
  if (!texture->texid) {
    glGenTextures(1,&texture->texid);
    if (!texture->texid) {
      gui_texture_del(texture);
      return 0;
    }
  }
  glBindTexture(GL_TEXTURE_2D,texture->texid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  return texture;
}

/* Trivial accessors.
 */
 
void gui_texture_get_size(int *w,int *h,const struct gui_texture *texture) {
  if (!texture) return;
  if (w) *w=texture->w;
  if (h) *h=texture->h;
}

/* Filter.
 */

void gui_texture_set_filter(struct gui_texture *texture,int filter) {
  glBindTexture(GL_TEXTURE_2D,texture->texid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,filter?GL_LINEAR:GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,filter?GL_LINEAR:GL_NEAREST);
}

/* Reallocate.
 */

int gui_texture_alloc(struct gui_texture *texture,int w,int h,int alpha) {
  glBindTexture(GL_TEXTURE_2D,texture->texid);
  glTexImage2D(GL_TEXTURE_2D,0,alpha?GL_RGBA:GL_RGB,w,h,0,alpha?GL_RGBA:GL_RGB,GL_UNSIGNED_BYTE,0);
  texture->w=w;
  texture->h=h;
}

/* Upload pixels.
 */
 
int gui_texture_upload_rgba(struct gui_texture *texture,int w,int h,const void *src) {
  glBindTexture(GL_TEXTURE_2D,texture->texid);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,src);
  texture->w=w;
  texture->h=h;
}

/* Use.
 */

void gui_texture_use(struct gui_texture *texture) {
  glBindTexture(GL_TEXTURE_2D,texture->texid);
}
