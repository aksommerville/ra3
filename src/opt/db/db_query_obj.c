#include "db_internal.h"
#include "opt/serial/serial.h"
 
struct db_query {
  struct db *db;
  int empty; // nonzero if we know the result is empty without checking
  
  char *text;
  int textc;
  
  uint32_t platform,author,genre;
  uint32_t flags,notflags;
  uint32_t ratinglo,ratinghi;
  uint32_t pubtimelo,pubtimehi;
  
  int own_input; // nonzero if (input) is a non-resident list that we own.
  struct db_list *input; // Search space or null for everything. Non-resident or resident! Check own_input.
  struct db_list *results; // Output. Non-resident.

  int sort,descend;
  int detail; // DB_DETAIL_*
  int limit;
  int pagep; // 1-based
  int pagec;
  int totalc;
};

/* Delete.
 */
 
void db_query_del(struct db_query *query) {
  if (!query) return;
  if (query->text) free(query->text);
  if (query->own_input) db_list_del(query->input);
  db_list_del(query->results);
  free(query);
}

/* New.
 */
 
struct db_query *db_query_new(struct db *db) {
  struct db_query *query=calloc(1,sizeof(struct db_query));
  if (!query) return 0;
  query->db=db;
  query->ratinghi=UINT32_MAX;
  query->pubtimehi=UINT32_MAX;
  query->sort=DB_SORT_none;
  query->detail=DB_DETAIL_record;
  query->limit=100;
  query->pagep=1;
  return query;
}

/* Split "LO..HI" strings.
 * In the single case, we put the same string in both.
 */
 
static int db_split_range(void *lopp,int *loc,void *hipp,int *hic,const char *src,int srcc) {
  const char *lo=src;
  *(const void**)lopp=lo;
  (*loc)=0;
  int srcp=0;
  while ((srcp<srcc)&&(src[srcp]!='.')) { srcp++; (*loc)++; }
  if (srcp>=srcc) {
    *(const void**)hipp=lo;
    *hic=*loc;
    return 0;
  }
  if ((srcp>srcc-2)||(src[srcp]!='.')||(src[srcp+1]!='.')) return -1;
  srcp+=2;
  *(const void**)hipp=src+srcp;
  *hic=srcc-srcp;
  return 0;
}

/* Add parameter.
 */
 
int db_query_add_parameter(const char *k,int kc,const char *v,int vc,void *_query) {
  struct db_query *query=_query;
  if (query->empty) return 1;
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  
  /* Values could be URL-encoded. These are expected to come straight off http_xfer_for_query, which doesn't decode.
   * We'll enforce a length limit too. "text" and "list" aren't intrisically limited, but say 256? That's plenty long.
   * Doesn't matter for keys: The keys we accept are all letters only.
   */
  char vv[256];
  int vvc=sr_url_decode(vv,sizeof(vv),v,vc);
  if ((vvc>=0)&&(vvc<=sizeof(vv))) {
    v=vv;
    vc=vvc;
  } // If it fails, meh, try it encoded. (maybe it's only a length thing, or maybe it's already decoded and contains '%')
  
  if ((kc==4)&&!memcmp(k,"text",4)) {
    if (query->text) return -1;
    if (!vc) return 0;
    if (!(query->text=malloc(vc+1))) return -1;
    memcpy(query->text,v,vc);
    query->text[vc]=0;
    query->textc=vc;
    return 0;
  }
  
  if ((kc==4)&&!memcmp(k,"list",4)) {
    if (!vc) return 0; // Empty arguments are fine, pretend we didn't see them.
    struct db_list *list=db_list_get_by_string(query->db,v,vc);
    if (!list) {
      query->empty=1;
      return 1;
    }
    if (query->input) {
      struct db_list *combined=db_query_list_and(query->db,query->input,list);
      if (!combined) return -1;
      if (query->own_input) db_list_del(query->input);
      query->input=combined;
      query->own_input=1;
    } else {
      query->input=list;
      query->own_input=0;
    }
    return 0;
  }
  
  if ((kc==8)&&!memcmp(k,"platform",8)) {
    if (!vc) return 0;
    if (query->platform) return -1;
    if (!(query->platform=db_string_lookup(query->db,v,vc))) { query->empty=1; return 1; }
    return 0;
  }
  
  if ((kc==6)&&!memcmp(k,"author",6)) {
    if (!vc) return 0;
    if (query->author) return -1;
    if (!(query->author=db_string_lookup(query->db,v,vc))) { query->empty=1; return 1; }
    return 0;
  }
  
  if ((kc==5)&&!memcmp(k,"genre",5)) {
    if (!vc) return 0;
    if (query->genre) return -1;
    if (!(query->genre=db_string_lookup(query->db,v,vc))) { query->empty=1; return 1; }
    return 0;
  }
  
  if ((kc==5)&&!memcmp(k,"flags",5)) {
    if (query->flags||(db_flags_eval(&query->flags,v,vc)<0)) return -1;
    if (query->flags&query->notflags) { query->empty=1; return 1; }
    return 0;
  }
  
  if ((kc==8)&&!memcmp(k,"notflags",8)) {
    if (query->notflags||(db_flags_eval(&query->notflags,v,vc)<0)) return -1;
    if (query->flags&query->notflags) { query->empty=1; return 1; }
    return 0;
  }
  
  if ((kc==6)&&!memcmp(k,"rating",6)) {
    const char *lo=0,*hi=0;
    int loc=0,hic=0,n;
    if (db_split_range(&lo,&loc,&hi,&hic,v,vc)<0) return -1;
    if (loc&&hic) {
      if (sr_int_eval(&n,lo,loc)<2) return -1; query->ratinglo=n;
      if (sr_int_eval(&n,hi,hic)<2) return -1; query->ratinghi=n;
    } else if (loc) {
      if (sr_int_eval(&n,lo,loc)<2) return -1; query->ratinglo=n;
    } else if (hic) {
      if (sr_int_eval(&n,hi,hic)<2) return -1; query->ratinghi=n;
    }
    return 0;
  }
  
  if ((kc==7)&&!memcmp(k,"pubtime",7)) {
    const char *lo=0,*hi=0;
    int loc=0,hic=0;
    if (db_split_range(&lo,&loc,&hi,&hic,v,vc)<0) return -1;
    if (loc&&hic) {
      query->pubtimelo=db_time_eval(lo,loc);
      query->pubtimehi=db_time_eval_upper(hi,hic);
    } else if (loc) {
      query->pubtimelo=db_time_eval(lo,loc);
    } else if (hic) {
      query->pubtimehi=db_time_eval_upper(hi,hic);
    }
    return 0;
  }
  
  if ((kc==4)&&!memcmp(k,"sort",4)) {
    if ((query->sort=db_sort_eval(&query->descend,v,vc))<0) return -1;
    return 0;
  }
  
  if ((kc==6)&&!memcmp(k,"detail",6)) {
    if (!vc) return 0;
    #define _(tag) if ((vc==sizeof(#tag)-1)&&!memcmp(v,#tag,vc)) { query->detail=DB_DETAIL_##tag; return 0; }
    DB_DETAIL_FOR_EACH
    #undef _
    return -1;
  }
  
  if ((kc==5)&&!memcmp(k,"limit",5)) {
    if (sr_int_eval(&query->limit,v,vc)<2) return -1;
    if (query->limit<1) query->limit=1;
    return 0;
  }
  
  if ((kc==4)&&!memcmp(k,"page",4)) {
    if (sr_int_eval(&query->pagep,v,vc)<2) return -1;
    return 0;
  }
  
  // Tolerate unexpected parameters.
  return 0;
}

/* Should we do the header query?
 */
 
static int db_query_needs_header_search(const struct db_query *query) {
  if (query->platform) return 1;
  if (query->author) return 1;
  if (query->genre) return 1;
  if (query->flags) return 1;
  if (query->notflags) return 1;
  if (query->ratinglo!=0) return 1;
  if (query->ratinghi!=UINT32_MAX) return 1;
  if (query->pubtimelo!=0) return 1;
  if (query->pubtimehi!=UINT32_MAX) return 1;
  return 0;
}

/* Search, sort, truncate, and encode.
 */
 
int db_query_finish(struct sr_encoder *dst,struct db_query *query) {
  // Not sure why anyone would, but if they call finish twice, regurgitate the same results.
  if (!query->results&&query->empty) {
    if (!(query->results=db_list_copy_nonresident(0))) return -1;
    query->totalc=0;
    query->pagec=1;
    return dst?sr_encode_raw(dst,"[]",2):0;
  }
  if (!query->results) {
  
    // Header search first if applicable; it's cheaper. List searches are performed implicitly already.
    if (db_query_needs_header_search(query)) {
      if (!(query->results=db_query_header_prelookupped(
        query->db,query->input,
        query->platform,query->author,query->genre,
        query->flags,query->notflags,
        query->ratinglo,query->ratinghi,
        query->pubtimelo,query->pubtimehi
      ))) return -1;
    } else if (query->input) {
      if (!(query->results=db_list_copy_nonresident(query->input))) return -1;
    }
    
    // Text search last.
    if (query->textc) {
      struct db_list *nresults=db_query_text(query->db,query->results,query->text,query->textc);
      if (!nresults) return -1;
      db_list_del(query->results);
      query->results=nresults;
    }
    
    // If none of the three options was invoked, return everything.
    if (!query->results) {
      if (!(query->results=db_list_everything(query->db))) return -1;
    }
    
    // Sort.
    if (db_list_sort_auto(query->db,query->results,query->sort,query->descend)<0) return -1;
    
    // Paginate.
    query->totalc=query->results->gameidc;
    query->pagec=db_list_paginate(query->results,query->limit,query->pagep-1);
  }
  return dst?db_list_encode_array(dst,query->db,query->results,DB_FORMAT_json,query->detail):0;
}

/* Trivial accessors.
 */

int db_query_get_page_count(const struct db_query *query) {
  return query->pagec;
}

int db_query_get_total_count(const struct db_query *query) {
  return query->totalc;
}

struct db_list *db_query_get_results(const struct db_query *query) {
  return query->results;
}
