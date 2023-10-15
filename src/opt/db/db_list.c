#define _GNU_SOURCE 1
#include "db_internal.h"
#include "opt/serial/serial.h"

/* Delete list.
 */
 
void db_list_del(struct db_list *list) {
  if (!list) return;
  if (list->gameidv) free(list->gameidv);
  if (list->gamev) free(list->gamev);
  free(list);
}

/* Get list from store.
 */

int db_list_count(const struct db *db) {
  return db->lists.c;
}

struct db_list *db_list_get_by_index(const struct db *db,int p) {
  if ((p<0)||(p>=db->lists.c)) return 0;
  return db->lists.v[p];
}

struct db_list *db_list_get_by_id(const struct db *db,uint32_t listid) {
  int p=db_liststore_search(&db->lists,listid);
  if (p<0) return 0;
  return db->lists.v[p];
}

/* Delete from store.
 */

int db_list_delete(struct db *db,uint32_t listid) {
  int p=db_liststore_search(&db->lists,listid);
  if (p<0) return 0;
  db_liststore_remove(&db->lists,p);
  db->dirty=1;
  return 0;
}

/* Copy to new nonresident list.
 */
 
struct db_list *db_list_copy_nonresident(const struct db_list *src) {
  struct db_list *list=calloc(1,sizeof(struct db_list));
  if (!list) return 0;
  if (src) memcpy(list,src,sizeof(struct db_list));
  list->listid=0;
  list->gameidv=0;
  list->gameidc=list->gameida=0;
  list->gamev=0;
  list->gamec=list->gamea=0;
  if (src&&src->gameidc) {
    if (!(list->gameidv=malloc(sizeof(uint32_t)*src->gameidc))) {
      free(list);
      return 0;
    }
    memcpy(list->gameidv,src->gameidv,sizeof(uint32_t)*src->gameidc);
    list->gameidc=list->gameida=src->gameidc;
  }
  return list;
}

/* Copy to new resident list.
 */
 
struct db_list *db_list_copy_resident(struct db *db,const struct db_list *src) {
  struct db_list *list=db_list_insert(db,0);
  if (!list) return 0;
  if (src) {
    list->name=src->name;
    list->desc=src->desc;
    list->sorted=src->sorted;
    if (src->gameidc) {
      if (!(list->gameidv=malloc(sizeof(uint32_t)*src->gameidc))) {
        db_list_delete(db,list->listid);
        return 0;
      }
      memcpy(list->gameidv,src->gameidv,sizeof(uint32_t)*src->gameidc);
      list->gameidc=list->gameida=src->gameidc;
    }
  }
  return list;
}

/* Insert new list.
 */
 
struct db_list *db_list_insert(struct db *db,uint32_t listid) {
  if (!listid) listid=db_liststore_next_id(&db->lists);
  int p=db_liststore_search(&db->lists,listid);
  if (p>=0) return 0;
  p=-p-1;
  struct db_list *list=db_liststore_insert(&db->lists,p,listid);
  if (!list) return 0;
  db->lists.dirty=1;
  db->dirty=1;
  return list;
}

/* Decode gameid from a JSON array.
 * It's either a straight integer, or an object containing "id".
 */
 
static int db_decode_gameid_from_json(uint32_t *dst,struct sr_decoder *decoder) {
  if (sr_decode_json_peek(decoder)=='#') {
    int n=0;
    if (sr_decode_json_int(&n,decoder)<0) return -1;
    *dst=n;
    return 0;
  }
  int jsonctx=sr_decode_json_object_start(decoder);
  if (jsonctx<0) return -1;
  const char *k;
  int kc,id=0;
  while ((kc=sr_decode_json_next(&k,decoder))>0) {
    if (id||(kc!=2)||memcmp(k,"id",2)) {
      if (sr_decode_json_skip(decoder)<0) return -1;
    } else {
      if (sr_decode_json_int(&id,decoder)<0) return -1;
      if (!id) return -1;
    }
  }
  if (sr_decode_json_end(decoder,jsonctx)<0) return -1;
  if (!id) return -1;
  *dst=id;
  return 0;
}

/* Patch from JSON.
 */
 
int db_list_patch_json(struct db *db,struct db_list *list,const char *src,int srcc) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  int jsonctx=sr_decode_json_object_start(&decoder);
  if (jsonctx<0) return -1;
  struct db_list incoming={0},mask={0};
  struct sr_decoder dgames={0};
  const char *k=0;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
  
    if ((kc==2)&&!memcmp(k,"id",2)) {
      int id;
      if (sr_decode_json_int(&id,&decoder)<0) return -1;
      if (list&&list->listid&&(list->listid!=id)) return -1;
      incoming.listid=id;
      mask.listid=1;
      continue;
    }
    
    if ((kc==4)&&!memcmp(k,"name",4)) {
      if (db_decode_json_string(&incoming.name,db,&decoder)<0) return -1;
      mask.name=1;
      continue;
    }
    
    if ((kc==4)&&!memcmp(k,"desc",4)) {
      if (db_decode_json_string(&incoming.desc,db,&decoder)<0) return -1;
      mask.desc=1;
      continue;
    }
    
    if ((kc==6)&&!memcmp(k,"sorted",6)) {
      int v=sr_decode_json_boolean(&decoder);
      if (v<0) return -1;
      incoming.sorted=v;
      mask.sorted=1;
      continue;
    }
    
    if ((kc==5)&&!memcmp(k,"games",5)) {
      dgames=decoder;
      if (sr_decode_json_skip(&decoder)<0) return -1;
      mask.gameidc=1;
      continue;
    }
  
    // Ignore unknown field.
    if (sr_decode_json_skip(&decoder)<0) return -1;
  }
  if (sr_decode_json_end(&decoder,jsonctx)<0) return -1;
  
  /* If games present, process them now, into (incoming).
   */
  if (mask.gameidc) {
    if ((jsonctx=sr_decode_json_array_start(&dgames))<0) return -1;
    while (sr_decode_json_next(0,&dgames)>0) {
      uint32_t gameid=0;
      if (
        (db_decode_gameid_from_json(&gameid,&dgames)<0)||
        (db_list_append(db,&incoming,gameid)<0)
      ) {
        if (incoming.gameidv) free(incoming.gameidv);
        return -1;
      }
    }
    if (sr_decode_json_end(&dgames,jsonctx)<0) {
      if (incoming.gameidv) free(incoming.gameidv);
      return -1;
    }
  }
  
  /* List not provided, they are asking to create one.
   */
  if (!list) {
    if (!(list=db_list_insert(db,incoming.listid))) {
      if (incoming.gameidv) free(incoming.gameidv);
      return -1;
    }
  }
  
  /* Transfer content.
   */
  if (mask.name) list->name=incoming.name;
  if (mask.desc) list->desc=incoming.desc;
  if (mask.sorted) list->sorted=incoming.sorted;
  
  if (mask.gameidc) {
    if (incoming.gameidc>list->gameida) {
      void *nv=malloc(sizeof(uint32_t)*incoming.gameidc);
      if (!nv) {
        if (incoming.gameidv) free(incoming.gameidv);
        return -1;
      }
      if (list->gameidv) free(list->gameidv);
      list->gameidv=nv;
      list->gameida=incoming.gameida;
    }
    memcpy(list->gameidv,incoming.gameidv,sizeof(uint32_t)*incoming.gameidc);
    list->gameidc=incoming.gameidc;
    list->gamec=0;
  }
  
  if (incoming.gameidv) free(incoming.gameidv);
  
  if (db&&list->listid) {
    db->dirty=1;
    db->lists.dirty=1;
  }
  return 0;
}

/* Set fields.
 */
 
int db_list_set_name(struct db *db,struct db_list *list,const char *src,int srcc) {
  list->name=db_string_intern(db,src,srcc);
  db_list_dirty(db,list);
}

int db_list_set_desc(struct db *db,struct db_list *list,const char *src,int srcc) {
  list->desc=db_string_intern(db,src,srcc);
  db_list_dirty(db,list);
}

/* Dirty.
 */
 
void db_list_dirty(struct db *db,struct db_list *list) {
  if (!db||!list->listid) return;
  db->lists.dirty=1;
  db->dirty=1;
}

/* Search gameid.
 */
 
static int db_list_search_gameid(const struct db_list *list,uint32_t gameid) {
  if (list->sorted) {
    int lo=0,hi=list->gameidc;
    while (lo<hi) {
      int ck=(lo+hi)>>1;
           if (gameid<list->gameidv[ck]) hi=ck;
      else if (gameid>list->gameidv[ck]) lo=ck+1;
      else return ck;
    }
    return -lo-1;
  } else {
    const uint32_t *v=list->gameidv;
    int i=0;
    for (;i<list->gameidc;i++,v++) if (*v==gameid) return i;
    return -list->gameidc-1;
  }
}

/* Test gameid.
 */

int db_list_has(const struct db_list *list,uint32_t gameid) {
  return db_list_search_gameid(list,gameid)>=0;
}

/* Add to list.
 */
 
int db_list_append(struct db *db,struct db_list *list,uint32_t gameid) {
  int p=db_list_search_gameid(list,gameid);
  if (p>=0) { // Already exists. Shuffle to back if not sorted.
    if ((p<list->gameidc)&&!list->sorted) {
      list->gamec=0;
      memmove(list->gameidv+p,list->gameidv+p+1,sizeof(uint32_t)*(list->gameidc-p-1));
      list->gameidv[list->gameidc-1]=gameid;
      if (db&&list->listid) {
        db->lists.dirty=1;
        db->dirty=1;
      }
    }
    return 0;
  }
  p=-p-1;
  if (list->gameidc>=list->gameida) {
    int na=list->gameida+32;
    if (na>INT_MAX/sizeof(uint32_t)) return -1;
    void *nv=realloc(list->gameidv,sizeof(uint32_t)*na);
    if (!nv) return -1;
    list->gameidv=nv;
    list->gameida=na;
  }
  list->gamec=0;
  memmove(list->gameidv+p+1,list->gameidv+p,sizeof(uint32_t)*(list->gameidc-p));
  list->gameidc++;
  list->gameidv[p]=gameid;
  if (db&&list->listid) {
    db->lists.dirty=1;
    db->dirty=1;
  }
  return 0;
}

/* Remove game by id.
 */
 
int db_list_remove(struct db *db,struct db_list *list,uint32_t gameid) {
  int p=db_list_search_gameid(list,gameid);
  if (p<0) return -1;
  list->gameidc--;
  memmove(list->gameidv+p,list->gameidv+p+1,sizeof(uint32_t)*(list->gameidc-p));
  list->gamec=0;
  if (db&&list->listid) {
    db->lists.dirty=1;
    db->dirty=1;
  }
  return 0;
}

/* Clear list.
 */
 
int db_list_clear(struct db *db,struct db_list *list) {
  if (!list->gameidc) return 0;
  list->gameidc=0;
  list->gamec=0;
  if (db&&list->listid) {
    db->dirty=1;
    db->lists.dirty=1;
  }
  return 0;
}

/* Sort IDs and drop games.
 */
 
static int db_list_cmp_gameid_asc(const void *a,const void *b) {
  return (*(int32_t*)a)-(*(int32_t*)b);
}
 
static int db_list_cmp_gameid_desc(const void *a,const void *b) {
  return (*(int32_t*)b)-(*(int32_t*)a);
}
 
int db_list_sort_gameid_asc(struct db *db,struct db_list *list) {
  list->gamec=0;
  qsort(list->gameidv,list->gameidc,sizeof(uint32_t),db_list_cmp_gameid_asc);
  list->sorted=1;
  if (db&&list->listid) {
    db->lists.dirty=1;
    db->dirty=1;
  }
  return 0;
}

int db_list_sort_gameid_desc(struct db *db,struct db_list *list) {
  list->gamec=0;
  qsort(list->gameidv,list->gameidc,sizeof(uint32_t),db_list_cmp_gameid_desc);
  list->sorted=0;
  if (db&&list->listid) {
    db->lists.dirty=1;
    db->dirty=1;
  }
  return 0;
}

/* Generic sort.
 */
 
int db_list_sort(
  struct db *db,
  struct db_list *list,
  int (*cmp)(const struct db_game *a,const struct db_game *b,void *userdata),
  void *userdata
) {
  if (list->gameidc!=list->gamec) {
    if (db_list_gamev_populate(db,list)<0) return -1;
  }
  list->sorted=0;
  qsort_r(list->gamev,list->gamec,sizeof(struct db_game),(void*)cmp,userdata);
  uint32_t *id=list->gameidv;
  struct db_game *game=list->gamev;
  int i=list->gamec;
  for (;i-->0;id++,game++) *id=game->gameid;
  if (db&&list->listid) {
    db->lists.dirty=1;
    db->dirty=1;
  }
  return 0;
}

/* Filter in place.
 */
 
int db_list_filter(
  struct db *db,
  struct db_list *list,
  int (*filter)(const struct db_game *game,void *userdata),
  void *userdata
) {
  if (list->gameidc!=list->gamec) {
    if (db_list_gamev_populate(db,list)<0) return -1;
  }
  int i=list->gamec,rmc=0;
  struct db_game *game=list->gamev+i-1;
  for (;i-->0;game--) {
    if (!filter(game,userdata)) {
      list->gamec--;
      memmove(game,game+1,sizeof(struct db_game)*(list->gamec-i));
      list->gameidc--;
      memmove(list->gameidv+i,list->gameidv+i+1,sizeof(uint32_t)*(list->gameidc-i));
      rmc++;
    }
  }
  if (rmc&&db&&list->listid) {
    db->lists.dirty=1;
    db->dirty=1;
  }
  return 0;
}

/* Drop game list.
 */
 
void db_list_gamev_drop(struct db_list *list) {
  list->gamec=0;
}

/* Populate gamev.
 */
 
int db_list_gamev_populate(const struct db *db,struct db_list *list) {
  list->gamec=0;
  if (!list->gameidc) return 0;
  if (!db) return -1;
  if (list->gamea<list->gameidc) {
    void *nv=realloc(list->gamev,sizeof(struct db_game)*list->gameidc);
    if (!nv) return -1;
    list->gamev=nv;
    list->gamea=list->gameidc;
  }
  const uint32_t *id=list->gameidv;
  struct db_game *dst=list->gamev;
  int i=list->gameidc;
  for (;i-->0;id++,dst++) {
    const struct db_game *game=db_game_get_by_id(db,*id);
    if (!game) return -1;
    memcpy(dst,game,sizeof(struct db_game));
  }
  list->gamec=list->gameidc;
  return 0;
}

/* Encode to JSON.
 */
 
static int db_list_encode_json(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_list *list,
  int detail
) {
  int jsonctx=sr_encode_json_object_start(dst,0,0);
  if (jsonctx<0) return -1;
  
  if (sr_encode_json_int(dst,"id",2,list->listid)<0) return -1;
  if (db_encode_json_string(dst,db,"name",4,list->name)<0) return -1;
  if (db_encode_json_string(dst,db,"desc",4,list->desc)<0) return -1;
  if (sr_encode_json_boolean(dst,"sorted",6,list->sorted)<0) return -1;
  
  int listctx=sr_encode_json_array_start(dst,"games",5);
  if (listctx<0) return -1;
  const uint32_t *v=list->gameidv;
  int i=list->gameidc;
  if (detail==DB_DETAIL_id) {
    for (;i-->0;v++) {
      if (sr_encode_json_int(dst,0,0,*v)<0) return -1;
    }
  } else {
    for (;i-->0;v++) {
      const struct db_game *game=db_game_get_by_id(db,*v);
      if (!game) return -1;
      if (db_game_encode(dst,db,game,DB_FORMAT_json,detail)<0) return -1;
    }
  }
  if (sr_encode_json_array_end(dst,listctx)<0) return -1;
  
  return sr_encode_json_object_end(dst,jsonctx);
}

/* Encode, dispatch on format.
 */
 
int db_list_encode(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_list *list,
  int format,
  int detail
) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: return db_list_encode_json(dst,db,list,detail);
  }
  return -1;
}
