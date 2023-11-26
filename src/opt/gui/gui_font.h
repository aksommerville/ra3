/* gui_font.h
 * We use fixed-size variable-width y8 bitmap fonts for everything.
 * Considered using freetype, but it's too complicated.
 * Some rules:
 *  - Font may be sourced from multiple image files, with each file defining a contiguous range of glyphs.
 *  - Each font has a fixed line height.
 *  - Pages may be monospaced or variable-width.
 *  - Variable-width pages must contain a fence row between image rows:
 *  - - First pixel must be pure white.
 *  - - The first column is skipped, then everything up to and including the first pure black, is one glyph.
 *  - - Leftmost column of the image must contain similar fences, to establish line height. (and they must all be the same height).
 *  - - There should *not* be fences at the right or bottom edge.
 *  - Monospaced pages must declare their geometry in the file name.
 *  - Monospaced pages must be composed of 16-glyph rows.
 *  - Variable file name: FONTNAME-CODEPOINT.png
 *  - Mono file name: FONTNAME-CODEPOINT-COLWxROWH.png
 *  - ^ CODEPOINT unprefixed hexadecimal.
 *  - Pages are not allowed to contain codepoint zero. It's conventional but not required, to start on a multiple of 16.
 *  - - (if you need glyphs in 1..15, you must start on a non-multiple-of-16).
 */
 
#ifndef GUI_FONT_H
#define GUI_FONT_H

#include <stdint.h>

struct gui_font {
  int refc;
  char *name;
  int namec;
  int lineh; // Applies to all pages. Required.
  int charw; // Zero if there's at least one variable-width page, or two monospaced pages with different widths.
  struct gui_font_page {
    uint32_t codepoint;
    int glyphc;
    int charw; // Nonzero only for monospaced pages. All glyphs are the same size and there's no fencing.
    uint8_t *v; // y8 LRTB, stride==w
    int w,h; // Image dimensions.
    struct gui_font_glyph { // Present only for variable-width pages.
      uint16_t x,y;
      uint8_t w;
    } *glyphv;
    int glypha;
  } *pagev;
  int pagec,pagea;
};

void gui_font_del(struct gui_font *font);
int gui_font_ref(struct gui_font *font);

struct gui_font *gui_font_new(struct gui *gui);

int gui_font_set_name(struct gui_font *font,const char *src,int srcc);

/* Adding a page, it will initially have zero glyphs, and we assert that it doesn't land inside any other page.
 * After you've set (glyphc), call gui_font_validate_pages() at some point to assert that there's no overlap.
 */
struct gui_font_page *gui_font_add_page(struct gui_font *font,uint32_t codepoint);
int gui_font_validate_pages(const struct gui_font *font);

int gui_font_pagev_search(const struct gui_font *font,uint32_t codepoint);

/* Main processing for initializing a font. One page at a time.
 * You must provide (colw,rowh) from gui_font_basename_parse, to identify monospaced pages.
 */
int gui_font_populate_page_from_image_file(
  struct gui_font *font,
  struct gui_font_page *page,
  const char *path,
  int colw,int rowh
);

struct gui_font_basename {
  const char *name;
  int namec;
  int codepoint;
  int colw,rowh;
};
int gui_font_basename_parse(struct gui_font_basename *dst,const char *src,int srcc);

/* Width in pixels that we would need to render this.
 * LF and CR are treated like any codepoint, we try to measure them horizontally.
 */
int gui_font_measure_line(const struct gui_font *font,const char *src,int srcc);

/* Break a chunk of text at sensible places to display on multiple lines no wider than (wlimit).
 * At the limit, eg (wlimit<=0), we'll make single-character lines, and they will exceed the limit.
 * If we return >dsta, the returned count is not reliable but everything up to (dsta) is final.
 * (we expect that dsta is a height limit on your end, and you won't need any detail beyond that).
 * (p,c) for each line does not necessarily cover the full input. We leave out trailing spaces.
 */
struct gui_font_line {
  int p; // Start position in (src).
  int c; // Length from (p) in bytes.
  int w; // Line width in pixels.
};
int gui_font_break_lines(struct gui_font_line *dst,int dsta,const struct gui_font *font,const char *src,int srcc,int wlimit);

/* Fill (dst) with an RGBA image of one line of text.
 * Color is the provided 24-bit RGB, and alpha is taken from the font.
 * It's advisable to initialize the image to transparent white (not transparent black!) if you're going to scale with interpolation.
 * We only visit pixels with nonzero alpha. But we do not blend, we overwrite visited pixels.
 */
void gui_font_render_line(void *dst,int dstw,int dsth,int dststride,const struct gui_font *font,const char *src,int srcc,int rgb);

#endif
