#include "db_internal.h"
#include "opt/serial/serial.h"

/* Nonzero if a search string is present in a comma-delimited list.
 */
 
static int db_string_in_comma_list(const char *q,int qc,const char *list,int listc) {
  int listp=0;
  while (listp<listc) {
    if ((unsigned char)list[listp]<=0x20) {
      listp++;
      continue;
    }
    const char *token=list+listp;
    int tokenc=0;
    while ((listp<listc)&&(list[listp++]!=',')) tokenc++;
    while (tokenc&&((unsigned char)token[tokenc-1]<=0x20)) tokenc--;
    if (tokenc!=qc) continue;
    if (memcmp(token,q,qc)) continue;
    return 1;
  }
  return 0;
}

/* Look up a game and select the most appropriate launcher.
 */
 
struct db_launcher *db_launcher_for_gameid(const struct db *db,uint32_t gameid) {
  struct db_game *game=db_game_get_by_id(db,gameid);
  if (!game) return 0;
  
  // Normalize suffix from basename.
  char sfx[16];
  int sfxc=0,basep=0;
  for (;(basep<sizeof(game->base))&&game->base[basep];basep++) {
    char ch=game->base[basep];
    if (ch=='.') {
      sfxc=0;
    } else if (sfxc>=sizeof(sfx)) {
      sfx[0]=0;
    } else if ((ch>='A')&&(ch<='Z')) {
      sfx[sfxc++]=ch+0x20;
    } else {
      sfx[sfxc++]=ch;
    }
  }
  if (!sfx[0]) sfxc=0;
  
  // Check each launcher.
  struct db_launcher *best=0;
  int bestscore=0;
  struct db_launcher *q=db->launchers.v;
  int i=db->launchers.c;
  for (i=db->launchers.c;i-->0;q++) {
    int score=0;
  
    if (game->platform&&(game->platform==q->platform)) score+=100;
    else if (game->platform&&q->platform) score-=100;
    
    if (q->suffixes&&sfxc) {
      const char *suffixes;
      int suffixesc=db_string_get(&suffixes,db,q->suffixes);
      if (suffixesc>0) {
        if (db_string_in_comma_list(sfx,sfxc,suffixes,suffixesc)) score+=100;
        else score-=100;
      }
    }
    
    if (score>bestscore) {
      best=q;
      bestscore=score;
    }
  }
  return best;
}

/* Direct store access.
 */
 
int db_launcher_count(const struct db *db) {
  return db->launchers.c;
}

struct db_launcher *db_launcher_get_by_index(const struct db *db,int p) {
  return db_flatstore_get(&db->launchers,p);
}

struct db_launcher *db_launcher_get_by_id(const struct db *db,uint32_t launcherid) {
  return db_flatstore_get(&db->launchers,db_flatstore_search1(&db->launchers,launcherid));
}

/* New launcher.
 */
 
struct db_launcher *db_launcher_insert(struct db *db,uint32_t launcherid) {
  if (!launcherid) launcherid=db_flatstore_next_id(&db->launchers);
  int p=db_flatstore_search1(&db->launchers,launcherid);
  if (p>=0) return 0;
  p=-p-1;
  struct db_launcher *launcher=db_flatstore_insert1(&db->launchers,p,launcherid);
  if (!launcher) return 0;
  db->dirty=1;
  return launcher;
}

/* Delete launcher.
 */
 
int db_launcher_delete(struct db *db,uint32_t launcherid) {
  int p=db_flatstore_search1(&db->launchers,launcherid);
  if (p<0) return -1;
  db_flatstore_remove(&db->launchers,p,1);
  db->dirty=1;
  return 0;
}

/* Decode JSON into temporary record and visit list.
 */
 
static int db_launcher_decode_json(
  struct db_launcher *dst,
  struct db_launcher *mask,
  struct db *db,
  const char *src,int srcc
) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_object_start(&decoder)<0) return -1;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
  
    if (
      ((kc==2)&&!memcmp(k,"id",2))||
      ((kc==10)&&!memcmp(k,"launcherid",10))
    ) {
      int n;
      if (sr_decode_json_int(&n,&decoder)<0) return -1;
      dst->launcherid=n;
      mask->launcherid=1;
      continue;
    }
    
    #define STRFLD(tag) { \
      if ((kc==sizeof(#tag)-1)&&!memcmp(k,#tag,kc)) { \
        if (db_decode_json_string(&dst->tag,db,&decoder)<0) return -1; \
        mask->tag=1; \
        continue; \
      } \
    }
    STRFLD(name)
    STRFLD(platform)
    STRFLD(suffixes)
    STRFLD(cmd)
    STRFLD(desc)
    #undef STRFLD
  
    if (sr_decode_json_skip(&decoder)<0) return -1;
  }
  if (sr_decode_json_end(&decoder,0)<0) return -1;
  return 0;
}

/* Patch from JSON.
 */
 
int db_launcher_patch_json(struct db *db,struct db_launcher *launcher,const char *src,int srcc) {
  struct db_launcher scratch={0},mask={0};
  if (db_launcher_decode_json(&scratch,&mask,db,src,srcc)<0) return -1;
  
  if (!launcher) {
    if (!(launcher=db_launcher_insert(db,scratch.launcherid))) return -1;
  }
  
  if (mask.name) launcher->name=scratch.name;
  if (mask.platform) launcher->platform=scratch.platform;
  if (mask.suffixes) launcher->suffixes=scratch.suffixes;
  if (mask.cmd) launcher->cmd=scratch.cmd;
  if (mask.desc) launcher->desc=scratch.desc;
  
  db->launchers.dirty=db->dirty=1;
  return 0;
}

/* Manual edit.
 */
 
int db_launcher_set_name(struct db *db,struct db_launcher *launcher,const char *src,int srcc) {
  launcher->name=db_string_intern(db,src,srcc);
  db->launchers.dirty=db->dirty=1;
  return 0;
}

int db_launcher_set_platform(struct db *db,struct db_launcher *launcher,const char *src,int srcc) {
  launcher->platform=db_string_intern(db,src,srcc);
  db->launchers.dirty=db->dirty=1;
  return 0;
}

int db_launcher_set_suffixes(struct db *db,struct db_launcher *launcher,const char *src,int srcc) {
  launcher->suffixes=db_string_intern(db,src,srcc);
  db->launchers.dirty=db->dirty=1;
  return 0;
}

int db_launcher_set_cmd(struct db *db,struct db_launcher *launcher,const char *src,int srcc) {
  launcher->cmd=db_string_intern(db,src,srcc);
  db->launchers.dirty=db->dirty=1;
  return 0;
}

int db_launcher_set_desc(struct db *db,struct db_launcher *launcher,const char *src,int srcc) {
  launcher->desc=db_string_intern(db,src,srcc);
  db->launchers.dirty=db->dirty=1;
  return 0;
}

void db_launcher_dirty(struct db *db,struct db_launcher *launcher) {
  db->launchers.dirty=db->dirty=1;
}

/* Encode JSON.
 */
 
static int db_launcher_encode_json(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_launcher *launcher
) {
  int jsonctx=sr_encode_json_object_start(dst,0,0);
  if (jsonctx<0) return -1;
  
  if (sr_encode_json_int(dst,"id",2,launcher->launcherid)<0) return -1;
  if (launcher->name&&(db_encode_json_string(dst,db,"name",4,launcher->name)<0)) return -1;
  if (launcher->platform&&(db_encode_json_string(dst,db,"platform",8,launcher->platform)<0)) return -1;
  if (launcher->suffixes&&(db_encode_json_string(dst,db,"suffixes",8,launcher->suffixes)<0)) return -1;
  if (launcher->cmd&&(db_encode_json_string(dst,db,"cmd",3,launcher->cmd)<0)) return -1;
  if (launcher->desc&&(db_encode_json_string(dst,db,"desc",4,launcher->desc)<0)) return -1;
  
  if (sr_encode_json_object_end(dst,jsonctx)<0) return -1;
  return 0;
}

/* Encode, dispatch on format.
 */
 
int db_launcher_encode(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_launcher *launcher,
  int format
) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: return db_launcher_encode_json(dst,db,launcher);
  }
  return -1;
}
