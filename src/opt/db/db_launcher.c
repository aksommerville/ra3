#include "db_internal.h"
#include "opt/serial/serial.h"

/* Nonzero if a search string is present in a comma-delimited list.
 */

static int db_suffix_in_list(const char *sfx,int sfxc,const char *list,int listc) {
  while (sfxc>0) {
    int listp=0;
    while (listp<listc) {
      if (list[listp]==',') { listp++; continue; }
      const char *q=list+listp;
      int qc=0;
      while ((listp<listc)&&(list[listp++]!=',')) qc++;
      if ((qc==sfxc)&&!memcmp(q,sfx,sfxc)) return 1;
    }
    // If there's a dot in (sfx), advance to beyond that and try again.
    while (sfxc&&(sfx[0]!='.')) { sfx++; sfxc--; }
    if (!sfxc) break;
    sfx++;
    sfxc--;
  }
  return 0;
}

/* Look up a game and select the most appropriate launcher.
 */
 
struct db_launcher *db_launcher_for_gameid(const struct db *db,uint32_t gameid) {
  
  /* If there's a comment with key "launcher", and its value is the name of a launcher, use it.
   * This is an important fallback mechanism so the user can pick off individual games that need some nonstandard launcher.
   * eg launcher "never" for known faulty games.
   */
  struct db_launcher *launcher=0;
  uint32_t stringid_launcher=db_string_lookup(db,"launcher",8);
  if (stringid_launcher) {
    struct db_comment *comment=0;
    int commentc=db_comments_get_by_gameid(&comment,db,gameid);
    for (;commentc-->0;comment++) {
      if (comment->k!=stringid_launcher) continue;
      struct db_launcher *q=db->launchers.v;
      int qi=db->launchers.c;
      for (;qi-->0;q++) {
        if (q->name==comment->v) {
          launcher=q;
          break;
        }
      }
      // Even if we found one, keep looking. The latest (ie highest-index) comment naming an existing launcher should win ties.
    }
  }
  if (launcher) return launcher;
  
  struct db_game *game=db_game_get_by_id(db,gameid);
  if (!game) return 0;
  
  // Normalize suffix from basename.
  // Read from just after the first dot.
  // If there's more than one dot, our string matcher examines the shorter possibilities.
  char sfx[32];
  int sfxc=0,basep=0,sfxready=0;
  for (;(basep<sizeof(game->base))&&game->base[basep];basep++) {
    char ch=game->base[basep];
    if (!sfxready) {
      if (ch=='.') sfxready=1;
    } else if (sfxc>=sizeof(sfx)) { // shouldn't happen; base names are limited to 32 bytes too.
      sfx[0]=0;
    } else if ((ch>='A')&&(ch<='Z')) {
      sfx[sfxc++]=ch+0x20;
    } else {
      sfx[sfxc++]=ch;
    }
  }
  if (!sfx[0]) sfxc=0;
  
  // Check each launcher.
  //TODO Review this algorithm for weird fuzzy cases, I really don't know whether it's sensible.
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
        if (db_suffix_in_list(sfx,sfxc,suffixes,suffixesc)) score+=50;
        else score-=50;
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
