#include "gui_internal.h"
#include "opt/fs/fs.h"
#include "opt/png/png.h"
#include "opt/serial/serial.h"

/* Delete.
 */
 
static void gui_font_page_cleanup(struct gui_font_page *page) {
  if (page->v) free(page->v);
  if (page->glyphv) free(page->glyphv);
}
 
void gui_font_del(struct gui_font *font) {
  if (!font) return;
  if (font->refc-->1) return;
  if (font->name) free(font->name);
  if (font->pagev) {
    while (font->pagec-->0) gui_font_page_cleanup(font->pagev+font->pagec);
    free(font->pagev);
  }
  free(font);
}

/* Retain.
 */

int gui_font_ref(struct gui_font *font) {
  if (!font) return -1;
  if (font->refc<1) return -1;
  if (font->refc==INT_MAX) return -1;
  font->refc++;
  return 0;
}

/* New.
 */

struct gui_font *gui_font_new(struct gui *gui) {
  if (!gui) return 0;
  struct gui_font *font=calloc(1,sizeof(struct gui_font));
  font->refc=1;
  return font;
}

/* Set name.
 */
 
int gui_font_set_name(struct gui_font *font,const char *src,int srcc) {
  if (!font) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (font->name) free(font->name);
  font->name=nv;
  font->namec=srcc;
  return 0;
}

/* Add page.
 */
 
struct gui_font_page *gui_font_add_page(struct gui_font *font,uint32_t codepoint) {
  int p=gui_font_pagev_search(font,codepoint);
  if (p>=0) return 0;
  p=-p-1;
  if (font->pagec>=font->pagea) {
    int na=font->pagea+4;
    if (na>INT_MAX/sizeof(struct gui_font_page)) return 0;
    void *nv=realloc(font->pagev,sizeof(struct gui_font_page)*na);
    if (!nv) return 0;
    font->pagev=nv;
    font->pagea=na;
  }
  struct gui_font_page *page=font->pagev+p;
  memmove(page+1,page,sizeof(struct gui_font_page)*(font->pagec-p));
  font->pagec++;
  memset(page,0,sizeof(struct gui_font_page));
  page->codepoint=codepoint;
  return page;
}

/* Assert no page overlap.
 */
 
int gui_font_validate_pages(const struct gui_font *font) {
  const struct gui_font_page *page=font->pagev+1;
  int i=font->pagec-1;
  for (;i-->0;page++) {
    if (page->codepoint<page[-1].codepoint+page[-1].glyphc) return -1;
  }
  return 0;
}

/* Search pages.
 */
 
int gui_font_pagev_search(const struct gui_font *font,uint32_t codepoint) {
  int lo=0,hi=font->pagec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct gui_font_page *page=font->pagev+ck;
         if (codepoint<page->codepoint) hi=ck;
    else if (codepoint>=page->codepoint+page->glyphc) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Populate (glyphv,glyphc) for a variable-width page, by reading the provided image.
 * Initialize or validate (font->lineh).
 * Caller initializes everything else, before or after doesn't matter.
 */
 
static int gui_font_page_add_glyph(struct gui_font_page *page,int x,int y,int w) {
  if (x>0xffff) return -1;
  if (y>0xffff) return -1;
  if (w>0xff) return -1;
  if (!w) return -1;
  if (page->glyphc>=page->glypha) {
    int na=page->glypha+16;
    if (na>INT_MAX/sizeof(struct gui_font_glyph)) return -1;
    void *nv=realloc(page->glyphv,sizeof(struct gui_font_glyph)*na);
    if (!nv) return -1;
    page->glyphv=nv;
    page->glypha=na;
  }
  struct gui_font_glyph *glyph=page->glyphv+page->glyphc++;
  glyph->x=x;
  glyph->y=y;
  glyph->w=w;
  return 0;
}
 
static int gui_font_populate_variable_page_row(
  struct gui_font *font,
  struct gui_font_page *page,
  struct png_image *image,
  int y,
  const uint8_t *row
) {
  
  // Each leading row must begin with a pure-white pixel.
  if (*row!=0xff) return -1;
  
  // Establish or validate row height.
  int rowh=0;
  const uint8_t *p=row;
  while (1) {
    if (y+rowh>=image->h) return -1;
    if (*p==0x00) break;
    if (*p!=0xff) return -1;
    rowh++;
    p+=image->stride;
  }
  if (rowh<1) {
    return -1;
  } else if (font->lineh) {
    if (rowh!=font->lineh) return -1;
  } else {
    font->lineh=rowh;
  }
  
  // Run along the control row horizontally.
  // If there's a black pixel somewhere right of us, add a glyph and move to its right.
  // When we run out of image, or it's white all the way to the edge, we're done.
  p=row;
  y++; // (y) is now the top of the glyphs region, and it doesn't match (p) anymore.
  int x=0;
  while (x<image->w) {
    if (*p!=0xff) return -1;
    // Could assert that this column is full white, or full white with a single black at the bottom, but these vertical bars are really just commentary.
    p++;
    x++;
    int chw=0,ok=0;
    while (1) {
      if (x+chw>=image->w) break;
      if (p[chw]==0x00) { ok=1; chw++; break; } // include the black column
      if (p[chw]!=0xff) return -1; // Control row must be nothing but black and white.
      chw++;
    }
    if (!ok) break; // end of row
    if (gui_font_page_add_glyph(page,x,y,chw)<0) return -1;
    x+=chw;
    p+=chw;
  }
  
  // We always consume the same amount: 1+lineh.
  return 1+font->lineh;
}
 
static int gui_font_populate_variable_page(
  struct gui_font *font,
  struct gui_font_page *page,
  struct png_image *image,
  const char *path
) {
  int y=0;
  const uint8_t *row=image->pixels;
  int ystop=image->h-1; // No need to read the last row; we expect one row below the focussed one.
  while (y<ystop) {
    int err=gui_font_populate_variable_page_row(font,page,image,y,row);
    if (err<=0) {
      fprintf(stderr,"%s: Error processing variable-width font row at y=%d\n",path,y);
      return -1;
    }
    y+=err;
    row+=err*image->stride;
  }
  return 0;
}

/* Populate page from decoded PNG file.
 * Caller must convert to colortype==0 depth==8.
 */
 
static int gui_font_populate_page_from_png(
  struct gui_font *font,
  struct gui_font_page *page,
  struct png_image *image,
  int colw,int rowh,
  const char *path
) {
  //fprintf(stderr,"%s: font=%.*s codepoint=U+%x image=%d,%d glyph=%d,%d path=%s\n",__func__,font->namec,font->name,page->codepoint,image->w,image->h,colw,rowh,path);
  if (image->colortype!=0) return -1;
  if (image->depth!=8) return -1;
  if (image->w!=image->stride) return -1;
  
  /* Monospaced pages are easy, and it's largely just a matter of validation.
   */
  if (colw||rowh) {
    if ((colw<1)||(rowh<1)||(colw>255)||(rowh>255)) return -1;
    if (font->lineh) {
      if (font->lineh!=rowh) return -1;
    } else {
      font->lineh=rowh;
    }
    if (image->w%colw) return -1;
    if (image->h%rowh) return -1;
    int colc=image->w/colw;
    int rowc=image->h/rowh;
    page->charw=colw;
    if (colc!=16) return -1;
    if (rowh<1) return 0;
    page->glyphc=colc*rowc;
    
  /* Variable-width pages, we need to build up a list of glyphs by reading the image.
   */
  } else {
    if (gui_font_populate_variable_page(font,page,image,path)<0) return -1;
  }
  
  /* Yoink the image's pixels.
   */
  page->v=image->pixels;
  image->pixels=0;
  page->w=image->w;
  page->h=image->h;
  
  //fprintf(stderr,"%.*s: Added page U+%x with %d glyphs\n",font->namec,font->name,page->codepoint,page->glyphc);
  
  return 0;
}

/* Populate page from named PNG file.
 */
 
int gui_font_populate_page_from_image_file(
  struct gui_font *font,
  struct gui_font_page *page,
  const char *path,
  int colw,int rowh
) {
  if (!font||!page||!path) return -1;
  if (page->v) return -1;
  void *serial=0;
  int serialc=file_read(&serial,path);
  if (serialc<0) return -1;
  struct png_image *image=png_decode(serial,serialc);
  free(serial);
  if (!image) return -1;
  {
    struct png_image *converted=png_image_reformat(image,0,0,0,0,8,0,0);
    png_image_del(image);
    if (!(image=converted)) return -1;
  }
  int err=gui_font_populate_page_from_png(font,page,image,colw,rowh,path);
  png_image_del(image);
  return err;
}

/* Parse basename.
 */
 
int gui_font_basename_parse(struct gui_font_basename *dst,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0;
  
  // Starts with name: [a-zA-Z0-9_]+ and must terminate with a dash.
  dst->name=src+srcp;
  dst->namec=0;
  while ((srcp<srcc)&&(src[srcp]!='-')) {
    char ch=src[srcp++];
    if (
      ((ch>='a')&&(ch<='z'))||
      ((ch>='A')&&(ch<='Z'))||
      ((ch>='0')&&(ch<='9'))||
      (ch=='_')
    ) dst->namec++;
    else return -1;
  }
  if (!dst->namec) return -1;
  if ((srcp>=srcc)||(src[srcp++]!='-')) return -1;
  
  // Next, base codepoint, unprefixed hexadecimal. Terminated by dash or dot.
  dst->codepoint=0;
  while (srcp<srcc) {
    char ch=src[srcp];
    if ((ch=='-')||(ch=='.')) break;
    srcp++;
    dst->codepoint<<=4;
         if ((ch>='0')&&(ch<='9')) dst->codepoint|=ch-'0';
    else if ((ch>='a')&&(ch<='f')) dst->codepoint|=ch-'a'+10;
    else if ((ch>='A')&&(ch<='F')) dst->codepoint|=ch-'A'+10;
    else return -1;
    if (dst->codepoint>0xffffff) return -1;
  }
  if (dst->codepoint<1) return -1;
  
  // If we're at a dash, it must be followed by COLWxROWH
  dst->colw=dst->rowh=0;
  if ((srcp<srcc)&&(src[srcp]=='-')) {
    srcp++;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
      dst->colw*=10;
      dst->colw+=src[srcp++]-'0';
    }
    if ((srcp>=srcc)||(src[srcp++]!='x')) return -1;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
      dst->rowh*=10;
      dst->rowh+=src[srcp++]-'0';
    }
    if ((dst->colw<1)||(dst->rowh<1)) return -1;
  }
  
  // Anything remaining must begin with a dot, and is ignored.
  if ((srcp<srcc)&&(src[srcp]!='.')) return -1;
  return 0;
}

/* Measure line.
 */
 
int gui_font_measure_line(const struct gui_font *font,const char *src,int srcc) {
  if (!font||!src) return 0;
  if (font->pagec<1) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int cpfirst=font->pagev[0].codepoint;
  int cppagezero=cpfirst+font->pagev[0].glyphc;
  int srcp=0,w=0;
  while (srcp<srcc) {
  
    int ch=0,seqlen;
    if ((seqlen=sr_utf8_decode(&ch,src+srcp,srcc-srcp))<1) {
      seqlen=1;
      ch=src[srcp++];
    }
    srcp+=seqlen;
    
    /* It's very likely that we have just one page, and even if there's more, very likely for a glyph to be in that first page.
     * So we only enter the general page search for codepoints above the first page, as an optimization.
     */
    const struct gui_font_page *page=0;
    if (ch<cpfirst) ;
    else if (ch<cppagezero) {
      page=font->pagev;
    } else {
      int p=gui_font_pagev_search(font,ch);
      if (p>=0) page=font->pagev+p;
    }
    if (!page) continue;
    
    if (page->charw) w+=page->charw;
    else {
      int glyphp=ch-page->codepoint;
      if (glyphp>=page->glyphc) continue; // shouldn't be possible
      w+=page->glyphv[glyphp].w;
    }
  }
  return w;
}

/* Render one glyph.
 * (dst) is RGBA, (src) is Y8.
 */
 
static void gui_font_render_glyph(
  uint8_t *dst,int dstx,int dstw,int dsth,int dststride,
  const uint8_t *src,int srcw,int srch,int srcstride,
  uint8_t r,uint8_t g,uint8_t b
) {
  if (srch>dsth) srch=dsth;
  if (dstx>dstw-srcw) srcw=dstw-dstx;
  if (srcw<1) return;
  if (srch<1) return;
  dst+=dstx<<2;
  for (;srch-->0;dst+=dststride,src+=srcstride) {
    uint8_t *dstp=dst;
    const uint8_t *srcp=src;
    int xi=srcw;
    for (;xi-->0;dstp+=4,srcp++) {
      if (!*srcp) continue;
      dstp[0]=r;
      dstp[1]=g;
      dstp[2]=b;
      dstp[3]=*srcp; // copy nonzero alphas verbatim (we assume that the unvisited pixels had zero alpha to begin with)
    }
  }
}

/* Render line.
 */
 
void gui_font_render_line(
  void *dst,int dstw,int dsth,int dststride,
  const struct gui_font *font,
  const char *src,int srcc,
  int rgb
) {
  if (!dst||!font||!src) return;
  if (font->pagec<1) return;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  uint8_t r=rgb>>16,g=rgb>>8,b=rgb;
  int cpfirst=font->pagev[0].codepoint;
  int cppagezero=cpfirst+font->pagev[0].glyphc;
  int srcp=0,x=0;
  while (srcp<srcc) {
  
    int ch=0,seqlen;
    if ((seqlen=sr_utf8_decode(&ch,src+srcp,srcc-srcp))<1) {
      seqlen=1;
      ch=src[srcp++];
    }
    srcp+=seqlen;
    
    const struct gui_font_page *page=0;
    if (ch<cpfirst) ;
    else if (ch<cppagezero) {
      page=font->pagev;
    } else {
      int p=gui_font_pagev_search(font,ch);
      if (p>=0) page=font->pagev+p;
    }
    if (!page) continue;
    
    ch-=page->codepoint;
    if ((ch<0)||(ch>=page->glyphc)) continue;
    const uint8_t *bits=page->v;
    int glyphw;
    if (page->charw) {
      bits+=(ch>>4)*font->lineh*page->w+(ch&15)*page->charw;
      glyphw=page->charw;
    } else {
      const struct gui_font_glyph *glyph=page->glyphv+ch;
      bits+=glyph->y*page->w+glyph->x;
      glyphw=glyph->w;
    }
    
    gui_font_render_glyph(
      dst,x,dstw,dsth,dststride,
      bits,glyphw,font->lineh,page->w,
      r,g,b
    );
    x+=glyphw;
  }
}

/* Break lines.
 */
 
int gui_font_break_lines(struct gui_font_line *dst,int dsta,const struct gui_font *font,const char *src,int srcc,int wlimit) {
  if (!dst||(dsta<0)) dsta=0;
  if (!font) return -1;
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc=0,srcp=0;
  while (srcp<srcc) {
  
    if (dstc>=dsta) return dsta+1; // We're not obliged to continue processing after filling (dst), just indicate that it's full.
    struct gui_font_line *line=dst+dstc++;
    line->p=srcp;
    line->c=0;
    line->w=0;
    
    // As a firm rule, each line must contain at least one character, even if it breaches the limit.
    // Try to take a full UTF-8 unit. But we're not going to bugger around with misencoded bits that the renderer will treat as multiple glyphs.
    while ((srcp<srcc)&&(src[srcp]&0x80)) { srcp++; line->c++; }
    if (srcp<srcc) { srcp++; line->c++; }
    line->w=gui_font_measure_line(font,src+line->p,line->c);
    
    // Extend by words until we reach the limit or a linefeed.
    while (line->w<wlimit) {
      int nextw=line->w;
      int nextc=line->c;
      // Consume non-word characters individually.
      while (line->p+nextc<srcc) {
        char ch=src[line->p+nextc];
        if (ch==0x0a) break;
        if (
          ((ch>='a')&&(ch<='z'))||
          ((ch>='A')&&(ch<='Z'))||
          ((ch>='0')&&(ch<='9'))||
          (ch=='_')
        ) break;
        int addw=gui_font_measure_line(font,&ch,1);
        if (addw>0) {
          if (nextw+addw>wlimit) break;
          nextw+=addw;
        }
        nextc++;
      }
      // Measure the next word.
      int wordlen=0;
      while (line->p+nextc+wordlen<srcc) {
        char ch=src[line->p+nextc+wordlen];
        if (
          ((ch>='a')&&(ch<='z'))||
          ((ch>='A')&&(ch<='Z'))||
          ((ch>='0')&&(ch<='9'))||
          (ch=='_')
        ) {
          wordlen++;
        } else {
          break;
        }
      }
      // If the word is followed by punctuation (not space!), add one character to it.
      // To avoid dots and commas breaking onto the next line.
      if (line->p+nextc+wordlen<srcc) {
        char ch=src[line->p+nextc+wordlen];
        if ((ch>0x20)&&(ch<0x7f)) {
          wordlen++;
        }
      }
      if (wordlen) {
        int wordw=gui_font_measure_line(font,src+line->p+nextc,wordlen);
        if (nextw+wordw<=wlimit) {
          nextw+=wordw;
          nextc+=wordlen;
        } else {
          break;
        }
      }
      // Commit progress, or if none was possible, stop adding things.
      if (nextc==line->c) break;
      line->c=nextc;
      line->w=nextw;
    }
    
    // Finally, skip whitespace. But stop at linefeed (and consume it).
    srcp+=line->c;
    while (srcp<srcc) {
      if ((unsigned char)src[srcp]>0x20) break;
      if (src[srcp]==0x0a) { srcp++; break; }
      srcp++;
    }
  }
  return dstc;
}
