#include "mn_internal.h"
#include "opt/serial/serial.h"
#include "opt/fakews/fakews.h"

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
  // Only thing we're interested in is "X-Page-Count". If we don't find it, default to 1.
  dbs->pagec=1;
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_object_start(&decoder)>=0) {
    const char *k;
    int kc;
    while ((kc=sr_decode_json_next(&k,&decoder))>0) {
      if ((kc==12)&&!memcmp(k,"X-Page-Count",12)) {
        int v=0;
        if (sr_decode_json_int(&v,&decoder)>=0) dbs->pagec=v;
        break;
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

/* HTTP response.
 */
 
void dbs_http_response(struct db_service *dbs,const char *src,int srcc) {
  struct eh_http_response response={0};
  if (eh_http_response_split(&response,src,srcc)<0) return;
  if (response.status!=200) return;
  
  if (response.x_correlation_id) {
    if (response.x_correlation_id==dbs->search_correlation_id) {
      dbs->search_correlation_id=0;
      dbs_set_game_list_json(dbs,response.body,response.bodyc);
      dbs_receive_game_list_headers(dbs,response.headers,response.headersc);
      return;
    }
  }
  
  fprintf(stderr,"%s: Unexpected HTTP response:\n%.*s\n",__func__,srcc,src);
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
  }
}

/* Select game.
 */
 
void dbs_select_game(struct db_service *dbs,int gameid) {
  if (gameid==dbs->gameid) return;
  dbs->gameid=gameid;
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

/* Launch game by id.
 */
 
int dbs_launch(struct db_service *dbs,int gameid) {
  char str[32];
  int strc=snprintf(str,sizeof(str),"%d",gameid);
  if ((strc<1)||(strc>=sizeof(str))) return -1;
  return eh_request_http("POST","/api/launch","gameid",str);
}
