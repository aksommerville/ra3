#include "db_internal.h"
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"

/* Direct access to store.
 */

int db_game_count(const struct db *db) {
  return db->games.c;
}

struct db_game *db_game_get_by_index(const struct db *db,int p) {
  return db_flatstore_get(&db->games,p);
}

struct db_game *db_game_get_by_id(const struct db *db,uint32_t gameid) {
  return db_flatstore_get(&db->games,db_flatstore_search1(&db->games,gameid));
}

/* Delete, cascading.
 */

int db_game_delete(struct db *db,uint32_t gameid) {
  int p=db_flatstore_search1(&db->games,gameid);
  if (p<0) return -1;
  db_flatstore_remove(&db->games,p,1);
  db->dirty=1;
  
  db_comment_delete_for_gameid(db,gameid);
  db_play_delete_for_gameid(db,gameid);
  struct db_list **list=db->lists.v;
  int i=db->lists.c;
  for (;i-->0;list++) {
    db_list_remove(db,*list,gameid);
  }
  db_blob_delete_for_gameid(db,gameid);
  
  return 0;
}

/* Insert.
 */
 
struct db_game *db_game_insert(struct db *db,const struct db_game *game) {
  if (!game) return 0;
  uint32_t gameid=game->gameid;
  if (!gameid) gameid=db_flatstore_next_id(&db->games);
  int p=db_flatstore_search1(&db->games,gameid);
  if (p>=0) return 0;
  p=-p-1;
  struct db_game *real=db_flatstore_insert1(&db->games,p,gameid);
  if (!real) return 0;
  memcpy(real,game,sizeof(struct db_game));
  real->gameid=gameid;
  db->dirty=1;
  return real;
}

/* Update.
 */
 
struct db_game *db_game_update(struct db *db,const struct db_game *game) {
  int p=db_flatstore_search1(&db->games,game->gameid);
  if (p<0) return 0;
  struct db_game *real=db_flatstore_get(&db->games,p);
  memcpy(real,game,sizeof(struct db_game));
  db->dirty=db->games.dirty=1;
  return real;
}

/* Simple convenience setters.
 */

int db_game_set_platform(struct db *db,struct db_game *game,const char *src,int srcc) {
  game->platform=db_string_intern(db,src,srcc);
  db->games.dirty=db->dirty=1;
  return 0;
}

int db_game_set_author(struct db *db,struct db_game *game,const char *src,int srcc) {
  game->author=db_string_intern(db,src,srcc);
  db->games.dirty=db->dirty=1;
  return 0;
}

int db_game_set_flags(struct db *db,struct db_game *game,uint32_t flags) {
  if (game->flags==flags) return 0;
  game->flags=flags;
  db->games.dirty=db->dirty=1;
  return 0;
}

int db_game_set_rating(struct db *db,struct db_game *game,uint32_t rating) {
  if (game->rating==rating) return 0;
  game->rating=rating;
  db->games.dirty=db->dirty=1;
  return 0;
}

int db_game_set_pubtime(struct db *db,struct db_game *game,const char *iso8601,int c) {
  game->pubtime=db_time_eval(iso8601,c);
  db->games.dirty=db->dirty=1;
  return 0;
}

int db_game_set_name(struct db *db,struct db_game *game,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc>sizeof(game->name)) srcc=sizeof(game->name);
  memcpy(game->name,src,srcc);
  memset(game->name+srcc,0,sizeof(game->name)-srcc);
  db->games.dirty=db->dirty=1;
  return 0;
}

/* Set path: A more complicated proposition.
 */

int db_game_set_path(struct db *db,struct db_game *game,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int sepp=path_split(src,srcc);
  const char *base=src+sepp+1; // sepp can be -1 but no other negative value -- this is safe
  int basec=srcc-sepp-1;
  if (basec>sizeof(game->base)) return -1;
  memcpy(game->base,base,basec);
  memset(game->base+basec,0,sizeof(game->base)-basec);
  if (sepp<=0) {
    game->dir=0;
  } else {
    if (!(game->dir=db_string_intern(db,src,sepp))) return -1;
  }
  db->games.dirty=db->dirty=1;
  return 0;
}

/* Dirty.
 */

void db_game_dirty(struct db *db,struct db_game *game) {
  db->dirty=1;
  db->games.dirty=1;
}

/* Get path.
 */
 
int db_game_get_path(char *dst,int dsta,const struct db *db,const struct db_game *game) {
  int dstc=0;
  if (game->dir||game->base[0]) {
    const char *dir=0;
    int dirc=db_string_get(&dir,db,game->dir);
    if (dirc>0) {
      if (dirc<=dsta) memcpy(dst,dir,dirc);
      dstc=dirc;
    }
    if (dstc<dsta) dst[dstc]=path_separator();
    dstc++;
    int basec=sizeof(game->base);
    while (basec&&!game->base[basec-1]) basec--;
    if (dstc<=dsta-basec) memcpy(dst+dstc,game->base,basec);
    dstc+=basec;
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}
