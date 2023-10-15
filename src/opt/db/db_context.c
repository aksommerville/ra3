#include "db_internal.h"
#include "opt/fs/fs.h"

/* Delete.
 */
 
void db_del(struct db *db) {
  if (!db) return;
  db_flatstore_cleanup(&db->games);
  db_flatstore_cleanup(&db->launchers);
  db_flatstore_cleanup(&db->comments);
  db_flatstore_cleanup(&db->plays);
  db_liststore_cleanup(&db->lists);
  db_stringstore_cleanup(&db->strings);
  if (db->root) free(db->root);
  free(db);
}

/* New.
 */

struct db *db_new(const char *root) {
  struct db *db=calloc(1,sizeof(struct db));
  if (!db) return 0;
  
  db->games.name="game"; db->games.objlen=sizeof(struct db_game);
  db->launchers.name="launcher"; db->launchers.objlen=sizeof(struct db_launcher);
  db->comments.name="comment"; db->comments.objlen=sizeof(struct db_comment);
  db->plays.name="play"; db->plays.objlen=sizeof(struct db_play);
  
  if (root) {
    if (
      (db_set_root(db,root,-1)<0)||
      (db_load(db)<0)
    ) {
      db_del(db);
      return 0;
    }
  }
  
  return db;
}

/* Trivial accessors.
 */

const char *db_get_root(struct db *db) {
  if (!db) return 0;
  return db->root;
}

int db_set_root(struct db *db,const char *root,int rootc) {
  if (!db) return -1;
  if (!root) rootc=0; else if (rootc<0) { rootc=0; while (root[rootc]) rootc++; }
  char *nv=0;
  if (rootc) {
    if (!(nv=malloc(rootc+1))) return -1;
    memcpy(nv,root,rootc);
    nv[rootc]=0;
  }
  if (db->root) free(db->root);
  db->root=nv;
  db->rootc=rootc;
  return 0;
}

int db_is_dirty(const struct db *db) {
  if (!db) return 0;
  return db->dirty;
}

void db_dirty(struct db *db) {
  if (!db) return;
  db->dirty=1;
  db->games.dirty=1;
  db->launchers.dirty=1;
  db->comments.dirty=1;
  db->plays.dirty=1;
  db->lists.dirty=1;
  db->strings.dirty=1;
}

/* Save.
 */

int db_save(struct db *db) {
  if (!db||!db->dirty) return 0;
  if (!db->rootc) return -1;
  if (!file_get_type(db->root)) {
    if (dir_mkdir(db->root)<0) return -1;
  }
  if (db_flatstore_load(&db->games,db->root,db->rootc)<0) return -1;
  if (db_flatstore_load(&db->launchers,db->root,db->rootc)<0) return -1;
  if (db_flatstore_load(&db->comments,db->root,db->rootc)<0) return -1;
  if (db_flatstore_load(&db->plays,db->root,db->rootc)<0) return -1;
  if (db_stringstore_load(&db->strings,db->root,db->rootc)<0) return -1;
  if (db_liststore_load(&db->lists,db->root,db->rootc)<0) return -1;
  db->dirty=0;
  return 1;
}

/* Load.
 */
 
int db_load(struct db *db) {
  if (!db->rootc) return -1;
  db_clear(db);
  if (db_flatstore_load(&db->games,db->root,db->rootc)<0) return -1;
  if (db_flatstore_load(&db->launchers,db->root,db->rootc)<0) return -1;
  if (db_flatstore_load(&db->comments,db->root,db->rootc)<0) return -1;
  if (db_flatstore_load(&db->plays,db->root,db->rootc)<0) return -1;
  if (db_stringstore_load(&db->strings,db->root,db->rootc)<0) return -1;
  if (db_liststore_load(&db->lists,db->root,db->rootc)<0) return -1;
  db->dirty=0;
  return 0;
}

/* Drop all content.
 */

void db_clear(struct db *db) {
  db_flatstore_clear(&db->games);
  db_flatstore_clear(&db->launchers);
  db_flatstore_clear(&db->comments);
  db_flatstore_clear(&db->plays);
  db_liststore_clear(&db->lists);
  db_stringstore_clear(&db->strings);
  db->dirty=1;
}
