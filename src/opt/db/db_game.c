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

/* Create new game.
 */

struct db_game *db_game_insert(struct db *db,uint32_t gameid) {
  if (!gameid) gameid=db_flatstore_next_id(&db->games);
  int p=db_flatstore_search1(&db->games,gameid);
  if (p>=0) return 0;
  struct db_game *game=db_flatstore_insert1(&db->games,p,gameid);
  if (!game) return 0;
  db->dirty=1;
  return game;
}

/* Decode from JSON into a scratch record and visit list.
 */
 
static int db_game_decode_json(struct db_game *dst,struct db_game *mask,struct db *db,const char *src,int srcc) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  int jsonctx=sr_decode_json_object_start(&decoder);
  if (jsonctx<0) return -1;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
  
    #define INTFLD(token,fldname) { \
      if ((kc==sizeof(token)-1)&&!memcmp(k,token,kc)) { \
        int n; \
        if (sr_decode_json_int(&n,&decoder)<0) return -1; \
        dst->fldname=n; \
        mask->fldname=1; \
        continue; \
      } \
    }
    #define STRFLD(token,fldname) { \
      if ((kc==sizeof(token)-1)&&!memcmp(k,token,kc)) { \
        if (db_decode_json_string(&dst->fldname,db,&decoder)<0) return -1; \
        mask->fldname=1; \
        continue; \
      } \
    }
    #define KSTRFLD(token,fldname) { \
      if ((kc==sizeof(token)-1)&&!memcmp(k,token,kc)) { \
        int err=sr_decode_json_string(dst->fldname,sizeof(dst->fldname),&decoder); \
        if (err<0) return -1; \
        if (err>sizeof(dst->fldname)) { \
          const char *srctoken=0; \
          int srctokenc=sr_decode_json_expression(&srctoken,&decoder); \
          if (srctokenc<0) return -1; \
          if ((srctokenc>=2)&&(srctoken[0]=='"')&&(srctoken[srctokenc-1]=='"')) { \
            srctoken++; \
            srctokenc-=2; \
          } \
          if (srctokenc>sizeof(dst->fldname)) srctokenc=sizeof(dst->fldname); \
          memcpy(dst->fldname,srctoken,srctokenc); \
          err=srctokenc; \
        } \
        memset(dst->fldname+err,0,sizeof(dst->fldname)-err); \
        mask->fldname[0]=1; \
        continue; \
      } \
    }
    
    INTFLD("id",gameid)
    STRFLD("platform",platform)
    STRFLD("author",author)
    STRFLD("genre",genre)
    INTFLD("rating",rating)
    KSTRFLD("name",name)
  
    #undef INTFLD
    #undef STRFLD
    #undef KSTRFLD
    
    if ((kc==7)&&!memcmp(k,"pubtime",7)) {
      char serialtime[32];
      int serialtimec=sr_decode_json_string(serialtime,sizeof(serialtime),&decoder);
      if ((serialtimec<0)||(serialtimec>sizeof(serialtime))) return -1;
      dst->pubtime=db_time_eval(serialtime,serialtimec);
      mask->pubtime=1;
      continue;
    }
    
    if ((kc==5)&&!memcmp(k,"flags",5)) {
      char serialflags[256];
      int serialflagsc=sr_decode_json_string(serialflags,sizeof(serialflags),&decoder);
      if ((serialflagsc<0)||(serialflagsc>sizeof(serialflags))) return -1;
      if (db_flags_eval(&dst->flags,serialflags,serialflagsc)<0) return -1;
      mask->flags=1;
      continue;
    }
    
    if ((kc==4)&&!memcmp(k,"path",4)) {
      char serialpath[1024];
      int serialpathc=sr_decode_json_string(serialpath,sizeof(serialpath),&decoder);
      if ((serialpathc<0)||(serialpathc>sizeof(serialpath))) return -1;
      if (db_game_set_path(db,dst,serialpath,serialpathc)<0) return -1;
      mask->dir=1;
      mask->base[0]=1;
      continue;
    }
  
    // Ignore unknown fields.
    if (sr_decode_json_skip(&decoder)<0) return -1;
  }
  if (sr_decode_json_end(&decoder,jsonctx)<0) return -1;
  return 0;
}

/* Patch from JSON.
 */

int db_game_patch_json(struct db *db,struct db_game *game,const char *src,int srcc) {

  struct db_game incoming={0},mask={0};
  if (db_game_decode_json(&incoming,&mask,db,src,srcc)<0) return -1;
  
  if (mask.gameid) {
    if (game&&(game->gameid!=incoming.gameid)) return -1;
  }
  
  if (!game) {
    if (!(game=db_game_insert(db,incoming.gameid))) return -1;
  }
  
  if (mask.platform) game->platform=incoming.platform;
  if (mask.author) game->author=incoming.author;
  if (mask.genre) game->genre=incoming.genre;
  if (mask.flags) game->flags=incoming.flags;
  if (mask.rating) game->rating=incoming.rating;
  if (mask.pubtime) game->pubtime=incoming.pubtime;
  if (mask.dir) game->dir=incoming.dir;
  
  if (mask.name[0]) memcpy(game->name,incoming.name,sizeof(incoming.name));
  if (mask.base[0]) memcpy(game->base,incoming.base,sizeof(incoming.base));
  
  db->dirty=1;
  return 0;
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

/* Encode blob.
 */
 
static int db_game_list_blob_cb(uint32_t gameid,const char *type,int typec,const char *time,int timec,const char *path,void *userdata) {
  struct sr_encoder *dst=userdata;
  return sr_encode_json_string(dst,0,0,path,-1);
}

/* Encode game as JSON.
 */
 
static int db_game_encode_json(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_game *game,
  int detail
) {
  if (!detail) detail=DB_DETAIL_record;
  
  // "id" detail is the only one that isn't a JSON object.
  if (detail==DB_DETAIL_id) {
    return sr_encode_json_int(dst,0,0,game->gameid);
  }
  
  /* Other formats all take {id,name}, and they build up in order.
   */
  if (detail<DB_DETAIL_name) return -1;
  int jsonctx=sr_encode_json_object_start(dst,0,0);
  if (jsonctx<0) return -1;
  
  if (sr_encode_json_int(dst,"id",2,game->gameid)<0) return -1;
  int namec=sizeof(game->name); while (namec&&!game->name[namec-1]) namec--;
  if (sr_encode_json_string(dst,"name",4,game->name,namec)<0) return -1;
  
  if (detail>=DB_DETAIL_record) {
    if (game->platform&&(db_encode_json_string(dst,db,"platform",8,game->platform)<0)) return -1;
    if (game->author&&(db_encode_json_string(dst,db,"author",6,game->author)<0)) return -1;
    if (game->genre&&(db_encode_json_string(dst,db,"genre",5,game->genre)<0)) return -1;
    if (game->flags) {
      char tmp[256];
      int tmpc=db_flags_repr(tmp,sizeof(tmp),game->flags);
      if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
      if (sr_encode_json_string(dst,"flags",5,tmp,tmpc)<0) return -1;
    }
    if (game->rating&&(db_encode_json_string(dst,db,"rating",6,game->rating)<0)) return -1;
    if (game->pubtime) {
      char tmp[32];
      int tmpc=db_time_repr(tmp,sizeof(tmp),game->pubtime);
      if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
      if (sr_encode_json_string(dst,"pubtime",7,tmp,tmpc)<0) return -1;
    }
    if (game->dir||game->base[0]) {
      char tmp[1024];
      int tmpc=db_game_get_path(tmp,sizeof(tmp),db,game);
      if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
      if (sr_encode_json_string(dst,"path",4,tmp,tmpc)<0) return -1;
    }
    
    if (detail>=DB_DETAIL_comments) {
      int subctx=sr_encode_json_array_start(dst,"comments",8);
      if (subctx<0) return -1;
      struct db_comment *comment=0;
      int commentc=db_comments_get_by_gameid(&comment,db,game->gameid);
      for (;commentc-->0;comment++) {
        if (db_comment_encode(dst,db,comment,DB_FORMAT_ingame)<0) return -1;
      }
      if (sr_encode_json_array_end(dst,subctx)<0) return -1;
      
      if (detail>=DB_DETAIL_plays) {
        if ((subctx=sr_encode_json_array_start(dst,"plays",5))<0) return -1;
        struct db_play *play=0;
        int playc=db_plays_get_by_gameid(&play,db,game->gameid);
        for (;playc-->0;play++) {
          if (db_play_encode(dst,db,play,DB_FORMAT_ingame)<0) return -1;
        }
        if (sr_encode_json_array_end(dst,subctx)<0) return -1;
        
        if (detail>=DB_DETAIL_lists) {
          if ((subctx=sr_encode_json_array_start(dst,"lists",5))<0) return -1;
          struct db_list **list=db->lists.v;
          int i=db->lists.c;
          for (;i-->0;list++) {
            if (!db_list_has(*list,game->gameid)) continue;
            int lctx=sr_encode_json_object_start(dst,0,0);
            if (lctx<0) return -1;
            if (sr_encode_json_int(dst,"id",2,(*list)->listid)<0) return -1;
            if (db_encode_json_string(dst,db,"name",4,(*list)->name)<0) return -1;
            if (sr_encode_json_object_end(dst,lctx)<0) return -1;
          }
          if (sr_encode_json_array_end(dst,subctx)<0) return -1;
          
          if (detail>=DB_DETAIL_blobs) {
            if ((subctx=sr_encode_json_array_start(dst,"blobs",5))<0) return -1;
            if (db_blob_for_gameid(db,game->gameid,1,db_game_list_blob_cb,dst)<0) return -1;
            if (sr_encode_json_array_end(dst,subctx)<0) return -1;
          }
        }
      }
    }
  }
  
  if (sr_encode_json_object_end(dst,jsonctx)<0) return -1;
  return 0;
}

/* Encode game, dispatch on format.
 */

int db_game_encode(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_game *game,
  int format,
  int detail
) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: return db_game_encode_json(dst,db,game,detail);
  }
  return -1;
}
