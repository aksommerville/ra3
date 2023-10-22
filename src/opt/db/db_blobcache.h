/* db_blobcache.h
 * In-memory index of blob store.
 * Easy to flush and rebuild lazily.
 */
 
#ifndef DB_BLOBCACHE_H
#define DB_BLOBCACHE_H

#define DB_BLOBCACHE_BASE_LIMIT 64 /* Longest I've seen is 34; 64 should be plenty. */

struct db_blobcache {
  struct db_blobcache_bucket {
    uint32_t gameidlo;
    struct db_blobcache_entry {
      uint32_t gameid;
      char base[DB_BLOBCACHE_BASE_LIMIT];
    } *entryv;
    int entryc,entrya;
  } *bucketv;
  int bucketc,bucketa;
};

void db_blobcache_cleanup(struct db_blobcache *blobcache);

/* Invalidating one gameid also invalidates the 100 surrounding it.
 */
void db_blobcache_invalidate_all(struct db_blobcache *blobcache);
void db_blobcache_invalidate_gameid(struct db_blobcache *blobcache,uint32_t gameid);

/* Trigger callback for each blob, stop if you return nonzero.
 * If (*empty) nonzero on a zero return, it means we didn't find anything because we haven't been populated.
 * In that case, you should do the real FS search.
 */
int db_blobcache_for_all(int *empty,struct db_blobcache *blobcache,int (*cb)(uint32_t gameid,const char *base,void *userdata),void *userdata);
int db_blobcache_for_bucket(int *empty,struct db_blobcache *blobcache,uint32_t gameid,int (*cb)(uint32_t gameid,const char *base,void *userdata),void *userdata);
int db_blobcache_for_gameid(int *empty,struct db_blobcache *blobcache,uint32_t gameid,int (*cb)(uint32_t gameid,const char *base,void *userdata),void *userdata);

/* When you read the FS manually, call this for everything you find.
 */
int db_blobcache_add(struct db_blobcache *blobcache,uint32_t gameid,const char *base);

#endif
