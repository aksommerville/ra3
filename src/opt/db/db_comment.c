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

/* Insert new comment.
 */

struct db_comment *db_comment_insert(struct db *db,uint32_t gameid,uint32_t time,uint32_t k) {
  if (!time) time=db_time_now();
  int p=db_flatstore_search3(&db->comments,gameid,time,k);
  int panic=100;
  while (p>=0) {
    if (panic--<0) return 0;
    time=db_time_advance(time);
    p=db_flatstore_search3(&db->comments,gameid,time,k);
  }
  p=-p-1;
  struct db_comment *comment=db_flatstore_insert3(&db->comments,p,gameid,time,k);
  if (!comment) return 0;
  db->dirty=1;
  return comment;
}

/* Insert new comment from JSON.
 */

int db_comment_insert_json(struct db *db,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  struct db_comment scratch={0};
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_object_start(&decoder)<0) return -1;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    
    if ((kc==6)&&!memcmp(k,"gameid",6)) {
      int n;
      if (sr_decode_json_int(&n,&decoder)<0) return -1;
      scratch.gameid=n;
      continue;
    }
    
    if ((kc==4)&&!memcmp(k,"time",4)) {
      char tmp[32];
      int tmpc=sr_decode_json_string(tmp,sizeof(tmp),&decoder);
      if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
      scratch.time=db_time_eval(tmp,tmpc);
      continue;
    }
    
    if ((kc==1)&&(k[0]=='k')) {
      if (db_decode_json_string(&scratch.k,db,&decoder)<0) return -1;
      continue;
    }
    
    if ((kc==1)&&(k[0]=='v')) {
      if (db_decode_json_string(&scratch.v,db,&decoder)<0) return -1;
      continue;
    }
    
    if (sr_decode_json_skip(&decoder)<0) return -1;
  }
  if (sr_decode_json_end(&decoder,0)<0) return -1;
  
  struct db_comment *comment=db_comment_insert(db,scratch.gameid,scratch.time,scratch.k);
  if (!comment) return -1;
  comment->v=scratch.v;
  return 0;
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

/* Encode to JSON.
 */
 
static int db_comment_encode_json(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_comment *comment,
  int include_gameid
) {
  int jsonctx=sr_encode_json_object_start(dst,0,0);
  if (jsonctx<0) return -1;
  
  if (include_gameid) {
    if (sr_encode_json_int(dst,"gameid",6,comment->gameid)<0) return -1;
  }
  if (comment->time) {
    char tmp[32];
    int tmpc=db_time_repr(tmp,sizeof(tmp),comment->time);
    if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
    if (sr_encode_json_string(dst,"time",4,tmp,tmpc)<0) return -1;
  }
  if (db_encode_json_string(dst,db,"k",1,comment->k)<0) return -1;
  if (db_encode_json_string(dst,db,"v",1,comment->v)<0) return -1;
  
  if (sr_encode_json_object_end(dst,jsonctx)<0) return -1;
  return 0;
}

/* Encode, dispatch on format.
 */

int db_comment_encode(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_comment *comment,
  int format
) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: return db_comment_encode_json(dst,db,comment,1);
    case DB_FORMAT_ingame: return db_comment_encode_json(dst,db,comment,0);
  }
  return -1;
}
