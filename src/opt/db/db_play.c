#include "db_internal.h"
#include "opt/serial/serial.h"

/* Direct store access.
 */
 
int db_play_count(const struct db *db) {
  return db->plays.c;
}

struct db_play *db_play_get_by_index(const struct db *db,int p) {
  return db_flatstore_get(&db->plays,p);
}

struct db_play *db_play_get_by_gameid_time(const struct db *db,uint32_t gameid,uint32_t time) {
  return db_flatstore_get(&db->plays,db_flatstore_search2(&db->plays,gameid,time));
}

/* Get multiple.
 */
 
int db_plays_get_by_gameid(struct db_play **dst,const struct db *db,uint32_t gameid) {
  int p=db_flatstore_search2(&db->plays,gameid,0);
  if (p<0) p=-p-1;
  struct db_play *play=db_flatstore_get(&db->plays,p);
  if (!play) return 0;
  if (dst) *dst=play;
  int c=0;
  while ((p<db->plays.c)&&(play->gameid==gameid)) { p++; c++; play++; }
  return c;
}

/* Delete one play.
 */
 
int db_play_delete(struct db *db,uint32_t gameid,uint32_t time) {
  int p=db_flatstore_search2(&db->plays,gameid,time);
  if (p<0) return -1;
  db_flatstore_remove(&db->plays,p,1);
  db->dirty=1;
  return 0;
}

/* Delete plays for gameid.
 */
 
int db_play_delete_for_gameid(struct db *db,uint32_t gameid) {
  int p=db_flatstore_search2(&db->plays,gameid,0);
  if (p<0) p=-p-1;
  const struct db_play *play=db_flatstore_get(&db->plays,p);
  if (!play) return 0;
  int i=p,c=0;
  while ((i<db->plays.c)&&(play->gameid==gameid)) { i++; c++; play++; }
  if (!c) return 0;
  db_flatstore_remove(&db->plays,p,c);
  db->dirty=1;
  return 0;
}

/* Insert.
 */

struct db_play *db_play_insert(struct db *db,const struct db_play *play) {
  if (!play||!play->gameid) return 0;
  
  // Fill in now if time unset, then advance time until it's unique.
  struct db_play scratch=*play;
  if (!scratch.time) scratch.time=db_time_now();
  int p=db_flatstore_search2(&db->plays,play->gameid,scratch.time);
  if (p<0) {
    p=-p-1;
  } else {
    const struct db_play *q=db_flatstore_get(&db->plays,p);
    while ((p<db->plays.c)&&(q->gameid==scratch.gameid)&&(q->time==scratch.time)) {
      if (!(scratch.time=db_time_advance(scratch.time))) return 0;
      p++;
      q++;
    }
  }
  
  struct db_play *real=db_flatstore_insert2(&db->plays,p,scratch.gameid,scratch.time);
  if (!real) return 0;
  real->dur_m=scratch.dur_m;
  db->dirty=1;
  return real;
}

/* Update.
 */

struct db_play *db_play_update(struct db *db,const struct db_play *play) {
  int p=db_flatstore_search2(&db->plays,play->gameid,play->time);
  if (p<0) return 0;
  struct db_play *real=db_flatstore_get(&db->plays,p);
  if (real->dur_m!=play->dur_m) {
    real->dur_m=play->dur_m;
    db->dirty=db->plays.dirty=1;
  }
  return real;
}

/* Finish play record if one exists.
 */
 
struct db_play *db_play_finish(struct db *db,uint32_t gameid) {
  int p=db_flatstore_search2(&db->plays,gameid+1,0);
  if (p<0) p=-p-1;
  p--;
  struct db_play *play=db_flatstore_get(&db->plays,p);
  if (!play) return 0;
  while ((p>=0)&&(play->gameid==gameid)) {
    if (!play->dur_m) {
      play->dur_m=db_time_diff_m(play->time,db_time_now());
      if (!play->dur_m) play->dur_m=1;
      db->dirty=db->plays.dirty=1;
      return play;
    }
    p--;
    play--;
  }
  return 0;
}

/* Manual edit.
 */
 
int db_play_set_dur_m(struct db *db,struct db_play *play,uint32_t dur_m) {
  play->dur_m=dur_m;
  db->plays.dirty=db->dirty=1;
  return 0;
}

void db_play_dirty(struct db *db,struct db_play *play) {
  db->plays.dirty=db->dirty=1;
}
