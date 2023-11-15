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
