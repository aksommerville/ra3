#include "db_internal.h"

/* Check each string in the store, and zero it out if nobody refers to it.
 */
 
static int db_gc_unused_strings(struct db *db) {
  if (db->strings.tocc<1) return 0;
  uint8_t *usage=calloc(1,db->strings.tocc+1); // index=id
  if (!usage) return -1;
  
  #define INUSE(stringid) { \
    if (stringid>db->strings.tocc) { \
      fprintf(stderr,"*** invalid string %d (tocc=%d) [%s:%d] ***\n",stringid,db->strings.tocc,__FILE__,__LINE__); \
      free(usage); \
      return -1; \
    } \
    usage[stringid]=1; \
  }
  
  {
    const struct db_game *game=db->games.v;
    int i=db->games.c;
    for (;i-->0;game++) {
      INUSE(game->platform)
      INUSE(game->author)
      INUSE(game->genre)
      INUSE(game->dir)
    }
  }
  
  {
    const struct db_comment *comment=db->comments.v;
    int i=db->comments.c;
    for (;i-->0;comment++) {
      INUSE(comment->k)
      INUSE(comment->v)
    }
  }
  
  // play do not contain any strings
  
  {
    const struct db_launcher *launcher=db->launchers.v;
    int i=db->launchers.c;
    for (;i-->0;launcher++) {
      INUSE(launcher->name)
      INUSE(launcher->platform)
      INUSE(launcher->suffixes)
      INUSE(launcher->cmd)
      INUSE(launcher->desc)
    }
  }
  
  {
    const struct db_upgrade *upgrade=db->upgrades.v;
    int i=db->upgrades.c;
    for (;i-->0;upgrade++) {
      INUSE(upgrade->name)
      INUSE(upgrade->desc)
      INUSE(upgrade->method)
      INUSE(upgrade->param)
      INUSE(upgrade->status)
    }
  }
  
  #undef INUSE
  
  struct db_string_toc_entry *entry=db->strings.toc;
  const uint8_t *inuse=usage+1;
  int i=db->strings.tocc,rmc=0;
  for (;i-->0;entry++,inuse++) {
    if (*inuse) continue;
    entry->p=0;
    entry->c=0;
    rmc++;
  }
  
  if (rmc) {
    fprintf(stderr,"%s: Eliminating %d strings.\n",__func__,rmc);
    db->strings.dirty=1;
    db->dirty=1;
  }
  
  free(usage);
  return 0;
}

/* Some text is about to be removed from the string buffer.
 * Update any record pointing above (p), subtract (c) from it.
 * If a record intersects (p:c) please fail.
 */
 
static int db_update_strings_for_removal(struct db *db,int p,int c) {
  int top=p+c;
  struct db_string_toc_entry *entry=db->strings.toc;
  int i=db->strings.tocc;
  for (;i-->0;entry++) {
    if (entry->p>=top) entry->p-=c;
    else if (entry->p>=p) return -1;
  }
  return 0;
}

/* Find unused portions of the text dump and condense them.
 */
 
static int db_gc_unused_text(struct db *db) {
  if (db->strings.textc<1) return 0;
  uint8_t *usage=calloc(1,db->strings.textc);
  if (!usage) return -1;
  
  const struct db_string_toc_entry *entry=db->strings.toc;
  int i=db->strings.tocc;
  for (;i-->0;entry++) {
    if (
      (entry->p>UINT32_MAX-entry->c)||
      (entry->p>db->strings.textc-entry->c)
    ) {
      fprintf(stderr,"*** string %d (%d@%d) overruns text buffer c=%d ***\n",(int)(entry-db->strings.toc),entry->c,entry->p,db->strings.textc);
      free(usage);
      return -1;
    }
    memset(usage+entry->p,1,entry->c);
  }
  
  int rmtotal=0;
  int p=db->strings.textc;
  while (p>0) {
    p--;
    if (usage[p]) continue;
    int c=1;
    while (p&&!usage[p-1]) { p--; c++; }
    if (db_update_strings_for_removal(db,p,c)<0) {
      fprintf(stderr,"*** panic during text removal [%s:%d] ***\n",__FILE__,__LINE__);
      free(usage);
      return -1;
    }
    db->strings.textc-=c;
    memmove(db->strings.text+p,db->strings.text+p+c,db->strings.textc-p);
    rmtotal+=c;
  }
  
  if (rmtotal) {
    fprintf(stderr,"%s: Eliminating %d bytes of unused text.\n",__func__,rmtotal);
    db->strings.dirty=1;
    db->dirty=1;
  }
  
  free(usage);
  return 0;
}

/* Garbage collection, schedule of operations.
 */
 
int db_gc(struct db *db) {
  if (db_gc_unused_strings(db)<0) return -1;
  if (db_gc_unused_text(db)<0) return -1;
  return 0;
}
