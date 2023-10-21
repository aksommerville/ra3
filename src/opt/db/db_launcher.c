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

/* Delete launcher.
 */
 
int db_launcher_delete(struct db *db,uint32_t launcherid) {
  int p=db_flatstore_search1(&db->launchers,launcherid);
  if (p<0) return -1;
  db_flatstore_remove(&db->launchers,p,1);
  db->dirty=1;
  return 0;
}

/* Insert.
 */

struct db_launcher *db_launcher_insert(struct db *db,const struct db_launcher *launcher) {
  uint32_t launcherid=launcher->launcherid;
  if (!launcherid) launcherid=db_flatstore_next_id(&db->launchers);
  int p=db_flatstore_search1(&db->launchers,launcherid);
  if (p>=0) return 0;
  p=-p-1;
  struct db_launcher *real=db_flatstore_insert1(&db->launchers,p,launcherid);
  if (!real) return 0;
  memcpy(real,launcher,sizeof(struct db_launcher));
  real->launcherid=launcherid;
  db->dirty=1;
  return real;
}

/* Update.
 */

struct db_launcher *db_launcher_update(struct db *db,const struct db_launcher *launcher) {
  int p=db_flatstore_search1(&db->launchers,launcher->launcherid);
  if (p<0) return 0;
  struct db_launcher *real=db_flatstore_get(&db->launchers,p);
  memcpy(real,launcher,sizeof(struct db_launcher));
  db->dirty=db->launchers.dirty=1;
  return real;
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
