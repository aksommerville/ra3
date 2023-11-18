#include "mn_internal.h"
#include "opt/serial/serial.h"

/* Cleanup.
 */
 
void dbs_cleanup(struct db_service *dbs) {
  if (dbs->gamelist) free(dbs->gamelist);
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

/* HTTP response.
 */
 
void dbs_http_response(struct db_service *dbs,const char *src,int srcc) {
  struct eh_http_response response={0};
  if (eh_http_response_split(&response,src,srcc)<0) return;
  if (response.status!=200) return;
  
  // If we get an array, assume it's a game list. TODO Eventually we'll need request/response correlation.
  if ((response.bodyc>=2)&&(response.body[0]=='[')&&(response.body[response.bodyc-1]==']')) {
    if (dbs_set_game_list_json(dbs,response.body,response.bodyc)<0) return;
  }
}

/* Request fresh search results, from the cached form.
 */
 
void dbs_refresh_search(struct db_service *dbs) {
  if (eh_request_http("POST","/api/query","text","drag","detail","blobs")<0) {
    fprintf(stderr,"%s: request failed\n",__func__);
  } else {
    fprintf(stderr,"%s: request sent\n",__func__);
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
