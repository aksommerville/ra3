#ifndef DB_INTERNAL_H
#define DB_INTERNAL_H

#include "db.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include "db_blobcache.h"

struct db_flatstore;
struct db_stringstore;
struct db_liststore;

/* Special store for strings.
 *************************************************************/
 
struct db_stringstore {
  // TOC is indexed by (id-1). Empty entries are perfectly legal.
  struct db_string_toc_entry {
    uint32_t p;
    uint32_t c;
  } *toc;
  int tocc,toca;
  char *text;
  int textc,texta;
  int dirty;
};

void db_stringstore_cleanup(struct db_stringstore *store);

void db_stringstore_clear(struct db_stringstore *store);

int db_stringstore_load(struct db_stringstore *store,const char *root,int rootc);
int db_stringstore_save(struct db_stringstore *store,const char *root,int rootc);

/* Special store for lists, since we can't hold them contiguously in memory.
 * The whole list store gets decoded and encoded on every save/load.
 * Serial format:
 *   Packed lists, sorted by id:
 *     0000   4 id
 *     0004   4 name
 *     0008   4 desc
 *     0010   1 sorted
 *     0011   3 gameidc
 *     0014 ... gameidv, 4 bytes each
 *************************************************************/
 
struct db_liststore {
  struct db_list **v;
  int c,a;
  int contigc;
  int dirty;
};

void db_liststore_cleanup(struct db_liststore *store);

void db_liststore_clear(struct db_liststore *store);
void db_liststore_remove(struct db_liststore *store,int p);

int db_liststore_decode(struct db_liststore *store,const void *src,int srcc);
int db_liststore_encode(struct sr_encoder *dst,const struct db_liststore *store);
int db_list_encode_binary(struct sr_encoder *dst,const struct db_list *list);

int db_liststore_load(struct db_liststore *store,const char *root,int rootc);
int db_liststore_save(struct db_liststore *store,const char *root,int rootc);

int db_liststore_search(const struct db_liststore *store,uint32_t listid);
struct db_list *db_liststore_insert(struct db_liststore *store,int p,uint32_t listid);
uint32_t db_liststore_next_id(const struct db_liststore *store);

/* Flat store, for fixed-size records.
 * (game,comment,play,launcher)
 *************************************************************/
 
struct db_flatstore {
  const char *name;
  int objlen;
  void *v;
  int c,a; // in records
  int dirty;
};

void db_flatstore_cleanup(struct db_flatstore *store);

static inline void *db_flatstore_get(const struct db_flatstore *store,int p) {
  if ((p<0)||(p>=store->c)) return 0;
  return (char*)store->v+p*store->objlen;
}

/* Different search and insert functions for key length in words.
 */
int db_flatstore_search1(const struct db_flatstore *store,uint32_t k);
void *db_flatstore_insert1(struct db_flatstore *store,int p,uint32_t k);
int db_flatstore_search2(const struct db_flatstore *store,uint32_t k0,uint32_t k1);
void *db_flatstore_insert2(struct db_flatstore *store,int p,uint32_t k,uint32_t k1);
int db_flatstore_search3(const struct db_flatstore *store,uint32_t k0,uint32_t k1,uint32_t k2);
void *db_flatstore_insert3(struct db_flatstore *store,int p,uint32_t k,uint32_t k1,uint32_t k2);

void db_flatstore_remove(struct db_flatstore *store,int p,int c);

void db_flatstore_clear(struct db_flatstore *store);

// Only valid for one-word keys. (ie game and launcher)
uint32_t db_flatstore_next_id(const struct db_flatstore *store);

int db_flatstore_load(struct db_flatstore *store,const char *root,int rootc);
int db_flatstore_save(struct db_flatstore *store,const char *root,int rootc);

/* Context.
 ************************************************************/

struct db {
  struct db_flatstore games;
  struct db_flatstore launchers;
  struct db_flatstore comments;
  struct db_flatstore plays;
  struct db_stringstore strings;
  struct db_liststore lists;
  struct db_blobcache blobcache;
  int dirty;
  char *root;
  int rootc;
};

#endif
