/* mn_widget_gamedetails.
 * Top row of home page.
 */
 
#include "../mn_internal.h"
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"
#include "opt/png/png.h"

static void gamedetails_arrange_bits(struct gui_widget *widget);

/* Object definition.
 */
 
struct mn_widget_gamedetails {
  struct gui_widget hdr;
  int gameid;
  int retry_fetch; // if >0, the last details fetch failed and we should try again after waiting so many frames.
  int gamelistseq;
  struct gui_texture *tex_name;
  struct mn_gamedetails_bit {
    struct gui_texture *texture;
    int x,y,w,h; // (x,y) relative to parent
    int editable;
    const char *k; // static string; mostly for internal identification
    int kc;
    int order;
    int solo; // if nonzero, this bit expects a full row to itself.
  } *bitv;
  int bitc,bita;
  struct png_image *flag_icons; // null or 256x32 RGBA
};

#define WIDGET ((struct mn_widget_gamedetails*)widget)

/* Delete.
 */
 
static void mn_gamedetails_bit_cleanup(struct mn_gamedetails_bit *bit) {
  gui_texture_del(bit->texture);
}
 
static void _gamedetails_del(struct gui_widget *widget) {
  if (WIDGET->bitv) {
    while (WIDGET->bitc-->0) mn_gamedetails_bit_cleanup(WIDGET->bitv+WIDGET->bitc);
    free(WIDGET->bitv);
  }
  png_image_del(WIDGET->flag_icons);
}

/* Init.
 */
 
static int _gamedetails_init(struct gui_widget *widget) {

  // Load flag icons.
  void *serial=0;
  int serialc=file_read(&serial,mn_data_path("flags.png"));
  if (serialc>=0) {
    struct png_image *image=png_decode(serial,serialc);
    free(serial);
    if (image) {
      if ((image->w!=256)||(image->h!=32)||(image->depth!=8)||(image->colortype!=6)) {
        fprintf(stderr,"flag icons image is not 256x32 rgba, won't use it\n");
        png_image_del(image);
      } else {
        WIDGET->flag_icons=image;
      }
    }
  }

  return 0;
}

/* Measure.
 */
 
static void _gamedetails_measure(int *w,int *h,struct gui_widget *widget,int maxw,int maxh) {
  *w=maxw;
  *h=maxh;
}

/* Pack.
 */
 
static void _gamedetails_pack(struct gui_widget *widget) {
  gamedetails_arrange_bits(widget);
}

/* Draw.
 */
 
static void _gamedetails_draw(struct gui_widget *widget,int x,int y) {
  gui_program_use(0);
  struct mn_gamedetails_bit *bit=WIDGET->bitv;
  int i=WIDGET->bitc;
  for (;i-->0;bit++) {
    if (!bit->texture) continue;
    gui_draw_texture(widget->gui,x+bit->x,y+bit->y,bit->texture);
  }
}

/* Add a blank bit.
 */
 
static struct mn_gamedetails_bit *gamedetails_add_bit(struct gui_widget *widget) {
  if (WIDGET->bitc>=WIDGET->bita) {
    int na=WIDGET->bita+16;
    if (na>INT_MAX/sizeof(struct mn_gamedetails_bit)) return 0;
    void *nv=realloc(WIDGET->bitv,sizeof(struct mn_gamedetails_bit)*na);
    if (!nv) return 0;
    WIDGET->bitv=nv;
    WIDGET->bita=na;
  }
  struct mn_gamedetails_bit *bit=WIDGET->bitv+WIDGET->bitc++;
  memset(bit,0,sizeof(struct mn_gamedetails_bit));
  return bit;
}

/* Add a "simple bit" string value.
 */
        
static struct mn_gamedetails_bit *gamedetails_add_simple_bit(struct gui_widget *widget,const char *k,int kc,const char *v,int vc,const char *fontname,int rgb) {
  struct mn_gamedetails_bit *bit=gamedetails_add_bit(widget);
  if (!bit) return 0;
  bit->k=k;
  bit->kc=kc;
  struct gui_font *font=gui_font_get(widget->gui,fontname,-1);
  if (!(bit->texture=gui_texture_from_text(widget->gui,font,v,vc,rgb))) return 0;
  gui_texture_get_size(&bit->w,&bit->h,bit->texture);
  return bit;
}
        
static struct mn_gamedetails_bit *gamedetails_add_multiline_bit(struct gui_widget *widget,const char *k,int kc,const char *v,int vc,const char *fontname,int rgb) {
  struct mn_gamedetails_bit *bit=gamedetails_add_bit(widget);
  if (!bit) return 0;
  bit->k=k;
  bit->kc=kc;
  struct gui_font *font=gui_font_get(widget->gui,fontname,-1);
  if (!(bit->texture=gui_texture_from_multiline_text(widget->gui,font,v,vc,rgb,widget->w))) return 0;
  gui_texture_get_size(&bit->w,&bit->h,bit->texture);
  return bit;
}

/* Blit a 32x32 tile from flag_icons to the RGBA output.
 * Caller ensures both sides are in bounds.
 * We actually memcpy each row; this won't work if there is existing content in (dst).
 */
 
static void gamedetails_blit_flag(void *dst,int dststride,struct gui_widget *widget,int tileid) {
  const uint8_t *src=(uint8_t*)WIDGET->flag_icons->pixels+tileid*32*4;
  int cpc=32*4;
  int yi=32;
  for (;yi-->0;dst=(char*)dst+dststride,src+=WIDGET->flag_icons->stride) {
    memcpy(dst,src,cpc);
  }
}

/* Add a bit for the flags string, picking icons for each flag.
 * Our icons don't actually line up 1:1 with flags.
 * Our icons, 32x32, reading left to right:
 *   other (review, and anything we don't recognize)
 *   player1
 *   player2
 *   playermore (+player3 +player4)
 *   faulty (+hardware +obscene)
 *   foreign
 *   favorite
 *   hack
 */
 
static struct mn_gamedetails_bit *gamedetails_add_flags_bit(struct gui_widget *widget,const char *src,int srcc) {
  if (!src||(srcc<1)) return 0;
  if (!WIDGET->flag_icons) return 0; // TODO Should we emit text if the icons aren't available? Would be trivial to do.
  
  // First read the input and tally up what we're going to display.
  int other=0,playerc=0,faulty=0,foreign=0,favorite=0,hack=0;
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
    
    if ((tokenc==7)&&!memcmp(token,"player1",7)) {
      if (playerc<1) playerc=1;
    } else if ((tokenc==7)&&!memcmp(token,"player2",7)) {
      if (playerc<2) playerc=2;
    } else if ((tokenc>=6)&&!memcmp(token,"player",6)) { // ..3 ..4 or ..more
      if (playerc<3) playerc=3;
    } else if ((tokenc==6)&&!memcmp(token,"faulty",6)) {
      faulty=1;
    } else if ((tokenc==7)&&!memcmp(token,"foreign",7)) {
      foreign=1;
    } else if ((tokenc==4)&&!memcmp(token,"hack",4)) {
      hack=1;
    } else if ((tokenc==8)&&!memcmp(token,"hardware",8)) {
      faulty=1;
    } else if ((tokenc==6)&&!memcmp(token,"review",6)) {
      other=1;
    } else if ((tokenc==7)&&!memcmp(token,"obscene",7)) {
      faulty=1;
    } else if ((tokenc==8)&&!memcmp(token,"favorite",8)) {
      favorite=1;
    } else {
      other=1;
    }
  }
  
  // Now determine the output width. If it comes up zero, don't bother creating a bit.
  const int iconw=32,iconh=32;
  const int margin=4;
  int w=0;
  if (other) w+=iconw+margin;
  if (playerc) w+=iconw+margin;
  if (faulty) w+=iconw+margin;
  if (foreign) w+=iconw+margin;
  if (favorite) w+=iconw+margin;
  if (hack) w+=iconw+margin;
  if (!w) return 0;
  
  // Generate the raw RGBA image by blitting from flag_icons.
  int stride=w<<2;
  uint32_t *rgba=calloc(stride,iconh);
  if (!rgba) return 0;
  int x=margin>>1;
  if (other) { gamedetails_blit_flag(rgba+x,stride,widget,0); x+=iconw+margin; }
  if (playerc) { gamedetails_blit_flag(rgba+x,stride,widget,playerc); x+=iconw+margin; } // player-count icons carefully arranged to match this index (1,2,3)
  if (faulty) { gamedetails_blit_flag(rgba+x,stride,widget,4); x+=iconw+margin; }
  if (foreign) { gamedetails_blit_flag(rgba+x,stride,widget,5); x+=iconw+margin; }
  if (favorite) { gamedetails_blit_flag(rgba+x,stride,widget,6); x+=iconw+margin; }
  if (hack) { gamedetails_blit_flag(rgba+x,stride,widget,7); x+=iconw+margin; }
  
  struct mn_gamedetails_bit *bit=gamedetails_add_bit(widget);
  if (!bit) {
    free(rgba);
    return 0;
  }
  bit->k="flags";
  bit->kc=5;
  if (
    !(bit->texture=gui_texture_new())||
    (gui_texture_upload_rgba(bit->texture,w,iconh,rgba)<0)
  ) {
    free(rgba);
    return 0;
  }
  free(rgba);
  bit->w=w;
  bit->h=iconh;
  return bit;
}

/* Clear all content.
 */
 
static void gamedetails_clear(struct gui_widget *widget) {
  while (WIDGET->bitc>0) {
    WIDGET->bitc--;
    mn_gamedetails_bit_cleanup(WIDGET->bitv+WIDGET->bitc);
  }
}

/* Generate text block for lists.
 * Decodes the incoming JSON.
 */
 
static int gamedetails_generate_lists_text(struct sr_encoder *dst,struct sr_decoder *decoder) {
  int arrayctx=sr_decode_json_array_start(decoder);
  if (arrayctx<0) return -1;
  while (sr_decode_json_next(0,decoder)>0) {
    int objctx=sr_decode_json_object_start(decoder);
    if (objctx<0) return -1;
    const char *k=0;
    int kc=0;
    while ((kc=sr_decode_json_next(&k,decoder))>0) {
      if ((kc==4)&&!memcmp(k,"name",4)) {
        char tmp[64];
        int tmpc=sr_decode_json_string(tmp,sizeof(tmp),decoder);
        if ((tmpc<0)||(tmpc>sizeof(tmp))) {
          if (sr_decode_json_skip(decoder)<0) return -1;
        } else {
          if (dst->c) {
            if (sr_encode_raw(dst,", ",2)<0) return -1;
          }
          if (sr_encode_raw(dst,tmp,tmpc)<0) return -1;
        }
      } else {
        if (sr_decode_json_skip(decoder)<0) return -1;
      }
    }
    if (sr_decode_json_end(decoder,objctx)<0) return -1;
  }
  if (sr_decode_json_end(decoder,arrayctx)<0) return -1;
  return 0;
}

/* Populate from JSON game.
 */
 
static void gamedetails_populate(struct gui_widget *widget,struct sr_decoder *decoder) {
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,decoder))>0) {
    
    /* comments: Add a bit for each comment with text.
     * Discard the timestamp. (or should we keep? We'll need it if we allow deleting or editing comments)
     */
    if ((kc==8)&&!memcmp(k,"comments",8)) {
      int jsonctx=sr_decode_json_array_start(decoder);
      if (jsonctx<0) return;
      int order=100;
      while (sr_decode_json_next(0,decoder)>0) {
        char text[1024];
        int textc=0;
        int subctx=sr_decode_json_object_start(decoder);
        if (subctx<0) return;
        const char *subk;
        int subkc;
        int comment_is_text=0;
        while ((subkc=sr_decode_json_next(&subk,decoder))>0) {
          if ((subkc==1)&&!memcmp(subk,"v",1)) {
            textc=sr_decode_json_string(text,sizeof(text),decoder);
            if ((textc<0)||(textc>sizeof(text))) {
              textc=0;
              if (sr_decode_json_skip(decoder)<0) return;
            }
          } else if ((subkc==1)&&!memcmp(subk,"k",1)) {
            char cmttype[32];
            int cmttypec=sr_decode_json_string(cmttype,sizeof(cmttype),decoder);
            if ((cmttypec<0)||(cmttypec>sizeof(cmttype))) {
              if (sr_decode_json_skip(decoder)<0) return;
            } else if ((cmttypec==4)&&!memcmp(cmttype,"text",4)) {
              comment_is_text=1;
            }
          } else {
            if (sr_decode_json_skip(decoder)<0) return;
          }
        }
        if (sr_decode_json_end(decoder,subctx)<0) return;
        if (comment_is_text&&(textc>0)&&(textc<=sizeof(text))) {
          struct mn_gamedetails_bit *bit=gamedetails_add_multiline_bit(widget,"comment",7,text,textc,"lightv40",0xffffff);
          if (bit) {
            bit->order=order++; // preserve order of comments, and start at 100 so they come after everything else
            bit->solo=1;
          }
        }
      }
      if (sr_decode_json_end(decoder,jsonctx)<0) return;
      continue;
    }
    
    /* lists: Compose a multiline text block from the list names, separated by comma+space.
     * Display that block as a solo row.
     */
    if ((kc==5)&&!memcmp(k,"lists",5)) {
      struct sr_encoder text={0};
      if (gamedetails_generate_lists_text(&text,decoder)<0) {
        sr_encoder_cleanup(&text);
        return;
      }
      if (text.c) {
        struct mn_gamedetails_bit *bit=gamedetails_add_multiline_bit(widget,"lists",5,text.v,text.c,"lightv40",0xc0c0c0);
        if (bit) {
          bit->order=9;
          bit->solo=1;
        }
      }
      sr_encoder_cleanup(&text);
      continue;
    }
    
    /* rating: Works like the general bits except we colorize based on its value.
     */
    if ((kc==6)&&!memcmp(k,"rating",6)) {
      int v=0;
      if (sr_decode_json_int(&v,decoder)<0) {
        if (sr_decode_json_skip(decoder)<0) return;
      }
      uint8_t r=0xff,g=0xff,b=0x00;
      if (v<=0) {
        r=g=b=0x80;
        v=0;
      } else if (v>=100) {
        r=0x00;
        v=99;
      } else if (v<50) {
        g=(v*0xff)/50;
      } else if (v>50) {
        r=((100-v)*0xff)/50;
      }
      char str[3]={0};
      if (v<10) str[0]='0'+v;
      else { str[0]='0'+v/10; str[1]='0'+v%10; }
      struct mn_gamedetails_bit *bit=gamedetails_add_simple_bit(widget,"rating",6,str,-1,"boldv40",(r<<16)|(g<<8)|b);
      if (bit) {
        bit->order=4;
        bit->solo=0;
      }
      continue;
    }
    
    /* flags: Compose a custom texture with icons instead of text labels.
     */
    if ((kc==5)&&!memcmp(k,"flags",5)) {
      char v[256];
      int vc=sr_decode_json_string(v,sizeof(v),decoder);
      if ((vc<0)||(vc>sizeof(v))) {
        if (sr_decode_json_skip(decoder)<0) return;
      } else if (vc) {
        struct mn_gamedetails_bit *bit=gamedetails_add_flags_bit(widget,v,vc);
        if (bit) {
          bit->order=8;
          bit->solo=0;
        }
      }
      continue;
    }

    /* Most things, we want to display as a simple scalar label.
     * Factor out the per-field work.
     */
    struct simple_bit {
      const char *name;
      int namec;
      const char *fontname;
      int rgb;
      int order;
      int solo;
    } simple_bits[]={
    #define _(tag,font,rgb,order,solo) {#tag,sizeof(#tag)-1,font,rgb,order,solo},
      _(name,    "boldv40", 0xffffff, 1,1)
      _(gameid,  "lightv40",0x808080, 2,0)
      _(platform,"lightv40",0x808080, 3,0)
      // 4: rating
      _(genre,   "lightv40",0xc0c0c0, 5,0)
      _(author,  "lightv40",0xc0c0c0, 6,0)
      _(pubtime, "lightv40",0xc0c0c0, 7,0)
      // 8: flags
      // 9: lists
      // 100+: comments
      // no need: path,blobs
    #undef _
    };
    
    const struct simple_bit *simple_bit=simple_bits;
    int i=sizeof(simple_bits)/sizeof(struct simple_bit);
    int ok=0;
    for (;i-->0;simple_bit++) {
      if (simple_bit->namec!=kc) continue;
      if (memcmp(simple_bit->name,k,kc)) continue;
      ok=1;
      break;
    }
    
    if (ok) {
      char tmp[256];
      int tmpc=sr_decode_json_string(tmp,sizeof(tmp),decoder);
      if ((tmpc>0)&&(tmpc<=sizeof(tmp))) {
        struct mn_gamedetails_bit *bit=gamedetails_add_simple_bit(widget,simple_bit->name,simple_bit->namec,tmp,tmpc,simple_bit->fontname,simple_bit->rgb);
        if (bit) {
          bit->order=simple_bit->order;
          bit->solo=simple_bit->solo;
        }
      } else {
        if (sr_decode_json_skip(decoder)<0) return;
      }
    } else {
      if (sr_decode_json_skip(decoder)<0) return;
    }
  }
}

/* Find a bit by name.
 */
 
static struct mn_gamedetails_bit *gamedetails_find_bit(const struct gui_widget *widget,const char *name,int namec) {
  struct mn_gamedetails_bit *bit=WIDGET->bitv;
  int i=WIDGET->bitc;
  for (;i-->0;bit++) {
    if (bit->kc!=namec) continue;
    if (memcmp(bit->k,name,namec)) continue;
    return bit;
  }
  return 0;
}

/* Having created a bunch of bits, now find real estate for them all.
 */
 
static int mn_gamedetails_bit_cmp(const void *A,const void *B) {
  const struct mn_gamedetails_bit *a=A,*b=B;
  return a->order-b->order;
}
 
static void gamedetails_arrange_bits(struct gui_widget *widget) {

  /* We're going to render in a kind of flow style.
   * Important that we first put all the bits in the expected order.
   */
  qsort(WIDGET->bitv,WIDGET->bitc,sizeof(struct mn_gamedetails_bit),mn_gamedetails_bit_cmp);
  
  const int margin=20;
  int y=0,p=0;
  while (p<WIDGET->bitc) {
    int c=1;
    int w=WIDGET->bitv[p].w+margin;
    if (!WIDGET->bitv[p].solo) {
      while ((p+c<WIDGET->bitc)&&!WIDGET->bitv[p+c].solo) {
        int nw=w+WIDGET->bitv[p+c].w+margin;
        if (nw>widget->w) break;
        c++;
        w=nw;
      }
    }
    int rowh=0;
    int x=(widget->w>>1)-(w>>1)+(margin>>1);
    int i=0;
    for (;i<c;i++) {
      struct mn_gamedetails_bit *bit=WIDGET->bitv+p+i;
      bit->x=x;
      bit->y=y;
      x+=bit->w+margin;
      if (bit->h>rowh) rowh=bit->h;
    }
    // now that we have the true (rowh), center each bit vertically
    for (i=0;i<c;i++) {
      struct mn_gamedetails_bit *bit=WIDGET->bitv+p+i;
      bit->y=y+(rowh>>1)-(bit->h>>1);
    }
    p+=c;
    y+=rowh;
  }
}

/* Update.
 */
 
static void _gamedetails_update(struct gui_widget *widget) {
  if (WIDGET->retry_fetch>0) {
    if (!--(WIDGET->retry_fetch)) {
      WIDGET->gameid=0;
    }
  } else if ((WIDGET->gameid!=mn.dbs.gameid)||(WIDGET->gamelistseq!=mn.dbs.gamelistseq)) {
    if (!widget->w||!widget->h) {
      // This can happen if we've just rebuilt. Wait for the next render to pack us, then we'll try again next time.
      // eg when exiting kiosk mode.
    } else {
      WIDGET->gamelistseq=mn.dbs.gamelistseq;
      WIDGET->gameid=mn.dbs.gameid;
      gamedetails_clear(widget);
      struct sr_decoder decoder={0};
      if (dbs_get_game_from_list(&decoder,&mn.dbs,WIDGET->gameid)>=0) {
        gamedetails_populate(widget,&decoder);
        gamedetails_arrange_bits(widget);
      } else {
        WIDGET->retry_fetch=15;
      }
    }
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_gamedetails={
  .name="gamedetails",
  .objlen=sizeof(struct mn_widget_gamedetails),
  .del=_gamedetails_del,
  .init=_gamedetails_init,
  .measure=_gamedetails_measure,
  .pack=_gamedetails_pack,
  .draw=_gamedetails_draw,
  .update=_gamedetails_update,
};
