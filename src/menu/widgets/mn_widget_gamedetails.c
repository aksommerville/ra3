/* mn_widget_gamedetails.
 * Top row of home page.
 */
 
#include "../mn_internal.h"
#include "opt/serial/serial.h"

/* Object definition.
 */
 
struct mn_widget_gamedetails {
  struct gui_widget hdr;
  int gameid;
  struct gui_texture *tex_name;
};

#define WIDGET ((struct mn_widget_gamedetails*)widget)

/* Delete.
 */
 
static void _gamedetails_del(struct gui_widget *widget) {
  gui_texture_del(WIDGET->tex_name);
}

/* Init.
 */
 
static int _gamedetails_init(struct gui_widget *widget) {
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
}

/* Draw.
 */
 
static void _gamedetails_draw(struct gui_widget *widget,int x,int y) {
  gui_program_use(0);
  if (WIDGET->tex_name) {
    int texw,texh;
    gui_texture_get_size(&texw,&texh,WIDGET->tex_name);
    int dstx=x+(widget->w>>1)-(texw>>1);
    gui_draw_texture(widget->gui,dstx,y,WIDGET->tex_name);
  }
}

/* Set name.
 */
 
static void gamedetails_set_name(struct gui_widget *widget,const char *src,int srcc) {
  if (!src) { src=""; srcc=0; } else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  gui_texture_del(WIDGET->tex_name);
  WIDGET->tex_name=0;
  if (!srcc) return;
  WIDGET->tex_name=gui_texture_from_text(widget->gui,0,src,srcc);
}

/* Clear all content.
 */
 
static void gamedetails_clear(struct gui_widget *widget) {
  gamedetails_set_name(widget,0,0);
  //TODO
}

/* Populate from JSON game.
 */
 
static void gamedetails_populate(struct gui_widget *widget,struct sr_decoder *decoder) {
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,decoder))>0) {
    /**
    const char *v;
    int vc=sr_decode_json_expression(&v,decoder);
    if (vc<0) return;
    if (vc<100) fprintf(stderr,"  %.*s: %.*s\n",kc,k,vc,v);
    else fprintf(stderr,"  %.*s %d bytes\n",kc,k,vc);
    /**/
    
    if ((kc==4)&&!memcmp(k,"name",4)) {
      char tmp[64];
      int tmpc=sr_decode_json_string(tmp,sizeof(tmp),decoder);
      if ((tmpc>0)&&(tmpc<=sizeof(tmp))) {
        gamedetails_set_name(widget,tmp,tmpc);
      } else {
        if (sr_decode_json_skip(decoder)<0) return;
      }
      
    } else {
      if (sr_decode_json_skip(decoder)<0) return;
    }
  }
}

/* Update.
 */
 
static void _gamedetails_update(struct gui_widget *widget) {
  if (WIDGET->gameid!=mn.dbs.gameid) {
    //fprintf(stderr,"%s:%d: TODO: Display details for gameid %d\n",__FILE__,__LINE__,mn.dbs.gameid);
    WIDGET->gameid=mn.dbs.gameid;
    gamedetails_clear(widget);
    struct sr_decoder decoder={0};
    if (dbs_get_game_from_list(&decoder,&mn.dbs,WIDGET->gameid)>=0) {
      gamedetails_populate(widget,&decoder);
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
