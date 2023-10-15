#include "db_internal.h"
#include "opt/fs/fs.h"
#include <errno.h>

/* Cleanup.
 */

void db_flatstore_cleanup(struct db_flatstore *store) {
  if (store->v) free(store->v);
}

/* Search.
 */
 
int db_flatstore_search1(const struct db_flatstore *store,uint32_t k) {
  int lo=0,hi=store->c;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const uint32_t *record=(uint32_t*)((char*)store->v+ck*store->objlen);
         if (k<record[0]) hi=ck;
    else if (k>record[0]) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

int db_flatstore_search2(const struct db_flatstore *store,uint32_t k0,uint32_t k1) {
  int lo=0,hi=store->c;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const uint32_t *record=(uint32_t*)((char*)store->v+ck*store->objlen);
         if (k0<record[0]) hi=ck;
    else if (k0>record[0]) lo=ck+1;
    else if (k1<record[1]) hi=ck;
    else if (k1>record[1]) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

int db_flatstore_search3(const struct db_flatstore *store,uint32_t k0,uint32_t k1,uint32_t k2) {
  int lo=0,hi=store->c;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const uint32_t *record=(uint32_t*)((char*)store->v+ck*store->objlen);
         if (k0<record[0]) hi=ck;
    else if (k0>record[0]) lo=ck+1;
    else if (k1<record[1]) hi=ck;
    else if (k1>record[1]) lo=ck+1;
    else if (k2<record[2]) hi=ck;
    else if (k2>record[2]) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Insert.
 */
 
static void *db_flatstore_insert(struct db_flatstore *store,int p) {
  if ((p<0)||(p>store->c)) return 0;
  if (store->c>=store->a) {
    int na=store->a+128;
    if (na>INT_MAX/store->objlen) return 0;
    void *nv=realloc(store->v,store->objlen*na);
    if (!nv) return 0;
    store->v=nv;
    store->a=na;
  }
  char *record=(char*)store->v+p*store->objlen;
  memmove(record+store->objlen,record,store->objlen*(store->c-p));
  store->c++;
  memset(record,0,store->objlen);
  store->dirty=1;
  return record;
}
 
void *db_flatstore_insert1(struct db_flatstore *store,int p,uint32_t k) {
  uint32_t *record=db_flatstore_insert(store,p);
  if (!record) return 0;
  record[0]=k;
  return record;
}

void *db_flatstore_insert2(struct db_flatstore *store,int p,uint32_t k0,uint32_t k1) {
  uint32_t *record=db_flatstore_insert(store,p);
  if (!record) return 0;
  record[0]=k0;
  record[1]=k1;
  return record;
}

void *db_flatstore_insert3(struct db_flatstore *store,int p,uint32_t k0,uint32_t k1,uint32_t k2) {
  uint32_t *record=db_flatstore_insert(store,p);
  if (!record) return 0;
  record[0]=k0;
  record[1]=k1;
  record[2]=k2;
  return record;
}

/* Remove.
 */

void db_flatstore_remove(struct db_flatstore *store,int p,int c) {
  if ((p<0)||(c<0)||(p>store->c-c)) return;
  store->c-=c;
  char *lo=(char*)store->v+p+store->objlen;
  char *hi=lo+c*store->objlen;
  memmove(lo,hi,store->objlen*(store->c-p));
  store->dirty=1;
}

/* Clear.
 */
 
void db_flatstore_clear(struct db_flatstore *store) {
  if (!store->c) return;
  store->c=0;
  store->dirty=1;
}

/* Next ID, for one-word keys.
 */
 
uint32_t db_flatstore_next_id(const struct db_flatstore *store) {
  int stride=store->objlen>>2;
  uint32_t *record=store->v;
  
  // It's reasonably likely that the ID space is packed already, and could be thousands of records.
  // Check for that case special.
  if (store->c&&(record[stride*(store->c-1)]==store->c)) return store->c+1;
  
  int i=store->c;
  uint32_t expect=1;
  for (;i-->0;record+=stride,expect++) {
    if (*record!=expect) return expect;
  }
  return expect;
}

/* Load.
 */
 
int db_flatstore_load(struct db_flatstore *store,const char *root,int rootc) {
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s%c%s",rootc,root,path_separator(),store->name);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  
  void *nv=0;
  int nc=file_read(&nv,path);
  if (nc<0) {
    if (errno==ENOENT) {
      store->c=0;
      store->dirty=0;
      return 0;
    }
    return -1;
  }
  nc/=store->objlen;
  
  if (store->v) free(store->v);
  store->v=nv;
  store->c=nc;
  store->a=nc;
  store->dirty=0;
  return 0;
}

/* Save.
 */
 
int db_flatstore_save(struct db_flatstore *store,const char *root,int rootc) {
  if (!store->dirty) return 0;
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s%c%s",rootc,root,path_separator(),store->name);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  if (file_write(path,store->v,store->c*store->objlen)<0) return -1;
  store->dirty=0;
  return 1;
}
