#include "db_internal.h"
#include "opt/serial/serial.h"

/* Direct access to store.
 */

int db_comment_count(const struct db *db) {
  return db->comments.c;
}

struct db_comment *db_comment_get_by_index(const struct db *db,int p) {
  return db_flatstore_get(&db->comments,p);
}

int db_comments_get_by_gameid(struct db_comment **dst,const struct db *db,uint32_t gameid) {
  int p=db_flatstore_search3(&db->comments,gameid,0,0);
  if (p<0) p=-p-1;
  struct db_comment *comment=db_flatstore_get(&db->comments,p);
  if (dst) *dst=comment;
  int c=0;
  while ((p<db->comments.c)&&(comment->gameid==gameid)) { p++; comment++; c++; }
  return c;
}

/* Delete one comment.
 */

int db_comment_delete(struct db *db,const struct db_comment *comment) {
  int p=db_flatstore_search3(&db->comments,comment->gameid,comment->time,comment->k);
  if (p<0) return -1;
  db_flatstore_remove(&db->comments,p,1);
  db->dirty=1;
  return 0;
}

/* Delete multiple comments.
 */
 
int db_comment_delete_for_gameid(struct db *db,uint32_t gameid) {
  int p=db_flatstore_search3(&db->comments,gameid,0,0);
  if (p<0) p=-p-1;
  struct db_comment *comment=db_flatstore_get(&db->comments,p);
  int c=0,i=p;
  while ((i<db->comments.c)&&(comment->gameid==gameid)) { i++; comment++; c++; }
  if (!c) return 0;
  db_flatstore_remove(&db->comments,p,c);
  db->dirty=1;
  return 0;
}

/* Insert.
 */
 
struct db_comment *db_comment_insert(struct db *db,const struct db_comment *comment) {
  if (!comment||!db_game_get_by_id(db,comment->gameid)) return 0;
  
  // (gameid,time,k) must be unique, so bump the timestamp until there's no collision.
  // Also, if timestamp is initially unset, set it to now.
  // It is theoretically possible to exhaust the key space; we check.
  struct db_comment scratch=*comment;
  if (!scratch.time) scratch.time=db_time_now();
  int p=db_flatstore_search3(&db->comments,scratch.gameid,scratch.time,scratch.k);
  if (p<0) p=-p-1;
  const struct db_comment *q=((struct db_comment*)db->comments.v)+p;
  while ((p<db->comments.c)&&(q->gameid==scratch.gameid)) {
    if (q->time<scratch.time) { p++; q++; continue; }
    if (q->time>scratch.time) break;
    if (q->k<scratch.k) { p++; q++; continue; }
    if (q->k>scratch.k) break;
    if (!(scratch.time=db_time_advance(scratch.time))) return 0;
    p++;
    q++;
  }
  
  struct db_comment *real=db_flatstore_insert3(&db->comments,p,scratch.gameid,scratch.time,scratch.k);
  if (!real) return 0;
  real->v=scratch.v;
  db->dirty=1;
  return real;
}

/* Update.
 */
 
struct db_comment *db_comment_update(struct db *db,const struct db_comment *comment) {
  int p=db_flatstore_search3(&db->comments,comment->gameid,comment->time,comment->k);
  if (p<0) return 0;
  struct db_comment *real=db_flatstore_get(&db->comments,p);
  if (real->v!=comment->v) {
    real->v=comment->v;
    db->dirty=db->comments.dirty=1;
  }
  return real;
}

/* Manual edit.
 */

int db_comment_set_v(struct db *db,struct db_comment *comment,const char *src,int srcc) {
  comment->v=db_string_intern(db,src,srcc);
  db->comments.dirty=db->dirty=1;
  return 0;
}

void db_comment_dirty(struct db *db,struct db_comment *comment) {
  db->comments.dirty=db->dirty=1;
}
