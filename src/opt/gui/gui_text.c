#include "gui_internal.h"
#include "opt/fs/fs.h"

static void gui_choose_default_font(struct gui *gui);

/* Global cleanup.
 */
 
void gui_text_quit(struct gui *gui) {
  if (gui->fontv) {
    while (gui->fontc-->0) gui_font_del(gui->fontv[gui->fontc]);
    free(gui->fontv);
  }
}

/* Consider one file during init.
 */
 
static int gui_text_cb_file(const char *path,const char *base,char type,void *userdata) {
  struct gui *gui=userdata;
  if (!type) type=file_get_type(path);
  if (type!='f') return 0;
  
  struct gui_font_basename name={0};
  if (gui_font_basename_parse(&name,base,-1)<0) {
    //fprintf(stderr,"%s: Not a font image.\n",path);
    return 0;
  }
  //fprintf(stderr,"%s: '%.*s' U+%x (%d,%d)\n",path,name.namec,name.name,name.codepoint,name.colw,name.rowh);
  
  struct gui_font *font=gui_font_get(gui,name.name,name.namec);
  if (!font) {
    if (!(font=gui_font_add(gui,name.name,name.namec))) return -1;
  }
  
  // If we have explicit glyph dimensions from the file (monospaced page), height must agree with the font.
  // If it's the first page, establish font's line height.
  if (name.rowh) {
    if (!font->lineh) font->lineh=name.rowh;
    else if (font->lineh!=name.rowh) {
      fprintf(stderr,"%s: Inconsistent line height for font '%.*s'\n",path,font->namec,font->name);
      return -1;
    }
  }
  if (font->pagec==0) {
    font->charw=name.colw;
  } else if (font->charw==name.colw) {
  } else {
    font->charw=0;
  }
  
  struct gui_font_page *page=gui_font_add_page(font,name.codepoint);
  if (!page) return -1;
  
  if (gui_font_populate_page_from_image_file(font,page,path,name.colw,name.rowh)<0) {
    fprintf(stderr,"%s: Error processing font image.\n",path);
    return -1;
  }
  if (gui_font_validate_pages(font)<0) {
    fprintf(stderr,"%s: Overlap detected in font '%.*s'\n",path,font->namec,font->name);
    return -1;
  }
  
  return 0;
}

/* Global init.
 */
 
int gui_text_init(struct gui *gui) {
  if (!gui->data_pathc) return 0;
  if (dir_read(gui->data_path,gui_text_cb_file,gui)<0) return -1;
  return 0;
}

/* Get font from repository.
 */
 
struct gui_font *gui_font_get(struct gui *gui,const char *name,int namec) {
  if (!gui||!name) return 0;
  if (namec<0) { namec=0; while (name[namec]) namec++; }
  struct gui_font **p=gui->fontv;
  int i=gui->fontc;
  for (;i-->0;p++) {
    if ((*p)->namec!=namec) continue;
    if (memcmp((*p)->name,name,namec)) continue;
    return *p;
  }
  return 0;
}

struct gui_font *gui_font_get_default(struct gui *gui) {
  if (gui->fontc<1) return 0;
  if (!gui->font) gui_choose_default_font(gui);
  return gui->font;
}

/* Create new font and add to repository.
 */
 
struct gui_font *gui_font_add(struct gui *gui,const char *name,int namec) {
  if (gui->fontc>=gui->fonta) {
    int na=gui->fonta+8;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(gui->fontv,sizeof(void*)*na);
    if (!nv) return 0;
    gui->fontv=nv;
    gui->fonta=na;
  }
  struct gui_font *font=gui_font_new(gui);
  if (!font) return 0;
  if (gui_font_set_name(font,name,namec)<0) {
    gui_font_del(font);
    return 0;
  }
  gui->fontv[gui->fontc++]=font;
  return font;
}

/* Choose a default font from (gui->fontv) and assign it WEAK to (gui->font).
 */
 
static void gui_choose_default_font(struct gui *gui) {
  if (gui->fontc<1) return;
  
  // Important that we call out all the ones we expect to exist, in the order of preference.
  // Otherwise, we'll pick [0] by default, and the order is determined by readdir(), which is essentially random.
  // If we wanted really to generalize this out, we could examine each font and weigh them by line height and G0 coverage or something.
  if (gui->font=gui_font_get(gui,"lightv40",-1)) return;
  if (gui->font=gui_font_get(gui,"boldv40",-1)) return;
  if (gui->font=gui_font_get(gui,"vintage8",-1)) return;
  
  gui->font=gui->fontv[0];
}

/* Texture from text.
 */
 
struct gui_texture *gui_texture_from_text(struct gui *gui,struct gui_font *font,const char *src,int srcc,int rgb) {
  if (!gui) return 0;
  if (!font) {
    if (gui->fontc<1) return 0;
    if (!gui->font) gui_choose_default_font(gui);
    font=gui->font;
  }
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  int w=gui_font_measure_line(font,src,srcc);
  if (w<1) return 0;
  if (w>GUI_TEXT_LINE_SANITY_LIMIT) return 0;
  int stride=w<<2;
  void *rgba=malloc(stride*font->lineh);
  if (!rgba) return 0;
  
  // Don't initialize to zeroes. Use the incoming color, so we don't get edge artifacts when scaling up.
  uint32_t blankwhite;
  uint32_t bodetect=0x04030201;
  if (*(uint8_t*)&bodetect==0x04) blankwhite=rgb<<8;
  else blankwhite=rgb&0xffffff;
  uint32_t *dst=rgba;
  int i=w*font->lineh;
  for (;i-->0;dst++) *dst=blankwhite;
  
  gui_font_render_line(rgba,w,font->lineh,stride,font,src,srcc,rgb);
  
  struct gui_texture *texture=gui_texture_new();
  if (!texture) {
    free(rgba);
    return 0;
  }
  int err=gui_texture_upload_rgba(texture,w,font->lineh,rgba);
  free(rgba);
  if (err<0) {
    gui_texture_del(texture);
    return 0;
  }
  
  return texture;
}

/* Texture from multi-line text.
 */
 
struct gui_texture *gui_texture_from_multiline_text(struct gui *gui,struct gui_font *font,const char *src,int srcc,int rgb,int wlimit) {
  if (!gui) return 0;
  if (!font) {
    if (gui->fontc<1) return 0;
    if (!gui->font) gui_choose_default_font(gui);
    font=gui->font;
  }
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  struct gui_font_line linev[32];
  const int linea=sizeof(linev)/sizeof(linev[0]);
  int linec=gui_font_break_lines(linev,linea,font,src,srcc,wlimit);
  if (linec<0) linec=0;
  else if (linec>linea) linec=linea;
  int h=linec*font->lineh;
  int w=0;
  struct gui_font_line *line=linev;
  int i=linec;
  for (;i-->0;line++) if (line->w>w) w=line->w;
  if (w>GUI_TEXT_LINE_SANITY_LIMIT) return 0;
  if (h>GUI_TEXT_LINE_SANITY_LIMIT) return 0;
  
  int stride=w<<2;
  void *rgba=malloc(stride*h);
  if (!rgba) return 0;
  
  uint32_t blankwhite;
  uint32_t bodetect=0x04030201;
  if (*(uint8_t*)&bodetect==0x04) blankwhite=rgb<<8;
  else blankwhite=rgb&0xffffff;
  uint32_t *dst=rgba;
  for (i=w*h;i-->0;dst++) *dst=blankwhite;
  
  int y=0;
  for (line=linev,i=linec;i-->0;line++,y+=font->lineh) {
    gui_font_render_line(rgba+stride*y,w,font->lineh,stride,font,src+line->p,line->c,rgb);
  }
  
  struct gui_texture *texture=gui_texture_new();
  if (!texture) {
    free(rgba);
    return 0;
  }
  int err=gui_texture_upload_rgba(texture,w,h,rgba);
  free(rgba);
  if (err<0) {
    gui_texture_del(texture);
    return 0;
  }
  
  return texture;
}
