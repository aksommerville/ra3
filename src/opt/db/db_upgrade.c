#include "db_internal.h"
#include "opt/serial/serial.h"

/* Direct store access.
 */

int db_upgrade_count(const struct db *db) {
  return db->upgrades.c;
}

struct db_upgrade *db_upgrade_get_by_index(const struct db *db,int p) {
  return db_flatstore_get(&db->upgrades,p);
}

struct db_upgrade *db_upgrade_get_by_id(const struct db *db,uint32_t upgradeid) {
  return db_flatstore_get(&db->upgrades,db_flatstore_search1(&db->upgrades,upgradeid));
}

/* Get by name or id.
 */

struct db_upgrade *db_upgrade_get_by_string(const struct db *db,const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  /* Check strings (upgrade->name) and (launcher->name), and inline text (game->name).
   * If any (upgrade->name) matches, it takes precedence over games and launchers.
   */
  uint32_t stringid=db_string_lookup(db,src,srcc);
  struct db_upgrade *fallback=0;
  struct db_upgrade *upgrade=db->upgrades.v;
  int i=db->upgrades.c;
  for (;i-->0;upgrade++) {
    if (stringid) {
      if (upgrade->name==stringid) return upgrade;
      if (upgrade->launcherid) {
        const struct db_launcher *launcher=db_launcher_get_by_id(db,upgrade->launcherid);
        if (launcher&&(launcher->name==stringid)) fallback=upgrade;
      }
    }
    if (upgrade->gameid&&(srcc<=DB_GAME_NAME_LIMIT)) {
      const struct db_game *game=db_game_get_by_id(db,upgrade->gameid);
      if (game&&!memcmp(game->name,src,srcc)) {
        if ((srcc<DB_GAME_NAME_LIMIT)&&game->name[srcc]) {
        } else fallback=upgrade;
      }
    }
  }
  if (fallback) return fallback;
  
  // No text match? Try as an integer ID.
  int upgradeid=0;
  if (sr_int_eval(&upgradeid,src,srcc)>=2) {
    struct db_upgrade *upgrade=db_upgrade_get_by_id(db,upgradeid);
    if (upgrade) return upgrade;
  }
  
  // Better luck next time!
  return 0;
}

/* Get by game or launcher.
 */
    
struct db_upgrade *db_upgrade_get_by_gameid(const struct db *db,uint32_t gameid) {
  if (!gameid) return 0;
  struct db_upgrade *upgrade=db->upgrades.v;
  int i=db->upgrades.c;
  for (;i-->0;upgrade++) if (upgrade->gameid==gameid) return upgrade;
  return 0;
}

struct db_upgrade *db_upgrade_get_by_launcherid(const struct db *db,uint32_t launcherid) {
  if (!launcherid) return 0;
  struct db_upgrade *upgrade=db->upgrades.v;
  int i=db->upgrades.c;
  for (;i-->0;upgrade++) if (upgrade->launcherid==launcherid) return upgrade;
  return 0;
}

/* Delete.
 */

int db_upgrade_delete(struct db *db,uint32_t upgradeid) {
  int p=db_flatstore_search1(&db->upgrades,upgradeid);
  if (p<0) return -1;
  db_flatstore_remove(&db->upgrades,p,1);
  db->dirty=1;
  
  // Gentle cascade: If any upgrade depends on this one, zero out the dependency. Don't delete the other upgrade.
  struct db_upgrade *upgrade=db->upgrades.v;
  int i=db->upgrades.c;
  for (;i-->0;upgrade++) {
    if (upgrade->depend==upgradeid) {
      upgrade->depend=0;
    }
  }
  
  return 0;
}

void db_upgrade_delete_for_gameid(struct db *db,uint32_t gameid) {
  int p=db->upgrades.c;
  while (p-->0) {
    struct db_upgrade *upgrade=db_flatstore_get(&db->upgrades,p);
    if (upgrade->gameid==gameid) {
      db_upgrade_delete(db,upgrade->upgradeid);
      // upgrade->gameid is supposed to be unique so we could stop here. But for safety's sake, keep going.
    }
  }
}

void db_upgrade_delete_for_launcherid(struct db *db,uint32_t launcherid) {
  int p=db->upgrades.c;
  while (p-->0) {
    struct db_upgrade *upgrade=db_flatstore_get(&db->upgrades,p);
    if (upgrade->launcherid==launcherid) {
      db_upgrade_delete(db,upgrade->upgradeid);
      // upgrade->launcherid is supposed to be unique so we could stop here. But for safety's sake, keep going.
    }
  }
}

/* Insert.
 */

struct db_upgrade *db_upgrade_insert(struct db *db,const struct db_upgrade *upgrade) {
  uint32_t upgradeid=upgrade->upgradeid;
  if (!upgradeid) upgradeid=db_flatstore_next_id(&db->upgrades);
  int p=db_flatstore_search1(&db->upgrades,upgradeid);
  if (p>=0) return 0;
  p=-p-1;
  struct db_upgrade *real=db_flatstore_insert1(&db->upgrades,p,upgradeid);
  if (!real) return 0;
  memcpy(real,upgrade,sizeof(struct db_upgrade));
  real->upgradeid=upgradeid;
  db->dirty=1;
  return real;
}

/* Update.
 */

struct db_upgrade *db_upgrade_update(struct db *db,const struct db_upgrade *upgrade) {
  int p=db_flatstore_search1(&db->upgrades,upgrade->upgradeid);
  if (p<0) return 0;
  struct db_upgrade *real=db_flatstore_get(&db->upgrades,p);
  memcpy(real,upgrade,sizeof(struct db_upgrade));
  db->dirty=db->upgrades.dirty=1;
  return real;
}

/* Notify of status.
 */
 
int db_upgrade_update_for_build(struct db *db,uint32_t upgradeid,const char *status,int statusc) {
  if (!status) statusc=0; else if (statusc<0) { statusc=0; while (status[statusc]) statusc++; }
  int p=db_flatstore_search1(&db->upgrades,upgradeid);
  if (p<0) return -1;
  struct db_upgrade *upgrade=db_flatstore_get(&db->upgrades,p);
  upgrade->checktime=db_time_now();
  if (!statusc) {
    upgrade->status=0;
    upgrade->buildtime=upgrade->checktime;
  } else if ((statusc==4)&&!memcmp(status,"noop",4)) {
    upgrade->status=0;
  } else {
    upgrade->status=db_string_intern(db,status,statusc);
  }
  db->dirty=db->upgrades.dirty=1;
  return 0;
}

/* Dirty.
 */

void db_upgrade_dirty(struct db *db,struct db_upgrade *upgrade) {
  db->upgrades.dirty=1;
  db->dirty=1;
}
