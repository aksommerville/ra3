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

/* New record.
 */
 
struct db_play *db_play_insert(struct db *db,uint32_t gameid) {
  uint32_t time=db_time_now();
  int p=db_flatstore_search2(&db->plays,gameid,time);
  int panic=100;
  while (p>=0) {
    if (panic--<0) return 0;
    time=db_time_advance(time);
    p=db_flatstore_search2(&db->plays,gameid,time);
  }
  p=-p-1;
  struct db_play *play=db_flatstore_insert2(&db->plays,p,gameid,time);
  if (!play) return 0;
  return play;
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

/* Encode JSON.
 */
 
static int db_play_encode_json(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_play *play,
  int include_gameid
) {
  int jsonctx=sr_encode_json_object_start(dst,0,0);
  if (jsonctx<0) return -1;
  if (include_gameid) {
    if (sr_encode_json_int(dst,"gameid",6,play->gameid)<0) return -1;
  }
  char tmp[32];
  int tmpc=db_time_repr(tmp,sizeof(tmp),play->time);
  if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
  if (sr_encode_json_string(dst,"time",4,tmp,tmpc)<0) return -1;
  if (sr_encode_json_int(dst,"dur",3,play->dur_m)<0) return -1;
  if (sr_encode_json_object_end(dst,jsonctx)<0) return -1;
  return 0;
}

int db_play_encode(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_play *play,
  int format
) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: return db_play_encode_json(dst,db,play,1);
    case DB_FORMAT_ingame: return db_play_encode_json(dst,db,play,0);
  }
  return -1;
}
