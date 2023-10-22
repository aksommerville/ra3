#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "db_blobcache.h"

/* Cleanup.
 */
 
static void db_blobcache_bucket_cleanup(struct db_blobcache_bucket *bucket) {
  if (bucket->entryv) free(bucket->entryv);
}

void db_blobcache_cleanup(struct db_blobcache *blobcache) {
  if (blobcache->bucketv) {
    while (blobcache->bucketc-->0) db_blobcache_bucket_cleanup(blobcache->bucketv+blobcache->bucketc);
    free(blobcache->bucketv);
  }
}

/* List primitives.
 */
 
static int db_blobcache_bucketv_search(const struct db_blobcache *blobcache,uint32_t gameidlo) {
  int lo=0,hi=blobcache->bucketc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct db_blobcache_bucket *bucket=blobcache->bucketv+ck;
         if (gameidlo<bucket->gameidlo) hi=ck;
    else if (gameidlo>bucket->gameidlo) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static struct db_blobcache_bucket *db_blobcache_bucketv_insert(struct db_blobcache *blobcache,int p,uint32_t gameidlo) {
  if ((p<0)||(p>blobcache->bucketc)) return 0;
  if (blobcache->bucketc>=blobcache->bucketa) {
    int na=blobcache->bucketa+32;
    if (na>INT_MAX/sizeof(struct db_blobcache_bucket)) return 0;
    void *nv=realloc(blobcache->bucketv,sizeof(struct db_blobcache_bucket)*na);
    if (!nv) return 0;
    blobcache->bucketv=nv;
    blobcache->bucketa=na;
  }
  struct db_blobcache_bucket *bucket=blobcache->bucketv+p;
  memmove(bucket+1,bucket,sizeof(struct db_blobcache_bucket)*(blobcache->bucketc-p));
  blobcache->bucketc++;
  memset(bucket,0,sizeof(struct db_blobcache_bucket));
  bucket->gameidlo=gameidlo;
  return bucket;
}

// Returns a valid index or insertion point as you'd expect, *** but if there's more than one match, it's nondeterministic which ***.
static int db_blobcache_entryv_search(const struct db_blobcache_bucket *bucket,uint32_t gameid) {
  int lo=0,hi=bucket->entryc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct db_blobcache_entry *entry=bucket->entryv+ck;
         if (gameid<entry->gameid) hi=ck;
    else if (gameid>entry->gameid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static struct db_blobcache_entry *db_blobcache_entryv_insert(struct db_blobcache_bucket *bucket,int p,uint32_t gameid) {
  if ((p<0)||(p>bucket->entryc)) return 0;
  if (bucket->entryc>=bucket->entrya) {
    int na=bucket->entrya+128;
    if (na>INT_MAX/sizeof(struct db_blobcache_entry)) return 0;
    void *nv=realloc(bucket->entryv,sizeof(struct db_blobcache_entry)*na);
    if (!nv) return 0;
    bucket->entryv=nv;
    bucket->entrya=na;
  }
  struct db_blobcache_entry *entry=bucket->entryv+p;
  memmove(entry+1,entry,sizeof(struct db_blobcache_entry)*(bucket->entryc-p));
  bucket->entryc++;
  memset(entry,0,sizeof(struct db_blobcache_entry));
  entry->gameid=gameid;
  return entry;
}

/* Invalidate all.
 */

void db_blobcache_invalidate_all(struct db_blobcache *blobcache) {
  while (blobcache->bucketc>0) {
    blobcache->bucketc--;
    db_blobcache_bucket_cleanup(blobcache->bucketv+blobcache->bucketc);
  }
}

/* Invalidate one bucket.
 */

void db_blobcache_invalidate_gameid(struct db_blobcache *blobcache,uint32_t gameid) {
  int p=db_blobcache_bucketv_search(blobcache,gameid-gameid%100);
  if (p<0) return;
  blobcache->bucketv[p].entryc=0;
}

/* Iterate.
 */

int db_blobcache_for_all(int *empty,struct db_blobcache *blobcache,int (*cb)(uint32_t gameid,const char *base,void *userdata),void *userdata) {
  if (empty) *empty=1;
  const struct db_blobcache_bucket *bucket=blobcache->bucketv;
  int bucketi=blobcache->bucketc;
  for (;bucketi-->0;bucket++) {
    const struct db_blobcache_entry *entry=bucket->entryv;
    int entryi=bucket->entryc;
    for (;entryi-->0;entry++) {
      if (empty) *empty=0;
      int err=cb(entry->gameid,entry->base,userdata);
      if (err) return err;
    }
  }
  return 0;
}

int db_blobcache_for_bucket(int *empty,struct db_blobcache *blobcache,uint32_t gameid,int (*cb)(uint32_t gameid,const char *base,void *userdata),void *userdata) {
  if (empty) *empty=1;
  int bucketp=db_blobcache_bucketv_search(blobcache,gameid-gameid%100);
  if (bucketp<0) return 0;
  const struct db_blobcache_bucket *bucket=blobcache->bucketv+bucketp;
  if (!bucket->entryc) return 0;
  if (empty) *empty=0;
  const struct db_blobcache_entry *entry=bucket->entryv;
  int entryi=bucket->entryc;
  for (;entryi-->0;entry++) {
    int err=cb(entry->gameid,entry->base,userdata);
    if (err) return err;
  }
  return 0;
}

int db_blobcache_for_gameid(int *empty,struct db_blobcache *blobcache,uint32_t gameid,int (*cb)(uint32_t gameid,const char *base,void *userdata),void *userdata) {
  if (empty) *empty=1;
  int bucketp=db_blobcache_bucketv_search(blobcache,gameid-gameid%100);
  if (bucketp<0) return 0;
  const struct db_blobcache_bucket *bucket=blobcache->bucketv+bucketp;
  if (!bucket->entryc) return 0;
  if (empty) *empty=0;
  int entryp=db_blobcache_entryv_search(bucket,gameid);
  if (entryp<0) return 0;
  const struct db_blobcache_entry *entry=bucket->entryv+entryp;
  while (entryp&&(entry[-1].gameid==gameid)) { entryp--; entry--; }
  int entryi=bucket->entryc-entryp;
  for (;entryi-->0;entry++) {
    if (entry->gameid!=gameid) break;
    int err=cb(gameid,entry->base,userdata);
    if (err) return err;
  }
  return 0;
}

/* Add one entry.
 */

int db_blobcache_add(struct db_blobcache *blobcache,uint32_t gameid,const char *base) {
  if (!base) return -1;
  int basec=0;
  while (base[basec]) basec++;
  if ((basec<1)||(basec>=DB_BLOBCACHE_BASE_LIMIT)) return -1;

  struct db_blobcache_bucket *bucket;
  uint32_t gameidlo=gameid-gameid%100;
  int bucketp=db_blobcache_bucketv_search(blobcache,gameidlo);
  if (bucketp<0) {
    bucketp=-bucketp-1;
    if (!(bucket=db_blobcache_bucketv_insert(blobcache,bucketp,gameidlo))) return -1;
  } else {
    bucket=blobcache->bucketv+bucketp;
  }
  
  int entryp=db_blobcache_entryv_search(bucket,gameid);
  if (entryp<0) {
    entryp=-entryp-1;
  } else {
    // Ensure we don't already have it.
    int p=entryp;
    struct db_blobcache_entry *entry=bucket->entryv+p;
    while (p&&(entry[-1].gameid==gameid)) {
      p--; entry--;
      if (!strcmp(entry->base,base)) return 0;
    }
    p=entryp;
    entry=bucket->entryv+p;
    int i=bucket->entryc-p;
    for (;i-->0;entry++) {
      if (entry->gameid!=gameid) break;
      if (!strcmp(entry->base,base)) return 0;
    }
  }
  struct db_blobcache_entry *entry=db_blobcache_entryv_insert(bucket,entryp,gameid);
  if (!entry) return -1;
  memcpy(entry->base,base,basec+1);
  
  return 0;
}
