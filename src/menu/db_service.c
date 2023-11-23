#include "mn_internal.h"
#include "opt/serial/serial.h"
#include "opt/fakews/fakews.h"
#include "opt/fs/fs.h"

/* Cleanup.
 */
 
void dbs_cleanup(struct db_service *dbs) {
  if (dbs->gamelist) free(dbs->gamelist);
  if (dbs->list) free(dbs->list);
  if (dbs->text) free(dbs->text);
  if (dbs->flags) free(dbs->flags);
  if (dbs->author) free(dbs->author);
  if (dbs->genre) free(dbs->genre);
  if (dbs->platform) free(dbs->platform);
  if (dbs->sort) free(dbs->sort);
  if (dbs->state_path) free(dbs->state_path);
  if (dbs->lists) free(dbs->lists);
  if (dbs->genres) free(dbs->genres);
  if (dbs->authors) free(dbs->authors);
  if (dbs->platforms) free(dbs->platforms);
}

/* After loading initial state if available, fill in anything missing.
 */
 
static int dbs_state_default(struct db_service *dbs) {

  // (sort) should be "name". I might not provide UI to change it.
  if (!dbs->sortc) {
    if (dbs_search_set_sort(dbs,"name",4)<0) return -1;
  }

  return 0;
}

/* Init.
 */
 
int dbs_init(struct db_service *dbs) {

  dbs->daterange[0]=0;
  dbs->daterange[1]=9999;

  char *dir=0;
  int dirc=eh_get_scratch_directory(&dir);
  if (dirc>=0) {
    char path[1024];
    int pathc=snprintf(path,sizeof(path),"%.*s/db_service",dirc,dir);
    if ((pathc>0)&&(pathc<sizeof(path))) {
      if (!(dbs->state_path=malloc(pathc+1))) return -1;
      memcpy(dbs->state_path,path,pathc);
      dbs->state_path[pathc]=0;
      dbs_state_read(dbs);
    }
    free(dir);
  }
  
  if (dbs_state_default(dbs)<0) return -1;

  return 0;
}

/* Decode state.
 */
 
static int dbs_state_decode(struct db_service *dbs,struct sr_decoder *decoder) {
  int topctx=sr_decode_json_object_start(decoder);
  if (topctx<0) return -1;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,decoder))>0) {
    
    #define STR(tag) if ((kc==sizeof(#tag)-1)&&!memcmp(k,#tag,kc)) { \
      char tmp[256]; \
      int tmpc=sr_decode_json_string(tmp,sizeof(tmp),decoder); \
      if ((tmpc<0)||(tmpc>sizeof(tmp))) { \
        if (sr_decode_json_skip(decoder)<0) return -1; \
      } else if (tmpc>0) { \
        char *nv=malloc(tmpc+1); \
        if (!nv) return -1; \
        memcpy(nv,tmp,tmpc); \
        nv[tmpc]=0; \
        if (dbs->tag) free(dbs->tag); \
        dbs->tag=nv; \
        dbs->tag##c=tmpc; \
      } else { \
        dbs->tag##c=0; \
      } \
      continue; \
    }
    #define INT(tag) if ((kc==sizeof(#tag)-1)&&!memcmp(k,#tag,kc)) { \
      int v; \
      if (sr_decode_json_int(&v,decoder)<0) { \
        if (sr_decode_json_skip(decoder)<0) return -1; \
      } else { \
        dbs->tag=v; \
      } \
      continue; \
    }
    
    INT(gameid)
    INT(page)
    INT(pagec)
    STR(list)
    STR(text)
    INT(pubtimelo)
    INT(pubtimehi)
    INT(ratinglo)
    INT(ratinghi)
    STR(flags)
    STR(author)
    STR(genre)
    STR(platform)
    STR(sort)
    
    #undef STR
    #undef INT
    if (sr_decode_json_skip(decoder)<0) return -1;
  }
  return sr_decode_json_end(decoder,topctx);
}

/* Encode state.
 */
 
static int dbs_state_encode(struct sr_encoder *encoder,struct db_service *dbs) {
  int topctx=sr_encode_json_object_start(encoder,0,0);
  if (topctx<0) return -1;
  
  if (sr_encode_json_int(encoder,"gameid",6,dbs->gameid)<0) return -1;
  if (sr_encode_json_int(encoder,"page",4,dbs->page)<0) return -1;
  if (sr_encode_json_int(encoder,"pagec",5,dbs->pagec)<0) return -1;
  
  #define STR(tag) if (dbs->tag##c&&(sr_encode_json_string(encoder,#tag,sizeof(#tag)-1,dbs->tag,dbs->tag##c)<0)) return -1;
  #define INT(tag) if (dbs->tag&&(sr_encode_json_int(encoder,#tag,sizeof(#tag)-1,dbs->tag)<0)) return -1;
  STR(list)
  STR(text)
  INT(pubtimelo)
  INT(pubtimehi)
  INT(ratinglo)
  INT(ratinghi)
  STR(flags)
  STR(author)
  STR(genre)
  STR(platform)
  STR(sort)
  #undef STR
  #undef INT
  
  if (sr_encode_json_object_end(encoder,topctx)<0) return -1;
  sr_encode_raw(encoder,"\n",1);
  return 0;
}

/* State to/from JSON file.
 */
 
int dbs_state_read(struct db_service *dbs) {
  if (!dbs||!dbs->state_path) return -1;
  void *serial=0;
  int serialc=file_read(&serial,dbs->state_path);
  if (serialc<0) return -1;
  struct sr_decoder decoder={.v=serial,.c=serialc};
  int err=dbs_state_decode(dbs,&decoder);
  free(serial);
  return err;
}
 
int dbs_state_write(struct db_service *dbs) {
  if (!dbs||!dbs->state_path) return -1;
  struct sr_encoder encoder={0};
  if (dbs_state_encode(&encoder,dbs)<0) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  int err=file_write(dbs->state_path,encoder.v,encoder.c);
  sr_encoder_cleanup(&encoder);
  return err;
}

/* Set game list.
 */
 
static int dbs_set_game_list_json(struct db_service *dbs,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (dbs->gamelist) free(dbs->gamelist);
  dbs->gamelist=nv;
  dbs->gamelistc=srcc;
  dbs->gamelistseq++;
  return 0;
}

static int dbs_receive_game_list_headers(struct db_service *dbs,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  dbs->totalc=0;
  dbs->pagec=1;
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_object_start(&decoder)>=0) {
    const char *k;
    int kc;
    while ((kc=sr_decode_json_next(&k,&decoder))>0) {
      if ((kc==12)&&!memcmp(k,"X-Page-Count",12)) {
        int v=0;
        if (sr_decode_json_int(&v,&decoder)>=0) dbs->pagec=v;
      } else if ((kc==13)&&!memcmp(k,"X-Total-Count",13)) {
        int v=0;
        if (sr_decode_json_int(&v,&decoder)>=0) dbs->totalc=v;
      } else {
        if (sr_decode_json_skip(&decoder)<0) break;
      }
    }
  }
  if (dbs->pagec<1) dbs->pagec=1;
  if (dbs->page<1) dbs->page=1;
  else if (dbs->page>dbs->pagec) dbs->page=dbs->pagec;
  return 0;
}

/* Receive daterange response.
 */
 
static int dbs_set_daterange_json(struct db_service *dbs,const char *src,int srcc) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_array_start(&decoder)<0) return -1;
  if (sr_decode_json_next(0,&decoder)<1) return -1;
  if (sr_decode_json_int(dbs->daterange+0,&decoder)<0) return -1;
  if (sr_decode_json_next(0,&decoder)<1) return -1;
  if (sr_decode_json_int(dbs->daterange+1,&decoder)<0) return -1;
  return 0;
}

/* HTTP response.
 */
 
void dbs_http_response(struct db_service *dbs,const char *src,int srcc) {
  struct eh_http_response response={0};
  if (eh_http_response_split(&response,src,srcc)<0) return;
  if (response.status!=200) {
    fprintf(stderr,"%s:ERROR: %.*s\n",__func__,srcc,src);
    return;
  }
  
  if (response.x_correlation_id) {
  
    if (response.x_correlation_id==dbs->search_correlation_id) {
      dbs->search_correlation_id=0;
      dbs_set_game_list_json(dbs,response.body,response.bodyc);
      dbs_receive_game_list_headers(dbs,response.headers,response.headersc);
      return;
    }
  
    #define TEXTRSP(tag) { \
      if (response.x_correlation_id==dbs->tag##_correlation_id) { \
        dbs->tag##_correlation_id=0; \
        char *nv=0; \
        if (response.bodyc) { \
          if (!(nv=malloc(response.bodyc+1))) return; \
          memcpy(nv,response.body,response.bodyc); \
          nv[srcc]=0; \
        } \
        if (dbs->tag) free(dbs->tag); \
        dbs->tag=nv; \
        dbs->tag##c=response.bodyc; \
        return; \
      } \
    }
    TEXTRSP(lists)
    TEXTRSP(genres)
    TEXTRSP(authors)
    TEXTRSP(platforms)
    #undef TEXTRSP
    
    if (response.x_correlation_id==dbs->daterange_correlation_id) {
      dbs->daterange_correlation_id=0;
      dbs_set_daterange_json(dbs,response.body,response.bodyc);
      return;
    }
    
    int i=dbs->listenerc;
    while (i-->0) {
      struct dbs_listener *listener=dbs->listenerv+i;
      if (listener->corrid!=response.x_correlation_id) continue;
      struct dbs_listener swap=*listener;
      dbs->listenerc--;
      memmove(listener,listener+1,sizeof(struct dbs_listener)*(dbs->listenerc-i));
      if (swap.cb) swap.cb(&response,swap.userdata);
      return;
    }
  }
  
  // Now that we're patching in addition to getting, there will be loose uninteresting responses. No worries.
  //fprintf(stderr,"%s: Unexpected HTTP response:\n%.*s\n",__func__,srcc,src);
}

/* Encode search request as an HTTP-over-fake-WebSocket packet.
 */
 
// Just the interesting part, inside the "query" object.
static int dbs_encode_search_query(struct sr_encoder *dst,struct db_service *dbs) {

  if (dbs->listc) {
    if (sr_encode_json_string(dst,"list",4,dbs->list,dbs->listc)<0) return -1;
  }
  if (dbs->textc) {
    if (sr_encode_json_string(dst,"text",4,dbs->text,dbs->textc)<0) return -1;
  }
  if (dbs->flagsc) {
    if (sr_encode_json_string(dst,"flags",5,dbs->flags,dbs->flagsc)<0) return -1;
  }
  if (dbs->authorc) {
    if (sr_encode_json_string(dst,"author",6,dbs->author,dbs->authorc)<0) return -1;
  }
  if (dbs->platformc) {
    if (sr_encode_json_string(dst,"platform",8,dbs->platform,dbs->platformc)<0) return -1;
  }
  if (dbs->genrec) {
    if (sr_encode_json_string(dst,"genre",5,dbs->genre,dbs->genrec)<0) return -1;
  }
  
  if (dbs->sortc) {
    if (sr_encode_json_string(dst,"sort",4,dbs->sort,dbs->sortc)<0) return -1;
  }
  if (dbs->page<1) dbs->page=1;
  if (sr_encode_json_int(dst,"page",4,dbs->page)<0) return -1;
  if (sr_encode_json_int(dst,"limit",5,DB_SERVICE_SEARCH_LIMIT)<0) return -1;
  if (sr_encode_json_string(dst,"detail",6,"blobs",5)<0) return -1;
  
  if (dbs->pubtimelo||dbs->pubtimehi) {
    char v[32];
    int vc=0;
    if (dbs->pubtimelo&&dbs->pubtimehi) vc=snprintf(v,sizeof(v),"%d..%d",dbs->pubtimelo,dbs->pubtimehi);
    else if (dbs->pubtimelo) vc=snprintf(v,sizeof(v),"%d..",dbs->pubtimelo);
    else if (dbs->pubtimehi) vc=snprintf(v,sizeof(v),"..%d",dbs->pubtimehi);
    if ((vc>0)&&(vc<sizeof(v))) {
      if (sr_encode_json_string(dst,"pubtime",7,v,vc)<0) return -1;
    }
  }
  
  if (dbs->ratinglo||dbs->ratinghi) {
    char v[32];
    int vc=0;
    if (dbs->ratinglo&&dbs->ratinghi) vc=snprintf(v,sizeof(v),"%d..%d",dbs->ratinglo,dbs->ratinghi);
    else if (dbs->ratinglo) vc=snprintf(v,sizeof(v),"%d..",dbs->ratinglo);
    else if (dbs->ratinghi) vc=snprintf(v,sizeof(v),"..%d",dbs->ratinghi);
    if ((vc>0)&&(vc<sizeof(v))) {
      if (sr_encode_json_string(dst,"rating",6,v,vc)<0) return -1;
    }
  }

  return 0;
}

// Outer framing. 
static int dbs_encode_search_request(struct sr_encoder *dst,struct db_service *dbs) {
  int topctx=sr_encode_json_object_start(dst,0,0);
  if (topctx<0) return -1;
  if (sr_encode_json_string(dst,"id",2,"http",4)<0) return -1;
  if (sr_encode_json_string(dst,"method",6,"POST",4)<0) return -1;
  if (sr_encode_json_string(dst,"path",4,"/api/query",10)<0) return -1;
  
  int hdrctx=sr_encode_json_object_start(dst,"headers",7);
  if (hdrctx<0) return -1;
  if (dbs->next_correlation_id<1) dbs->next_correlation_id=1;
  dbs->search_correlation_id=dbs->next_correlation_id++;
  if (sr_encode_json_int(dst,"X-Correlation-Id",16,dbs->search_correlation_id)<0) return -1;
  if (sr_encode_json_object_end(dst,hdrctx)<0) return -1;
  
  int qctx=sr_encode_json_object_start(dst,"query",5);
  if (qctx<0) return -1;
  if (dbs_encode_search_query(dst,dbs)<0) return -1;
  if (sr_encode_json_object_end(dst,qctx)<0) return -1;
  
  if (sr_encode_json_object_end(dst,topctx)<0) return -1;
  return 0;
}

/* Request fresh search results, from the cached form.
 */
 
void dbs_refresh_search(struct db_service *dbs) {
  
  struct fakews *fakews=eh_get_fakews();
  if (!fakews) return;
  if (!fakews_is_connected(fakews)) {
    if (fakews_connect_now(fakews)<0) {
      fprintf(stderr,"%s: Failed to connect to Romassist.\n",__func__);
      return;
    }
  }
  
  struct sr_encoder packet={0};
  if (dbs_encode_search_request(&packet,dbs)<0) {
    sr_encoder_cleanup(&packet);
    fprintf(stderr,"%s: Failed to assemble search request.\n",__func__);
    return;
  }
  //fprintf(stderr,"%s:%d:%s: Sending search request:\n%.*s\n",__FILE__,__LINE__,__func__,packet.c,packet.v);
  
  int err=fakews_send(fakews,1,packet.v,packet.c);
  sr_encoder_cleanup(&packet);
  if (err<0) {
    fprintf(stderr,"%s: Failed to send search request.\n",__func__);
  } else {
    //fprintf(stderr,"%s: Searching...\n",__func__);
    dbs_state_write(dbs);
  }
}

/* Select game.
 */
 
void dbs_select_game(struct db_service *dbs,int gameid) {
  if (gameid==dbs->gameid) return;
  dbs->gameid=gameid;
  dbs_state_write(dbs);
}

/* Get game from list.
 */
 
int dbs_get_game_from_list(struct sr_decoder *dst,const struct db_service *dbs,int gameid) {
  struct sr_decoder decoder={.v=dbs->gamelist,.c=dbs->gamelistc};
  if (sr_decode_json_array_start(&decoder)<0) return -1;
  while (sr_decode_json_next(0,&decoder)>0) {
    int jsonctx=sr_decode_json_object_start(&decoder);
    if (jsonctx<0) {
      if (sr_decode_json_skip(&decoder)<0) return -1;
      continue;
    }
    *dst=decoder;
    const char *k;
    int kc;
    while ((kc=sr_decode_json_next(&k,&decoder))>0) {
      if ((kc==6)&&!memcmp(k,"gameid",6)) {
        int qgameid=0;
        if (sr_decode_json_int(&qgameid,&decoder)<0) return -1;
        if (qgameid==gameid) return 0; // got it!
      } else {
        if (sr_decode_json_skip(&decoder)<0) return -1;
      }
    }
    if (sr_decode_json_end(&decoder,jsonctx)<0) return -1;
  }
  return -1;
}

/* Decode game.
 */
 
int dbs_game_get(struct dbs_game *game,const struct db_service *dbs,int gameid) {
  struct sr_decoder decoder={0};
  if (dbs_get_game_from_list(&decoder,dbs,gameid)<0) return -1;
  return dbs_game_decode(game,decoder.v+decoder.p,decoder.c);
}

int dbs_game_decode(struct dbs_game *game,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  memset(game,0,sizeof(struct dbs_game));
  if (!srcc) return 0; // edge case, i'm calling empty valid because "{}" would be too.
  struct sr_decoder decoder={.v=src,.c=srcc,.jsonctx='{'};
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
  
    if ((kc==6)&&!memcmp(k,"gameid",6)) {
      if (sr_decode_json_int(&game->gameid,&decoder)<0) return -1;
      continue;
    }
    
    if ((kc==6)&&!memcmp(k,"rating",6)) {
      if (sr_decode_json_int(&game->rating,&decoder)<0) return -1;
      continue;
    }
    
    if ((kc==7)&&!memcmp(k,"pubtime",7)) {
      // Be careful here. pubtime is usually just the year, but could be a full ISO 8601 date, in which case we only want the first four digits.
      char tmp[32];
      int tmpc=sr_decode_json_string(tmp,sizeof(tmp),&decoder);
      if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
      if (tmpc>4) tmpc=4;
      else if (!tmpc) { tmp[0]='0'; tmpc=1; }
      sr_int_eval(&game->pubtime,tmp,tmpc);
      continue;
    }
    
    // Everything else is a straight JSON token.
    #define _(tag) if ((kc==sizeof(#tag)-1)&&!memcmp(k,#tag,kc)) { \
      if ((game->tag##c=sr_decode_json_expression(&game->tag,&decoder))<0) return -1; \
      continue; \
    }
    _(name)
    _(platform)
    _(author)
    _(genre)
    _(flags)
    _(path)
    _(comments)
    _(lists)
    _(blobs)
    #undef _
    
    if (sr_decode_json_skip(&decoder)<0) return -1;
  }
  return 0;
}

/* Patch game.
 */
 
static int dbs_encode_blank_patch(struct sr_encoder *dst,struct db_service *dbs,int gameid) {
  struct fakews *fakews=eh_get_fakews();
  if (!fakews) return -1;
  if (!fakews_is_connected(fakews)) {
    if (fakews_connect_now(fakews)<0) {
      fprintf(stderr,"%s: Failed to connect to Romassist.\n",__func__);
      return -1;
    }
  }
  if (sr_encode_json_object_start(dst,0,0)!=0) return -1;
  if (sr_encode_json_string(dst,"id",2,"http",4)<0) return -1;
  if (sr_encode_json_string(dst,"method",6,"PATCH",5)<0) return -1;
  if (sr_encode_json_string(dst,"path",4,"/api/game?detail=id",19)<0) return -1;
  if (sr_encode_json_object_start(dst,"body",4)!='{') return -1;
  if (sr_encode_json_int(dst,"gameid",6,gameid)<0) return -1;
  return 0;
}

static int dbs_finish_patch(struct sr_encoder *dst) {
  if (sr_encode_json_object_end(dst,'{')<0) return -1;
  if (sr_encode_json_object_end(dst,0)<0) return -1;
  return 0;
}
 
int dbs_replace_game_field_string(struct db_service *dbs,int gameid,const char *k,int kc,const char *v,int vc) {
  struct sr_encoder encoder={0};
  if (
    (dbs_encode_blank_patch(&encoder,dbs,gameid)<0)||
    (sr_encode_json_string(&encoder,k,kc,v,vc)<0)||
    (dbs_finish_patch(&encoder)<0)
  ) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  int err=fakews_send(eh_get_fakews(),1,encoder.v,encoder.c);
  sr_encoder_cleanup(&encoder);
  if (err<0) return err;
  dbs_refresh_search(dbs);
  return 0;
}

int dbs_replace_game_field_int(struct db_service *dbs,int gameid,const char *k,int kc,int v) {
  struct sr_encoder encoder={0};
  if (
    (dbs_encode_blank_patch(&encoder,dbs,gameid)<0)||
    (sr_encode_json_int(&encoder,k,kc,v)<0)||
    (dbs_finish_patch(&encoder)<0)
  ) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  int err=fakews_send(eh_get_fakews(),1,encoder.v,encoder.c);
  sr_encoder_cleanup(&encoder);
  if (err<0) return err;
  dbs_refresh_search(dbs);
  return 0;
}

/* Create list.
 */
 
static int dbs_create_list_inner(struct sr_encoder *encoder,struct db_service *dbs,const char *name,int namec) {
  struct fakews *fakews=eh_get_fakews();
  if (!fakews) return -1;
  if (!fakews_is_connected(fakews)) {
    if (fakews_connect_now(fakews)<0) {
      fprintf(stderr,"%s: Failed to connect to Romassist.\n",__func__);
      return -1;
    }
  }
  if (sr_encode_json_object_start(encoder,0,0)!=0) return -1;
  if (sr_encode_json_string(encoder,"id",2,"http",4)<0) return -1;
  if (sr_encode_json_string(encoder,"method",6,"PUT",3)<0) return -1;
  if (sr_encode_json_string(encoder,"path",4,"/api/list",-1)<0) return -1;
  int bctx=sr_encode_json_object_start(encoder,"body",4);
  if (bctx<0) return -1;
  if (sr_encode_json_string(encoder,"name",4,name,namec)<0) return -1;
  if (sr_encode_json_object_end(encoder,bctx)<0) return -1;
  if (sr_encode_json_object_end(encoder,0)<0) return -1;
  return fakews_send(fakews,1,encoder->v,encoder->c);
}
 
int dbs_create_list(struct db_service *dbs,const char *name,int namec) {
  struct sr_encoder encoder={0};
  int err=dbs_create_list_inner(&encoder,dbs,name,namec);
  sr_encoder_cleanup(&encoder);
  if (err>=0) dbs_refresh_lists(dbs);
  return err;
}

/* Add/remove game to/from list.
 */
 
static int dbs_modify_list(struct sr_encoder *encoder,struct db_service *dbs,const char *path,int gameid,const char *listid,int listidc) {
  struct fakews *fakews=eh_get_fakews();
  if (!fakews) return -1;
  if (!fakews_is_connected(fakews)) {
    if (fakews_connect_now(fakews)<0) {
      fprintf(stderr,"%s: Failed to connect to Romassist.\n",__func__);
      return -1;
    }
  }
  if (sr_encode_json_object_start(encoder,0,0)!=0) return -1;
  if (sr_encode_json_string(encoder,"id",2,"http",4)<0) return -1;
  if (sr_encode_json_string(encoder,"method",6,"POST",4)<0) return -1;
  if (sr_encode_json_string(encoder,"path",4,path,-1)<0) return -1;
  int qctx=sr_encode_json_object_start(encoder,"query",5);
  if (qctx<0) return -1;
  if (sr_encode_json_int(encoder,"gameid",6,gameid)<0) return -1;
  if (sr_encode_json_string(encoder,"listid",6,listid,listidc)<0) return -1;
  if (sr_encode_json_object_end(encoder,qctx)<0) return -1;
  if (sr_encode_json_object_end(encoder,0)<0) return -1;
  return fakews_send(fakews,1,encoder->v,encoder->c);
}
 
int dbs_add_to_list(struct db_service *dbs,int gameid,const char *listid,int listidc) {
  struct sr_encoder encoder={0};
  int err=dbs_modify_list(&encoder,dbs,"/api/list/add",gameid,listid,listidc);
  sr_encoder_cleanup(&encoder);
  if (err>=0) dbs_refresh_search(dbs);
  return err;
}

int dbs_remove_from_list(struct db_service *dbs,int gameid,const char *listid,int listidc) {
  struct sr_encoder encoder={0};
  int err=dbs_modify_list(&encoder,dbs,"/api/list/remove",gameid,listid,listidc);
  sr_encoder_cleanup(&encoder);
  if (err>=0) dbs_refresh_search(dbs);
  return err;
}

/* General request with callback.
 */
 
int dbs_request_http(
  struct db_service *dbs,
  const char *method,const char *path,
  void (*cb)(struct eh_http_response *rsp,void *userdata),
  void *userdata
) {
  struct fakews *fakews=eh_get_fakews();
  if (!fakews) return -1;
  if (!fakews_is_connected(fakews)) {
    if (fakews_connect_now(fakews)<0) {
      fprintf(stderr,"%s: Failed to connect to Romassist.\n",__func__);
      return -1;
    }
  }
  
  if (dbs->listenerc>=dbs->listenera) {
    int na=dbs->listenera+16;
    if (na>INT_MAX/sizeof(struct dbs_listener)) return -1;
    void *nv=realloc(dbs->listenerv,sizeof(struct dbs_listener)*na);
    if (!nv) return -1;
    dbs->listenerv=nv;
    dbs->listenera=na;
  }
  struct dbs_listener *listener=dbs->listenerv+dbs->listenerc++;
  memset(listener,0,sizeof(struct dbs_listener));
  if (dbs->next_correlation_id<1) dbs->next_correlation_id=1;
  listener->corrid=dbs->next_correlation_id++;
  listener->cb=cb;
  listener->userdata=userdata;
  
  struct sr_encoder encoder={0};
  sr_encode_json_object_start(&encoder,0,0);
  sr_encode_json_string(&encoder,"id",2,"http",4);
  sr_encode_json_string(&encoder,"method",6,method,-1);
  sr_encode_json_string(&encoder,"path",4,path,-1);
  int hdrctx=sr_encode_json_object_start(&encoder,"headers",7);
  sr_encode_json_int(&encoder,"X-Correlation-Id",16,listener->corrid);
  sr_encode_json_object_end(&encoder,hdrctx);
  if (sr_encode_json_object_end(&encoder,0)<0) {
    sr_encoder_cleanup(&encoder);
    dbs->listenerc--;
    return -1;
  }
  
  int err=fakews_send(fakews,1,encoder.v,encoder.c);
  sr_encoder_cleanup(&encoder);
  if (err<0) {
    dbs->listenerc--;
    return err;
  }
  
  return listener->corrid;
}

void dbs_cancel_request(struct db_service *dbs,int corrid) {
  int i=dbs->listenerc;
  while (i-->0) {
    struct dbs_listener *listener=dbs->listenerv+i;
    if (listener->corrid!=corrid) continue;
    dbs->listenerc--;
    memmove(listener,listener+1,sizeof(struct dbs_listener)*(dbs->listenerc-i));
  }
}

/* Launch game by id.
 */
 
int dbs_launch(struct db_service *dbs,int gameid) {
  char str[32];
  int strc=snprintf(str,sizeof(str),"%d",gameid);
  if ((strc<1)||(strc>=sizeof(str))) return -1;
  return eh_request_http("POST","/api/launch","gameid",str);
}

/* Metadata calls.
 */
 
void dbs_refresh_all_metadata(struct db_service *dbs) {
  dbs_refresh_lists(dbs);
  dbs_refresh_genres(dbs);
  dbs_refresh_authors(dbs);
  dbs_refresh_platforms(dbs);
  dbs_refresh_daterange(dbs);
}

static int dbs_encode_metadata_request(struct sr_encoder *dst,struct db_service *dbs,const char *path,int corrid) {
  int topctx=sr_encode_json_object_start(dst,0,0);
  if (topctx<0) return -1;
  if (sr_encode_json_string(dst,"id",2,"http",4)<0) return -1;
  if (sr_encode_json_string(dst,"method",6,"GET",3)<0) return -1;
  if (sr_encode_json_string(dst,"path",4,path,-1)<0) return -1;
  if (corrid) {
    int hdrctx=sr_encode_json_object_start(dst,"headers",7);
    if (hdrctx<0) return -1;
    if (sr_encode_json_int(dst,"X-Correlation-Id",16,corrid)<0) return -1;
    if (sr_encode_json_object_end(dst,hdrctx)<0) return -1;
  }
  if (sr_encode_json_object_end(dst,topctx)<0) return -1;
  return 0;
}

static void dbs_refresh_metadata_1(struct db_service *dbs,const char *path,int *corrid) {
  struct fakews *fakews=eh_get_fakews();
  if (!fakews) return;
  if (!fakews_is_connected(fakews)) {
    if (fakews_connect_now(fakews)<0) {
      fprintf(stderr,"%s: Failed to connect to Romassist.\n",__func__);
      return;
    }
  }
  if (dbs->next_correlation_id<1) dbs->next_correlation_id=1;
  *corrid=dbs->next_correlation_id++;
  struct sr_encoder packet={0};
  if (dbs_encode_metadata_request(&packet,dbs,path,*corrid)<0) {
    sr_encoder_cleanup(&packet);
    return;
  }
  int err=fakews_send(fakews,1,packet.v,packet.c);
  sr_encoder_cleanup(&packet);
}

void dbs_refresh_lists(struct db_service *dbs) {
  dbs_refresh_metadata_1(dbs,"/api/listids",&dbs->lists_correlation_id);
}

void dbs_refresh_genres(struct db_service *dbs) {
  dbs_refresh_metadata_1(dbs,"/api/meta/genre",&dbs->genres_correlation_id);
}

void dbs_refresh_authors(struct db_service *dbs) {
  dbs_refresh_metadata_1(dbs,"/api/meta/author",&dbs->authors_correlation_id);
}

void dbs_refresh_platforms(struct db_service *dbs) {
  dbs_refresh_metadata_1(dbs,"/api/meta/platform",&dbs->platforms_correlation_id);
}

void dbs_refresh_daterange(struct db_service *dbs) {
  // Doesn't matter that the response format is unlike other /meta/ calls.
  dbs_refresh_metadata_1(dbs,"/api/meta/daterange",&dbs->daterange_correlation_id);
}

/* Set search criteria.
 */
 
static int dbs_set_string(char **v,int *c,struct db_service *dbs,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (*v) free(*v);
  *v=nv;
  *c=srcc;
  dbs->page=1;
  dbs->gameid=0;
  return 0;
}
 
int dbs_search_set_list(struct db_service *dbs,const char *v,int c) {
  dbs->last_search_interaction=DB_SERVICE_INTERACTION_list;
  return dbs_set_string(&dbs->list,&dbs->listc,dbs,v,c);
}
int dbs_search_set_text(struct db_service *dbs,const char *v,int c) { 
  dbs->last_search_interaction=DB_SERVICE_INTERACTION_other;
  return dbs_set_string(&dbs->text,&dbs->textc,dbs,v,c);
}
int dbs_search_set_flags(struct db_service *dbs,const char *v,int c) { 
  dbs->last_search_interaction=DB_SERVICE_INTERACTION_other;
  return dbs_set_string(&dbs->flags,&dbs->flagsc,dbs,v,c);
}
int dbs_search_set_author(struct db_service *dbs,const char *v,int c) { 
  dbs->last_search_interaction=DB_SERVICE_INTERACTION_other;
  return dbs_set_string(&dbs->author,&dbs->authorc,dbs,v,c);
}
int dbs_search_set_genre(struct db_service *dbs,const char *v,int c) { 
  dbs->last_search_interaction=DB_SERVICE_INTERACTION_other;
  return dbs_set_string(&dbs->genre,&dbs->genrec,dbs,v,c);
}
int dbs_search_set_platform(struct db_service *dbs,const char *v,int c) { 
  dbs->last_search_interaction=DB_SERVICE_INTERACTION_other;
  return dbs_set_string(&dbs->platform,&dbs->platformc,dbs,v,c);
}
int dbs_search_set_sort(struct db_service *dbs,const char *v,int c) {
  dbs->last_search_interaction=DB_SERVICE_INTERACTION_other;
  return dbs_set_string(&dbs->sort,&dbs->sortc,dbs,v,c);
}

int dbs_search_set_pubtime(struct db_service *dbs,int lo,int hi) {
  dbs->last_search_interaction=DB_SERVICE_INTERACTION_other;
  dbs->pubtimelo=lo;
  dbs->pubtimehi=hi;
  dbs->page=1;
  dbs->gameid=0;
  return 0;
}

int dbs_search_set_rating(struct db_service *dbs,int lo,int hi) {
  dbs->last_search_interaction=DB_SERVICE_INTERACTION_other;
  dbs->ratinglo=lo;
  dbs->ratinghi=hi;
  dbs->page=1;
  dbs->gameid=0;
  return 0;
}
