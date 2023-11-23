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

struct db_list *db_list_get_by_string(const struct db *db,const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  // If it's an integer, try as ID.
  int id;
  if (sr_int_eval(&id,src,srcc)>=2) {
    struct db_list *list=db_list_get_by_id(db,id);
    if (list) return list;
  }
  
  // Exact name matches only. So if the string isn't already interned, we know there's no match.
  uint32_t stringid=db_string_lookup(db,src,srcc);
  if (!stringid) return 0;
  struct db_list **list=db->lists.v;
  int i=db->lists.c;
  for (;i-->0;list++) {
    if ((*list)->name!=stringid) continue;
    return *list;
  }
  return 0;
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

/* New nonresident list with every game.
 */
 
struct db_list *db_list_everything(const struct db *db) {
  struct db_list *list=calloc(1,sizeof(struct db_list));
  if (!list) return 0;
  list->sorted=1;
  if (db->games.c>0) {
    if (!(list->gameidv=malloc(sizeof(uint32_t)*db->games.c))) {
      free(list);
      return 0;
    }
    uint32_t *dst=list->gameidv;
    const struct db_game *game=db->games.v;
    int i=db->games.c;
    for (;i-->0;dst++,game++) *dst=game->gameid;
    list->gameidc=list->gameida=db->games.c;
  }
  return list;
}

/* Insert.
 */
 
struct db_list *db_list_insert(struct db *db,const struct db_list *list) {
  uint32_t listid=list->listid;
  if (!listid) listid=db_liststore_next_id(&db->lists);
  int p=db_liststore_search(&db->lists,listid);
  if (p>=0) return 0;
  p=-p-1;
  struct db_list *real=db_liststore_insert(&db->lists,p,listid);
  if (!real) return 0;
  real->name=list->name;
  real->desc=list->desc;
  real->sorted=list->sorted;
  if (list->gameidc>0) {
    if (!(real->gameidv=malloc(sizeof(uint32_t)*list->gameidc))) {
      db_liststore_remove(&db->lists,p);
      return 0;
    }
    memcpy(real->gameidv,list->gameidv,sizeof(uint32_t)*list->gameidc);
    real->gameidc=real->gameida=list->gameidc;
  }
  db->dirty=1;
  return real;
}

/* Update.
 */
 
struct db_list *db_list_update(struct db *db,const struct db_list *list) {
  int p=db_liststore_search(&db->lists,list->listid);
  if (p<0) return 0;
  struct db_list *real=db->lists.v[p];
  if (list->gameidc>real->gameida) {
    void *nv=realloc(real->gameidv,sizeof(uint32_t)*list->gameidc);
    if (!nv) return 0;
    real->gameidv=nv;
    real->gameida=list->gameidc;
  }
  memcpy(real->gameidv,list->gameidv,sizeof(uint32_t)*list->gameidc);
  real->gameidc=list->gameidc;
  real->gamec=0;
  real->name=list->name;
  real->desc=list->desc;
  real->sorted=list->sorted;
  db->dirty=db->lists.dirty=1;
  return real;
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
