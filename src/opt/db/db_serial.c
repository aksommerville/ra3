#include "db_internal.h"
#include "opt/serial/serial.h"

/* Helpers while decoding an object.
 */
 
#define DECODE_STR(recname,fldname,key) if ((kc==sizeof(key)-1)&&!memcmp(k,key,kc)) { \
  if (db_decode_json_string(&recname->fldname,db,&decoder)<0) return -1; \
  continue; \
}

#define DECODE_KSTR(recname,fldname,key) if ((kc==sizeof(key)-1)&&!memcmp(k,key,kc)) { \
  int dstc=sr_decode_json_string(recname->fldname,sizeof(recname->fldname),&decoder); \
  if (dstc<0) return -1; \
  if (dstc>sizeof(recname->fldname)) return -1; \
  memset(recname->fldname+dstc,0,sizeof(recname->fldname)-dstc); \
  continue; \
}

#define DECODE_INT(recname,fldname,key) if ((kc==sizeof(key)-1)&&!memcmp(k,key,kc)) { \
  int iv; \
  if (sr_decode_json_int(&iv,&decoder)<0) return -1; \
  recname->fldname=iv; \
  continue; \
}

#define DECODE_BOOL(recname,fldname,key) if ((kc==sizeof(key)-1)&&!memcmp(k,key,kc)) { \
  int iv=sr_decode_json_boolean(&decoder); \
  if (iv<0) return -1; \
  recname->fldname=iv; \
  continue; \
}

#define DECODE_TIME(recname,fldname,key) if ((kc==sizeof(key)-1)&&!memcmp(k,key,kc)) { \
  char timesrc[32]; \
  int timesrcc=sr_decode_json_string(timesrc,sizeof(timesrc),&decoder); \
  if ((timesrcc<0)||(timesrcc>sizeof(timesrc))) return -1; \
  recname->fldname=db_time_eval(timesrc,timesrcc); \
  continue; \
}

#define ENCODE_TIME(k,kc,tsrc) { \
  char ttext[32]; \
  int ttextc=db_time_repr(ttext,sizeof(ttext),tsrc); \
  if ((ttextc<0)||(ttextc>sizeof(ttext))) return -1; \
  if (sr_encode_json_string(dst,k,kc,ttext,ttextc)<0) return -1; \
}

#define ENCODE_KSTRING(k,kc,fld) { \
  int srcc=sizeof(fld); \
  while (srcc&&!fld[srcc-1]) srcc--; \
  if (sr_encode_json_string(dst,k,kc,fld,srcc)<0) return -1; \
}

/* Game, encode JSON.
 * (detail) is in order. Generally, greater values include all values below.
 */
 
static int db_game_encode_json_comments(struct sr_encoder *dst,const struct db *db,const struct db_game *game) {
  struct db_comment *comment;
  int commentc=db_comments_get_by_gameid(&comment,db,game->gameid);
  if (commentc<1) return 0;
  int jsonctx=sr_encode_json_array_start(dst,"comments",8);
  if (jsonctx<0) return -1;
  for (;commentc-->0;comment++) {
    if (sr_encode_json_setup(dst,0,0)<0) return -1;
    if (db_comment_encode(dst,db,comment,DB_FORMAT_ingame,0)<0) return -1;
  }
  return sr_encode_json_array_end(dst,jsonctx);
}

static int db_game_encode_json_plays(struct sr_encoder *dst,const struct db *db,const struct db_game *game) {
  struct db_play *play;
  int playc=db_plays_get_by_gameid(&play,db,game->gameid);
  if (playc<1) return 0;
  int jsonctx=sr_encode_json_array_start(dst,"plays",5);
  if (jsonctx<0) return -1;
  for (;playc-->0;play++) {
    if (sr_encode_json_setup(dst,0,0)<0) return -1;
    if (db_play_encode(dst,db,play,DB_FORMAT_ingame,0)<0) return -1;
  }
  return sr_encode_json_array_end(dst,jsonctx);
}

static int db_game_encode_json_lists(struct sr_encoder *dst,const struct db *db,const struct db_game *game) {
  struct db_list **list=db->lists.v;
  int listc=db->lists.c;
  int jsonctx=sr_encode_json_array_start(dst,"lists",5);
  if (jsonctx<0) return -1;
  for (;listc-->0;list++) {
    if (!db_list_has(*list,game->gameid)) continue;
    int innerctx=sr_encode_json_object_start(dst,0,0);
    if (innerctx<0) return -1;
    sr_encode_json_int(dst,"listid",6,(*list)->listid);
    if (db_encode_json_string(dst,db,"name",4,(*list)->name)<0) return -1;
    if (sr_encode_json_object_end(dst,innerctx)<0) return -1;
  }
  return sr_encode_json_array_end(dst,jsonctx);
}

static int db_game_encode_json_blobs_cb(uint32_t gameid,const char *type,int typec,const char *time,int timec,const char *path,void *userdata) {
  if (sr_encode_json_string(userdata,0,0,path,-1)<0) return -1;
  return 0;
}

static int db_game_encode_json_blobs(struct sr_encoder *dst,const struct db *db,const struct db_game *game) {
  int jsonctx=sr_encode_json_array_start(dst,"blobs",5);
  if (jsonctx<0) return -1;
  if (db_blob_for_gameid((struct db*)db,game->gameid,1,db_game_encode_json_blobs_cb,dst)<0) return -1;
  return sr_encode_json_array_end(dst,jsonctx);
}
 
static int db_game_encode_json(struct sr_encoder *dst,const struct db *db,const struct db_game *game,int detail) {

  // "id" is unlike other details: It's a plain JSON int instead of an object.
  if (detail<=DB_DETAIL_id) {
    return sr_encode_json_int(dst,0,0,game->gameid);
  }

  int jsonctx=sr_encode_json_object_start_no_setup(dst);
  if (jsonctx<0) return -1;
  
  sr_encode_json_int(dst,"gameid",6,game->gameid);
  if (game->name[0]) ENCODE_KSTRING("name",4,game->name)
  if (detail<=DB_DETAIL_name) goto _done_;
  
  if (game->platform&&(db_encode_json_string(dst,db,"platform",8,game->platform)<0)) return -1;
  if (game->author&&(db_encode_json_string(dst,db,"author",6,game->author)<0)) return -1;
  if (game->genre&&(db_encode_json_string(dst,db,"genre",5,game->genre)<0)) return -1;
  if (game->flags) {
    char text[256];
    int textc=db_flags_repr(text,sizeof(text),game->flags);
    if ((textc<1)||(textc>sizeof(text))) return -1;
    sr_encode_json_string(dst,"flags",5,text,textc);
  }
  if (game->rating) sr_encode_json_int(dst,"rating",6,game->rating);
  if (game->pubtime) ENCODE_TIME("pubtime",7,game->pubtime);
  if (game->dir||game->base[0]) {
    char path[1024];
    int pathc=db_game_get_path(path,sizeof(path),db,game);
    if ((pathc<1)||(pathc>=sizeof(path))) return -1;
    sr_encode_json_string(dst,"path",4,path,pathc);
  }
  if (detail<=DB_DETAIL_record) goto _done_;

  if (db_game_encode_json_comments(dst,db,game)<0) return -1;
  if (detail<=DB_DETAIL_comments) goto _done_;
  
  if (db_game_encode_json_plays(dst,db,game)<0) return -1;
  if (detail<=DB_DETAIL_plays) goto _done_;
  
  if (db_game_encode_json_lists(dst,db,game)<0) return -1;
  if (detail<=DB_DETAIL_lists) goto _done_;
  
  if (db_game_encode_json_blobs(dst,db,game)<0) return -1;
  if (detail<=DB_DETAIL_blobs) goto _done_;
  
 _done_:;
  return sr_encode_json_object_end(dst,jsonctx);
}

/* Game, decode JSON.
 */
 
static int db_game_decode_json(struct db_game *game,struct db *db,const void *src,int srcc) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  
  if (sr_decode_json_peek(&decoder)!='{') {
    int v;
    if (sr_decode_json_int(&v,&decoder)<0) return -1;
    game->gameid=v;
    return 0;
  }
  
  int jsonctx=sr_decode_json_object_start(&decoder);
  if (jsonctx<0) return -1;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
  
    DECODE_INT(game,gameid,"gameid")
    DECODE_KSTR(game,name,"name")
    DECODE_STR(game,platform,"platform")
    DECODE_STR(game,author,"author")
    DECODE_STR(game,genre,"genre")
    DECODE_INT(game,rating,"rating")
    DECODE_TIME(game,pubtime,"pubtime")
    
    if ((kc==5)&&!memcmp(k,"flags",5)) {
      char text[256];
      int textc=sr_decode_json_string(text,sizeof(text),&decoder);
      if ((textc<0)||(textc>sizeof(text))) return -1;
      if (db_flags_eval(&game->flags,text,textc)<0) return -1;
      continue;
    }
    
    if ((kc==4)&&!memcmp(k,"path",4)) {
      char text[1024];
      int textc=sr_decode_json_string(text,sizeof(text),&decoder);
      if ((textc<0)||(textc>=sizeof(text))) return -1;
      if (db_game_set_path(db,game,text,textc)<0) return -1;
      continue;
    }
    
    if (sr_decode_json_skip(&decoder)<0) return -1;
  }
  return sr_decode_json_end(&decoder,jsonctx);
}

/* Game, dispatch.
 */
 
int db_game_encode(struct sr_encoder *dst,const struct db *db,const struct db_game *game,int format,int detail) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: return db_game_encode_json(dst,db,game,detail);
  }
  return -1;
}

int db_game_decode(struct db_game *game,struct db *db,int format,const void *src,int srcc) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: return db_game_decode_json(game,db,src,srcc);
  }
  return -1;
}

/* Comment, encode JSON.
 */
 
static int db_comment_encode_json(struct sr_encoder *dst,const struct db *db,const struct db_comment *comment,int detail,int include_gameid) {
  int jsonctx=sr_encode_json_object_start_no_setup(dst);
  if (jsonctx<0) return -1;
  
  if (include_gameid) sr_encode_json_int(dst,"gameid",6,comment->gameid);
  ENCODE_TIME("time",4,comment->time)
  if (db_encode_json_string(dst,db,"k",1,comment->k)<0) return -1;
  if (db_encode_json_string(dst,db,"v",1,comment->v)<0) return -1;
  
  return sr_encode_json_object_end(dst,jsonctx);
}

/* Comment, decode JSON.
 */
 
static int db_comment_decode_json(struct db_comment *comment,struct db *db,const void *src,int srcc) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  int jsonctx=sr_decode_json_object_start(&decoder);
  if (jsonctx<0) return -1;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
  
    DECODE_INT(comment,gameid,"gameid")
    DECODE_TIME(comment,time,"time")
    DECODE_STR(comment,k,"k")
    DECODE_STR(comment,v,"v")
    
    if (sr_decode_json_skip(&decoder)<0) return -1;
  }
  return sr_decode_json_end(&decoder,jsonctx);
}

/* Comment, dispatch.
 */

int db_comment_encode(struct sr_encoder *dst,const struct db *db,const struct db_comment *comment,int format,int detail) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: return db_comment_encode_json(dst,db,comment,detail,1);
    case DB_FORMAT_ingame: return db_comment_encode_json(dst,db,comment,detail,0);
  }
  return -1;
}

int db_comment_decode(struct db_comment *comment,struct db *db,int format,const void *src,int srcc) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: 
    case DB_FORMAT_ingame: return db_comment_decode_json(comment,db,src,srcc);
  }
  return -1;
}

/* Play, encode JSON.
 */
 
static int db_play_encode_json(struct sr_encoder *dst,const struct db *db,const struct db_play *play,int detail,int include_gameid) {
  int jsonctx=sr_encode_json_object_start_no_setup(dst);
  if (jsonctx<0) return -1;
  
  if (include_gameid) {
    sr_encode_json_int(dst,"gameid",6,play->gameid);
  }
  ENCODE_TIME("time",4,play->time)
  sr_encode_json_int(dst,"dur_m",5,play->dur_m);
  
  return sr_encode_json_object_end(dst,jsonctx);
}

/* Play, decode JSON.
 */
 
static int db_play_decode_json(struct db_play *play,struct db *db,const void *src,int srcc) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  int jsonctx=sr_decode_json_object_start(&decoder);
  if (jsonctx<0) return -1;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    
    DECODE_INT(play,gameid,"gameid")
    DECODE_TIME(play,time,"time")
    DECODE_INT(play,dur_m,"dur_m")
    
    if (sr_decode_json_skip(&decoder)<0) return -1;
  }
  return sr_decode_json_end(&decoder,jsonctx);
}

/* Play, dispatch.
 */

int db_play_encode(struct sr_encoder *dst,const struct db *db,const struct db_play *play,int format,int detail) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: return db_play_encode_json(dst,db,play,detail,1);
    case DB_FORMAT_ingame: return db_play_encode_json(dst,db,play,detail,0);
  }
  return -1;
}

int db_play_decode(struct db_play *play,struct db *db,int format,const void *src,int srcc) {
  switch (format) {
    case 0:
    case DB_FORMAT_json:
    case DB_FORMAT_ingame: return db_play_decode_json(play,db,src,srcc);
  }
  return -1;
}

/* Launcher, encode JSON.
 */
 
static int db_launcher_encode_json(struct sr_encoder *dst,const struct db *db,const struct db_launcher *launcher,int detail) {
  int jsonctx=sr_encode_json_object_start_no_setup(dst);
  if (jsonctx<0) return -1;
  
  sr_encode_json_int(dst,"launcherid",10,launcher->launcherid);
  db_encode_json_string(dst,db,"name",4,launcher->name);
  db_encode_json_string(dst,db,"platform",8,launcher->platform);
  db_encode_json_string(dst,db,"suffixes",8,launcher->suffixes);
  db_encode_json_string(dst,db,"cmd",3,launcher->cmd);
  db_encode_json_string(dst,db,"desc",4,launcher->desc);
  
  return sr_encode_json_object_end(dst,jsonctx);
}

/* Launcher, decode JSON.
 */
 
static int db_launcher_decode_json(struct db_launcher *launcher,struct db *db,const void *src,int srcc) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  int jsonctx=sr_decode_json_object_start(&decoder);
  if (jsonctx<0) return -1;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    
    DECODE_INT(launcher,launcherid,"launcherid")
    DECODE_STR(launcher,name,"name")
    DECODE_STR(launcher,platform,"platform")
    DECODE_STR(launcher,suffixes,"suffixes")
    DECODE_STR(launcher,cmd,"cmd")
    DECODE_STR(launcher,desc,"desc")
    
    if (sr_decode_json_skip(&decoder)<0) return -1;
  }
  return sr_decode_json_end(&decoder,jsonctx);
}

/* Launcher, dispatch.
 */

int db_launcher_encode(struct sr_encoder *dst,const struct db *db,const struct db_launcher *launcher,int format,int detail) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: return db_launcher_encode_json(dst,db,launcher,detail);
  }
  return -1;
}

int db_launcher_decode(struct db_launcher *launcher,struct db *db,int format,const void *src,int srcc) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: return db_launcher_decode_json(launcher,db,src,srcc);
  }
  return -1;
}

/* Upgrade, encode JSON.
 */
 
static int db_upgrade_encode_json(struct sr_encoder *dst,const struct db *db,const struct db_upgrade *upgrade) {
  int jsonctx=sr_encode_json_object_start_no_setup(dst);
  if (jsonctx<0) return -1;
  
  sr_encode_json_int(dst,"upgradeid",9,upgrade->upgradeid);
  db_encode_json_string(dst,db,"name",4,upgrade->name);
  if (upgrade->desc) db_encode_json_string(dst,db,"desc",4,upgrade->desc);
  if (upgrade->gameid) sr_encode_json_int(dst,"gameid",6,upgrade->gameid);
  if (upgrade->launcherid) sr_encode_json_int(dst,"launcherid",10,upgrade->launcherid);
  if (upgrade->depend) sr_encode_json_int(dst,"depend",6,upgrade->depend);
  if (upgrade->method) db_encode_json_string(dst,db,"method",6,upgrade->method);
  if (upgrade->param) db_encode_json_string(dst,db,"param",5,upgrade->param);
  if (upgrade->checktime) ENCODE_TIME("checktime",9,upgrade->checktime)
  if (upgrade->buildtime) ENCODE_TIME("buildtime",9,upgrade->buildtime)
  if (upgrade->status) db_encode_json_string(dst,db,"status",6,upgrade->status);
  
  /* Encoded upgrades have a read-only "displayName" field, to spare clients the need of looking up the game or launcher for display purposes.
   * If displayName comes back to us we ignore it.
   */
  if (upgrade->name) {
    db_encode_json_string(dst,db,"displayName",11,upgrade->name);
  } else if (upgrade->gameid) {
    const struct db_game *game=db_game_get_by_id(db,upgrade->gameid);
    if (game) {
      int namec=DB_GAME_NAME_LIMIT;
      while (namec&&!game->name[namec-1]) namec--;
      sr_encode_json_string(dst,"displayName",11,game->name,namec);
    }
  } else if (upgrade->launcherid) {
    const struct db_launcher *launcher=db_launcher_get_by_id(db,upgrade->launcherid);
    if (launcher) db_encode_json_string(dst,db,"displayName",11,launcher->name);
  }
  
  return sr_encode_json_object_end(dst,jsonctx);
}

/* Upgrade, decode JSON.
 */
 
static int db_upgrade_decode_json(struct db_upgrade *upgrade,struct db *db,const void *src,int srcc) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  int jsonctx=sr_decode_json_object_start(&decoder);
  if (jsonctx<0) return -1;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    
    DECODE_INT(upgrade,upgradeid,"upgradeid")
    DECODE_STR(upgrade,name,"name")
    DECODE_STR(upgrade,desc,"desc")
    DECODE_INT(upgrade,gameid,"gameid")
    DECODE_INT(upgrade,launcherid,"launcherid")
    DECODE_INT(upgrade,depend,"depend")
    DECODE_STR(upgrade,method,"method")
    DECODE_STR(upgrade,param,"param")
    DECODE_TIME(upgrade,checktime,"checktime")
    DECODE_TIME(upgrade,buildtime,"buildtime")
    DECODE_STR(upgrade,status,"status")
    
    if (sr_decode_json_skip(&decoder)<0) return -1;
  }
  return sr_decode_json_end(&decoder,jsonctx);
}

/* Upgrade, dispatch.
 */
 
int db_upgrade_encode(struct sr_encoder *dst,const struct db *db,const struct db_upgrade *upgrade,int format,int detail) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: return db_upgrade_encode_json(dst,db,upgrade);
  }
  return -1;
}

int db_upgrade_decode(struct db_upgrade *upgrade,struct db *db,int format,const void *src,int srcc) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: return db_upgrade_decode_json(upgrade,db,src,srcc);
  }
  return -1;
}

/* List, encode JSON.
 */
 
static int db_list_encode_json_array(struct sr_encoder *dst,const struct db *db,const struct db_list *list,int detail) {
  int jsonctx=sr_encode_json_array_start_no_setup(dst);
  if (jsonctx<0) return -1;
  
  if (list->gameidc==list->gamec) {
    const struct db_game *game=list->gamev;
    int i=list->gamec;
    for (;i-->0;game++) {
      if (sr_encode_json_setup(dst,0,0)<0) return -1;
      if (db_game_encode(dst,db,game,DB_FORMAT_json,detail)<0) return -1;
    }
    
  } else if (detail==DB_DETAIL_id) {
    const uint32_t *gameid=list->gameidv;
    int i=list->gameidc;
    for (;i-->0;gameid++) {
      if (sr_encode_json_int(dst,0,0,*gameid)<0) return -1;
    }
    
  } else {
    const uint32_t *gameid=list->gameidv;
    int i=list->gameidc;
    for (;i-->0;gameid++) {
      const struct db_game *game=db_game_get_by_id(db,*gameid);
      if (!game) return -1;
      if (sr_encode_json_setup(dst,0,0)<0) return -1;
      if (db_game_encode(dst,db,game,DB_FORMAT_json,detail)<0) return -1;
    }
  }
  
  return sr_encode_json_array_end(dst,jsonctx);
}
 
static int db_list_encode_json(struct sr_encoder *dst,const struct db *db,const struct db_list *list,int detail) {
  int jsonctx=sr_encode_json_object_start_no_setup(dst);
  if (jsonctx<0) return -1;
  
  sr_encode_json_int(dst,"listid",6,list->listid);
  if (list->name) db_encode_json_string(dst,db,"name",4,list->name);
  if (list->desc) db_encode_json_string(dst,db,"desc",4,list->desc);
  sr_encode_json_boolean(dst,"sorted",6,list->sorted);
  if (sr_encode_json_setup(dst,"games",5)<0) return -1;
  if (db_list_encode_json_array(dst,db,list,detail)<0) return -1;
  
  return sr_encode_json_object_end(dst,jsonctx);
}

/* List, decode JSON.
 */
 
static uint32_t db_list_extract_gameid_from_json(struct sr_decoder *decoder) {
  switch (sr_decode_json_peek(decoder)) {
  
    case '{': {
        uint32_t gameid=0;
        int jsonctx=sr_decode_json_object_start(decoder);
        if (jsonctx<0) return -1;
        const char *k=0;
        int kc=0;
        while ((kc=sr_decode_json_next(&k,decoder))>0) {
          if ((kc==6)&&!memcmp(k,"gameid",6)) {
            int n=0;
            if (sr_decode_json_int(&n,decoder)<0) return -1;
            gameid=n;
          } else {
            if (sr_decode_json_skip(decoder)<0) return -1;
          }
        }
        if (sr_decode_json_end(decoder,jsonctx)<0) return -1;
        return gameid;
      }
    
    default: {
        int n;
        if (sr_decode_json_int(&n,decoder)<0) return -1;
        return n;
      }
  }
}
 
static int db_list_decode_json_array(struct db_list *list,struct db *db,const void *src,int srcc) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  int jsonctx=sr_decode_json_array_start(&decoder);
  if (jsonctx<0) return -1;
  while (sr_decode_json_next(0,&decoder)>0) {
    uint32_t gameid=db_list_extract_gameid_from_json(&decoder);
    if (!gameid) return -1;
    if (db_list_append(db,list,gameid)<0) return -1;
  }
  return sr_decode_json_end(&decoder,jsonctx);
}

static int db_list_decode_json(struct db_list *list,struct db *db,const void *src,int srcc) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  int jsonctx=sr_decode_json_object_start(&decoder);
  if (jsonctx<0) return -1;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    
    DECODE_INT(list,listid,"listid")
    DECODE_STR(list,name,"name")
    DECODE_STR(list,desc,"desc")
    DECODE_BOOL(list,sorted,"sorted")
    
    if ((kc==5)&&!memcmp(k,"games",5)) {
      int subctx=sr_decode_json_array_start(&decoder);
      if (subctx<0) return -1;
      while (sr_decode_json_next(0,&decoder)>0) {
        uint32_t gameid=db_list_extract_gameid_from_json(&decoder);
        if (!gameid) return -1;
        if (db_list_append(db,list,gameid)<0) return -1;
      }
      if (sr_decode_json_end(&decoder,subctx)<0) return -1;
      continue;
    }
    
    if (sr_decode_json_skip(&decoder)<0) return -1;
  }
  return sr_decode_json_end(&decoder,jsonctx);
}

/* List, dispatch.
 */

int db_list_encode(struct sr_encoder *dst,const struct db *db,const struct db_list *list,int format,int detail) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: return db_list_encode_json(dst,db,list,detail);
  }
  return -1;
}

int db_list_encode_array(struct sr_encoder *dst,const struct db *db,const struct db_list *list,int format,int detail) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: return db_list_encode_json_array(dst,db,list,detail);
  }
  return -1;
}

int db_list_decode(struct db_list *list,struct db *db,int format,const void *src,int srcc) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: return db_list_decode_json(list,db,src,srcc);
  }
  return -1;
}

int db_list_decode_array(struct db_list *list,struct db *db,int format,const void *src,int srcc) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: return db_list_decode_json_array(list,db,src,srcc);
  }
  return -1;
}
