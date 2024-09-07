#include "db_internal.h"
#include "opt/serial/serial.h"
#include <stdarg.h>

/* Encode first member of a Table.
 */
 
static int _db_export_table_schema(struct sr_encoder *dst,...) {
  va_list vargs;
  va_start(vargs,dst);
  int jsonctx=sr_encode_json_array_start(dst,0,0);
  for (;;) {
    const char *k=va_arg(vargs,const char*);
    if (!k) break;
    if (sr_encode_json_string(dst,0,0,k,-1)<0) return -1;
  }
  return sr_encode_json_array_end(dst,jsonctx);
}

#define db_export_table_schema(dst,...) _db_export_table_schema(dst,##__VA_ARGS__,(void*)0)

/* Encode timestamp as a JSON string.
 */
 
static int db_encode_time(struct sr_encoder *dst,struct db *db,const char *k,int kc,uint32_t time) {
  if (!time) return sr_encode_json_string(dst,k,kc,"",0);
  char tmp[64];
  int tmpc=db_time_repr(tmp,sizeof(tmp),time);
  if ((tmpc<1)||(tmpc>sizeof(tmp))) tmpc=0;
  return sr_encode_json_string(dst,k,kc,tmp,tmpc);
}

/* Comments, within a game.
 */
 
static int db_export_comments(struct sr_encoder *dst,struct db *db,uint32_t gameid) {
  struct db_comment *comment;
  int commentc=db_comments_get_by_gameid(&comment,db,gameid);
  if (commentc<1) return sr_encode_json_int(dst,0,0,0);
  int jsonctx=sr_encode_json_array_start(dst,0,0);
  if (db_export_table_schema(dst,"time","k","v")<0) return -1;
  for (;commentc-->0;comment++) {
    int innerctx=sr_encode_json_array_start(dst,0,0);
    if (db_encode_time(dst,db,0,0,comment->time)<0) return -1;
    if (db_encode_json_string(dst,db,0,0,comment->k)<0) return -1;
    if (db_encode_json_string(dst,db,0,0,comment->v)<0) return -1;
    if (sr_encode_json_array_end(dst,innerctx)<0) return -1;
  }
  return sr_encode_json_array_end(dst,jsonctx);
}

/* Plays, within a game.
 */
 
static int db_export_plays(struct sr_encoder *dst,struct db *db,uint32_t gameid) {
  struct db_play *play;
  int playc=db_plays_get_by_gameid(&play,db,gameid);
  if (playc<1) return sr_encode_json_int(dst,0,0,0);
  int jsonctx=sr_encode_json_array_start(dst,0,0);
  if (db_export_table_schema(dst,"time","dur")<0) return -1;
  for (;playc-->0;play++) {
    int innerctx=sr_encode_json_array_start(dst,0,0);
    if (db_encode_time(dst,db,0,0,play->time)<0) return -1;
    sr_encode_json_int(dst,0,0,play->dur_m);
    if (sr_encode_json_array_end(dst,innerctx)<0) return -1;
  }
  return sr_encode_json_array_end(dst,jsonctx);
}

/* Export games.
 */
    
static int db_export_game(struct sr_encoder *dst,struct db *db,const struct db_game *game,uint32_t flags) {
  int jsonctx=sr_encode_json_array_start(dst,0,0);
  
  sr_encode_json_int(dst,0,0,game->gameid);
  int namec=0;
  while ((namec<sizeof(game->name))&&game->name[namec]) namec++;
  if (sr_encode_json_string(dst,0,0,game->name,namec)<0) return -1;
  
  char path[1024];
  int pathc=db_game_get_path(path,sizeof(path),db,game);
  if ((pathc<0)||(pathc>sizeof(path))) return -1;
  if (sr_encode_json_string(dst,0,0,path,pathc)<0) return -1;
  
  if (db_encode_json_string(dst,db,0,0,game->platform)<0) return -1;
  if (db_encode_json_string(dst,db,0,0,game->author)<0) return -1;
  if (db_encode_json_string(dst,db,0,0,game->genre)<0) return -1;
  
  int flagsctx=sr_encode_json_array_start(dst,0,0);
  if (game->flags) {
    uint32_t bit=1;
    for (;bit;bit<<=1) {
      if (!(game->flags&bit)) continue;
      if (sr_encode_json_string(dst,0,0,db_flag_repr(bit),-1)<0) return -1;
    }
  }
  if (sr_encode_json_array_end(dst,flagsctx)<0) return -1;
  
  sr_encode_json_int(dst,0,0,game->rating);
  if (db_encode_time(dst,db,0,0,game->pubtime)<0) return -1;
  
  if (flags&DB_EXPORT_comments) {
    if (db_export_comments(dst,db,game->gameid)<0) return -1;
  }
  
  if (flags&DB_EXPORT_plays) {
    if (db_export_plays(dst,db,game->gameid)<0) return -1;
  }

  return sr_encode_json_array_end(dst,jsonctx);
}
 
static int db_export_games(struct sr_encoder *dst,struct db *db,uint32_t flags) {
  int outerctx=sr_encode_json_array_start(dst,"games",5);
  
  /* Alas we don't have a constant schema.
   */
  int jsonctx=sr_encode_json_array_start(dst,0,0);
  sr_encode_json_string(dst,0,0,"gameid",6);
  sr_encode_json_string(dst,0,0,"name",4);
  sr_encode_json_string(dst,0,0,"platform",8);
  sr_encode_json_string(dst,0,0,"author",6);
  sr_encode_json_string(dst,0,0,"genre",5);
  sr_encode_json_string(dst,0,0,"flags",5);
  sr_encode_json_string(dst,0,0,"rating",6);
  sr_encode_json_string(dst,0,0,"pubtime",7);
  if (flags&DB_EXPORT_comments) {
    sr_encode_json_string(dst,0,0,"comments",8);
  }
  if (flags&DB_EXPORT_plays) {
    sr_encode_json_string(dst,0,0,"plays",5);
  }
  if (sr_encode_json_array_end(dst,jsonctx)<0) return -1;
  
  const struct db_game *game=db->games.v;
  int i=db->games.c;
  for (;i-->0;game++) {
    if (db_export_game(dst,db,game,flags)<0) return -1;
  }
  
  return sr_encode_json_array_end(dst,outerctx);
}

/* Export launchers.
 */
 
static int db_export_launcher(struct sr_encoder *dst,struct db *db,const struct db_launcher *launcher) {
  int jsonctx=sr_encode_json_array_start(dst,0,0);
  sr_encode_json_int(dst,0,0,launcher->launcherid);
  if (db_encode_json_string(dst,db,0,0,launcher->name)<0) return -1;
  if (db_encode_json_string(dst,db,0,0,launcher->platform)<0) return -1;
  if (db_encode_json_string(dst,db,0,0,launcher->suffixes)<0) return -1;
  if (db_encode_json_string(dst,db,0,0,launcher->cmd)<0) return -1;
  if (db_encode_json_string(dst,db,0,0,launcher->desc)<0) return -1;
  return sr_encode_json_array_end(dst,jsonctx);
}
 
static int db_export_launchers(struct sr_encoder *dst,struct db *db) {
  int jsonctx=sr_encode_json_array_start(dst,"launchers",9);
  if (db_export_table_schema(dst,"launcherid","name","platform","suffixes","cmd","desc")<0) return -1;
  const struct db_launcher *launcher=db->launchers.v;
  int i=db->launchers.c;
  for (;i-->0;launcher++) {
    if (db_export_launcher(dst,db,launcher)<0) return -1;
  }
  return sr_encode_json_array_end(dst,jsonctx);
}

/* Export upgrades.
 */
    
static int db_export_upgrade(struct sr_encoder *dst,struct db *db,const struct db_upgrade *upgrade) {
  int jsonctx=sr_encode_json_array_start(dst,0,0);
  sr_encode_json_int(dst,0,0,upgrade->upgradeid);
  if (db_encode_json_string(dst,db,0,0,upgrade->name)<0) return -1;
  if (db_encode_json_string(dst,db,0,0,upgrade->desc)<0) return -1;
  sr_encode_json_int(dst,0,0,upgrade->gameid);
  sr_encode_json_int(dst,0,0,upgrade->launcherid);
  sr_encode_json_int(dst,0,0,upgrade->depend);
  if (db_encode_json_string(dst,db,0,0,upgrade->method)<0) return -1;
  if (db_encode_json_string(dst,db,0,0,upgrade->param)<0) return -1;
  if (db_encode_time(dst,db,0,0,upgrade->checktime)<0) return -1;
  if (db_encode_time(dst,db,0,0,upgrade->buildtime)<0) return -1;
  if (db_encode_json_string(dst,db,0,0,upgrade->status)<0) return -1;
  return sr_encode_json_array_end(dst,jsonctx);
}
 
static int db_export_upgrades(struct sr_encoder *dst,struct db *db) {
  int jsonctx=sr_encode_json_array_start(dst,"upgrades",8);
  if (db_export_table_schema(dst,"upgradeid","name","desc","gameid","launcherid","depend","method","param","checktime","buildtime","status")<0) return -1;
  const struct db_upgrade *upgrade=db->upgrades.v;
  int i=db->upgrades.c;
  for (;i-->0;upgrade++) {
    if (db_export_upgrade(dst,db,upgrade)<0) return -1;
  }
  return sr_encode_json_array_end(dst,jsonctx);
}

/* Export lists.
 */
 
static int db_export_list(struct sr_encoder *dst,struct db *db,const struct db_list *list) {
  int jsonctx=sr_encode_json_array_start(dst,0,0);
  sr_encode_json_int(dst,0,0,list->listid);
  if (db_encode_json_string(dst,db,0,0,list->name)<0) return -1;
  if (db_encode_json_string(dst,db,0,0,list->desc)<0) return -1;
  sr_encode_json_int(dst,0,0,list->sorted);
  int actx=sr_encode_json_array_start(dst,0,0);
  const uint32_t *v=list->gameidv;
  int i=list->gameidc;
  for (;i-->0;v++) {
    if (sr_encode_json_int(dst,0,0,*v)<0) return -1;
  }
  if (sr_encode_json_array_end(dst,actx)<0) return -1;
  return sr_encode_json_array_end(dst,jsonctx);
}
 
static int db_export_lists(struct sr_encoder *dst,struct db *db) {
  int jsonctx=sr_encode_json_array_start(dst,"lists",5);
  if (db_export_table_schema(dst,"listid","name","desc","sorted","gameids")<0) return -1;
  struct db_list **p=db->lists.v;
  int i=db->lists.c;
  for (;i-->0;p++) {
    if (db_export_list(dst,db,*p)<0) return -1;
  }
  return sr_encode_json_array_end(dst,jsonctx);
}

/* Export blobs.
 * NB This one is not a table; it's just an array of basenames.
 */
 
static int db_export_blobs_cb(uint32_t gameid,const char *type,int typec,const char *time,int timec,const char *path,void *userdata) {
  struct sr_encoder *dst=userdata;
  const char *base=path;
  int i=0; for (;path[i];i++) if (path[i]=='/') base=path+i+1;
  if (sr_encode_json_string(dst,0,0,base,-1)<0) return -1;
  return 0;
}
 
static int db_export_blobs(struct sr_encoder *dst,struct db *db) {
  int jsonctx=sr_encode_json_array_start(dst,"blobs",5);
  if (db_blob_for_each(db,0,db_export_blobs_cb,dst)<0) return -1;
  return sr_encode_json_array_end(dst,jsonctx);
}

/* Export, main entry point.
 */
 
int db_export(struct sr_encoder *dst,struct db *db,uint32_t flags) {
  if (!dst||!db) return -1;
  int jsonctx=sr_encode_json_object_start(dst,0,0);
  if (jsonctx<0) return -1;
  
  if (db_export_games(dst,db,flags)<0) return -1;
  if (flags&DB_EXPORT_launchers) {
    if (db_export_launchers(dst,db)<0) return -1;
  }
  if (flags&DB_EXPORT_upgrades) {
    if (db_export_upgrades(dst,db)<0) return -1;
  }
  if (flags&DB_EXPORT_lists) {
    if (db_export_lists(dst,db)<0) return -1;
  }
  if (flags&DB_EXPORT_blobs) {
    if (db_export_blobs(dst,db)<0) return -1;
  }
  
  if (sr_encode_json_object_end(dst,jsonctx)<0) return -1;
  return 0;
}
