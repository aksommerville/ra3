#include "http.h"
#include "http_dict.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Cleanup.
 */
 
static void http_dict_entry_cleanup(struct http_dict_entry *entry) {
  if (entry->k) free(entry->k);
  if (entry->v) free(entry->v);
}
 
void http_dict_cleanup(struct http_dict *dict) {
  if (!dict) return;
  if (dict->v) {
    while (dict->c-->0) http_dict_entry_cleanup(dict->v+dict->c);
    free(dict->v);
  }
  memset(dict,0,sizeof(struct http_dict));
}

/* Get.
 */

int http_dict_get(void *dstpp,const struct http_dict *dict,const char *k,int kc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  const struct http_dict_entry *entry=dict->v;
  int i=dict->c;
  for (;i-->0;entry++) {
    if (entry->kc!=kc) continue;
    if (http_memcasecmp(k,entry->k,kc)) continue;
    *(void**)dstpp=entry->v;
    return entry->vc;
  }
  return -1;
}

/* Set.
 */
 
int http_dict_set(struct http_dict *dict,const char *k,int kc,const char *v,int vc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  
  // Replace if we already have it.
  {
    struct http_dict_entry *entry=dict->v;
    int i=dict->c;
    for (;i-->0;entry++) {
      if (entry->kc!=kc) continue;
      if (http_memcasecmp(entry->k,k,kc)) continue;
      char *nv=malloc(vc+1);
      if (!nv) return -1;
      memcpy(nv,v,vc);
      nv[vc]=0;
      if (entry->v) free(entry->v);
      entry->v=nv;
      entry->vc=vc;
      return 0;
    }
  }
  
  // Append.
  if (dict->c>=dict->a) {
    int na=dict->a+16;
    if (na>INT_MAX/sizeof(struct http_dict_entry)) return -1;
    void *nv=realloc(dict->v,sizeof(struct http_dict_entry)*na);
    if (!nv) return -1;
    dict->v=nv;
    dict->a=na;
  }
  char *nk=malloc(kc+1);
  if (!nk) return -1;
  memcpy(nk,k,kc);
  nk[kc]=0;
  char *nv=malloc(vc+1);
  if (!nv) { free(nk); return -1; }
  memcpy(nv,v,vc);
  nv[vc]=0;
  struct http_dict_entry *entry=dict->v+dict->c++;
  entry->k=nk;
  entry->kc=kc;
  entry->v=nv;
  entry->vc=vc;
  return 0;
}

/* Clear.
 */
 
void http_dict_clear(struct http_dict *dict) {
  while (dict->c>0) {
    dict->c--;
    http_dict_entry_cleanup(dict->v+dict->c);
  }
}
