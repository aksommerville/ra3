/* ra_migrate_bits.c
 * Standalone pieces of migration logic.
 * The operation is pretty complicated, I'm splitting into two files just for clarity.
 * Every function here should be a pretty discrete operation.
 */

#include "ra_migrate_internal.h"

/* Introductory logging, and initialize any context fields with a nonzero default.
 */
 
int ra_migrate_introduce(struct ra_migrate_context *ctx) {
  fprintf(stderr,"********************** Romassist migration wizard ******************\n");
  fprintf(stderr,"Pulling from remote instance at '%s'.\n",ra.migrate);
  
  ctx->username=getenv("USER");
  
  ctx->gamecounts.beforec=db_game_count(ra.db);
  ctx->listcounts.beforec=db_list_count(ra.db);
  ctx->launchercounts.beforec=db_launcher_count(ra.db);
  ctx->upgradecounts.beforec=db_upgrade_count(ra.db);
  ctx->commentcounts.beforec=db_comment_count(ra.db);
  
  // sic "intern" not "lookup" for these, in case we're starting from an empty DB.
  ctx->str_native=db_string_intern(ra.db,"native",6);
  ctx->str_never=db_string_intern(ra.db,"never",5);
  ctx->str_git_make=db_string_intern(ra.db,"git+make",8);
  
  return 0;
}

/* Connect to remote.
 */
 
int ra_migrate_connect(struct ra_migrate_context *ctx) {
  if (ra_minhttp_connect(&ctx->minhttp,ra.migrate)<0) {
    fprintf(stderr,"%s: Failed to connect.\n",ra.migrate);
    return -2;
  }
  return 0;
}

/* Fetch all DB tables.
 */
 
int ra_migrate_fetch(struct ra_migrate_context *ctx) {
  int status;
  if ((status=ra_minhttp_request_sync(
    0,&ctx->games,&ctx->minhttp,
    "GET","/api/game?index=0&count=10&detail=blobs" //TODO count=1000000 when we're ready
  ))!=200) {
    fprintf(stderr,"%s: GET /api/game: status %d\n",ra.migrate,status);
    return -2;
  }
  if ((status=ra_minhttp_request_sync(
    0,&ctx->lists,&ctx->minhttp,
    "GET","/api/list?index=0&count=1000000&detail=id"
  ))!=200) {
    fprintf(stderr,"%s: GET /api/list: status %d\n",ra.migrate,status);
    return -2;
  }
  if ((status=ra_minhttp_request_sync(
    0,&ctx->launchers,&ctx->minhttp,
    "GET","/api/launcher?index=0&count=1000000"
  ))!=200) {
    fprintf(stderr,"%s: GET /api/launcher: status %d\n",ra.migrate,status);
    return -2;
  }
  if ((status=ra_minhttp_request_sync(
    0,&ctx->upgrades,&ctx->minhttp,
    "GET","/api/upgrade?index=0&count=1000000"
  ))!=200) {
    fprintf(stderr,"%s: GET /api/upgrade: status %d\n",ra.migrate,status);
    return -2;
  }
  fprintf(stderr,"Fetched all DB content. In bytes: games=%d lists=%d launchers=%d upgrades=%d\n",ctx->games.c,ctx->lists.c,ctx->launchers.c,ctx->upgrades.c);
  return 0;
}

/* Final validation.
 */
 
int ra_migrate_validate_final(struct ra_migrate_context *ctx) {
  return 0;
}

/* Final report: Success case.
 */
 
void ra_migrate_report_changes(struct ra_migrate_context *ctx) {
  fprintf(stderr,"****************** Migration complete *******************\n");
  //TODO Show stats.
}

/* Final report: DB failure case.
 */
 
void ra_migrate_report_changes_after_failed_commit(struct ra_migrate_context *ctx) {
  fprintf(stderr,"!!! ERROR !!! Failed to save database after migration !!!\n");
  //TODO List all blob and external changes.
}

/* Get record from a JSON array.
 */
 
struct ra_migrate_get_record {
  void *dbrecord;
  int (*decodefn)(void *dbrecord,struct db *db,int format,const void *src,int srcc);
  struct sr_decoder *text;
  const char *k;
  int kc;
  int id;
};

static int ra_migrate_get_record_1(const char *src,int srcc,void *userdata) {
  struct ra_migrate_get_record *ctx=userdata;
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_object_start(&decoder)<0) return -1;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    if ((kc==ctx->kc)&&!memcmp(k,ctx->k,kc)) {
      int id;
      if (sr_decode_json_int(&id,&decoder)<0) {
        if (sr_decode_json_skip(&decoder)<0) return -1;
      } else if (id==ctx->id) {
      
        if (ctx->text) {
          memset(ctx->text,0,sizeof(struct sr_decoder));
          ctx->text->v=src;
          ctx->text->c=srcc;
        }
        if (ctx->dbrecord) {
          int err=ctx->decodefn(ctx->dbrecord,ra.db,DB_FORMAT_json,src,srcc);
          if (err<0) return err;
        }
        return 1;
      }
    } else {
      if (sr_decode_json_skip(&decoder)<0) return -1;
    }
  }
  if (sr_decode_json_end(&decoder,0)<0) return -1;
  return 0;
}
 
int ra_migrate_get_record(
  void *dbrecord,
  struct sr_decoder *text,
  const char *src,int srcc,
  const char *k,int kc, // eg "gameid", "listid", ...must be an integer.
  int id,
  void *decodefn // (int db_XXX_decode(XXX *record,struct db *db,int format,const void *src,int srcc))
) {
  if (!dbrecord||!decodefn) dbrecord=decodefn=0;
  if (!k) return -1;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  struct ra_migrate_get_record ctx={
    .dbrecord=dbrecord,
    .decodefn=decodefn,
    .text=text,
    .k=k,
    .kc=kc,
    .id=id,
  };
  if (sr_for_each_of_json_array(src,srcc,ra_migrate_get_record_1,&ctx)>0) return 0;
  return -1;
}

int ra_migrate_get_game(struct db_game *game,struct sr_decoder *text,struct ra_migrate_context *ctx,int gameid) {
  return ra_migrate_get_record(game,text,ctx->games.v,ctx->games.c,"gameid",6,gameid,db_game_decode);
}

int ra_migrate_get_list(struct db_list *list,struct sr_decoder *text,struct ra_migrate_context *ctx,int listid) {
  return ra_migrate_get_record(list,text,ctx->lists.v,ctx->lists.c,"listid",6,listid,db_list_decode);
}

int ra_migrate_get_launcher(struct db_launcher *launcher,struct sr_decoder *text,struct ra_migrate_context *ctx,int launcherid) {
  return ra_migrate_get_record(launcher,text,ctx->launchers.v,ctx->launchers.c,"launcherid",10,launcherid,db_launcher_decode);
}

int ra_migrate_get_upgrade(struct db_upgrade *upgrade,struct sr_decoder *text,struct ra_migrate_context *ctx,int upgradeid) {
  return ra_migrate_get_record(upgrade,text,ctx->upgrades.v,ctx->upgrades.c,"upgradeid",9,upgradeid,db_upgrade_decode);
}

int ra_migrate_get_upgrade_by_gameid(struct db_upgrade *upgrade,struct sr_decoder *text,struct ra_migrate_context *ctx,int gameid) {
  return ra_migrate_get_record(upgrade,text,ctx->upgrades.v,ctx->upgrades.c,"gameid",6,gameid,db_upgrade_decode);
}

/* Decode JSON game.
 */
 
int ra_migrate_decode_game(
  struct db_game *game,
  struct sr_decoder *comments,
  struct sr_decoder *blobs,
  struct ra_migrate_context *ctx,
  const char *src,int srcc
) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  // Use the standard game decoder for most of it. Would be crazy for us to duplicate that.
  if (game&&(db_game_decode(game,ra.db,DB_FORMAT_json,src,srcc)<0)) {
    fprintf(stderr,"Failed to decode JSON game: %.*s\n",srcc,src);
    return -2;
  }
  // And if they want the other stuff, do another pass over the JSON.
  // Decoding the other stuff is perfectly uncontroversial, we just return JSON expressions.
  if (comments||blobs) {
    struct sr_decoder decoder={.v=src,.c=srcc};
    if (sr_decode_json_object_start(&decoder)<0) {
      fprintf(stderr,"Failed to decode JSON game: %.*s\n",srcc,src);
      return -2;
    }
    const char *k;
    int kc;
    while ((kc=sr_decode_json_next(&k,&decoder))>0) {
      const char *v;
      int vc=sr_decode_json_expression(&v,&decoder);
      if (vc<0)  {
        fprintf(stderr,"Failed to decode JSON game: %.*s\n",srcc,src);
        return -2;
      }
      if (comments&&(kc==8)&&!memcmp(k,"comments",8)) {
        memset(comments,0,sizeof(struct sr_decoder));
        comments->v=v;
        comments->c=vc;
      } else if (blobs&&(kc==5)&&!memcmp(k,"blobs",5)) {
        memset(blobs,0,sizeof(struct sr_decoder));
        blobs->v=v;
        blobs->c=c;
      }
    }
  }
  return 0;
}

/* Find an existing game for one incoming.
 */
 
struct db_game *ra_migrate_find_existing_game(const struct db_game *incoming) {
  struct db_game *existing=0;
  
  /* There's a good chance that the user is migrating highly similar databases, where games mostly have the same IDs.
   * That will be true for me, at least.
   * But don't just assume that an ID match is the same game.
   * Verify platform and basename too -- Those are very unlikely to change once a game is added.
   */
  if (existing=db_game_get_by_id(ra.db,incoming.gameid)) {
    if (
      (existing->platform==incoming->platform)&&
      !strncmp(existing->base,incoming->base,DB_GAME_BASE_LIMIT)
    ) {
      return existing;
    }
  }
  
  /* Where ID does not match, it's quite a bit hairier.
   * In a perfect world, we'd take a hash of the actual file and work only from that.
   * That is not practical for us. (and actually not correct either: Consider native games built specific for each host).
   * (platform) must always match -- it should be known when the record was first added, and should never change.
   * Let's say (platform,base)?
   * Basename collisions should be rare, and in collision cases it's fair to assume that the two are on different platforms.
   * (think "donkey_kong.bin", is it atari2600 or atari5200?)
   * And though there's no guarantee that basename will hold constant, it's not something we typically change.
   */
  if (existing=db_game_get_by_index(ra.db,0)) {
    int i=db_game_count(ra.db);
    for (;i-->0;existing++) {
      if (existing->platform!=incoming->platform) continue;
      if (strncmp(existing->base,incoming->base,DB_GAME_BASE_LIMIT)) continue;
      return existing;
    }
  }
  
  return 0;
}

/* Should update game?
 */
 
int ra_migrate_should_update_game(
  struct db_game *update,
  struct ra_migrate_context *ctx,
  const struct db_game *existing,
  const struct db_game *incoming
) {
  int answer=0;
  memcpy(update,existing,sizeof(struct db_game));
  #define SCALAR(k) if (!existing->k&&incoming->k) { update->k=incoming->k; answer=1; }
  SCALAR(platform)
  SCALAR(author)
  SCALAR(genre)
  SCALAR(flags)
  SCALAR(rating)
  SCALAR(pubtime)
  #undef SCALAR
  // Don't replace (gameid,dir,base), those are private to the local db.
  // (name) unset is vanishingly unlikely so don't bother checking.
  // If we were less conservative and take in all changes from remote, then we'd check (name) too.
  return answer;
}

/* Should insert game?
 */
 
int ra_migrate_should_insert_game(
  struct db_game *update,
  struct ra_migrate_context *ctx,
  const struct db_game *incoming
) {

  // First a sanity check: If any of (platform,dir,base) unset, assume it's faulty and reject.
  if (!incoming->platform) return 0;
  if (!incoming->dir) return 0;
  if (!incoming->base[0]) return 0;
  
  // Also, "platform=never" and the "faulty" flag -- don't bother.
  // I guess it's not really our job to act as a quality filter, but those cases are obvious.
  if (incoming->platform==ctx->str_never) return 0;
  if (incoming->flags&DB_FLAG_faulty) return 0;
  
  /* If the platform is "native", we shouldn't try to copy it over HTTP.
   * Proceed with these only if there is a "git+make" upgrade record for the game.
   */
  if (incoming->platform==ctx->str_native) {
    struct db_upgrade upgrade={0};
    if (ra_migrate_get_upgrade_by_gameid(&upgrade,0,ctx,incoming->gameid)<0) return 0;
    if (upgrade.method!=ctx->str_git_make) return 0;
  }
  
  /* OK let's insert it!
   * Provisionally start with the exact incoming record.
   */
  memcpy(update,incoming,sizeof(struct db_game));
  
  /* Be careful with (gameid): Keep from (incoming) if it's not in use, why not, but zero if it's already in use.
   * We know that (incoming->gameid) is not *this* game, that would have been detected earlier and our caller wouldn't be trying to insert.
   * db does accept nonzero gameid at insert, but will fail if it's in use.
   * I figure if we have identical game collections on different hosts, it might be helpful to keep IDs in order too.
   */
  if (db_game_get_by_id(ra.db,update->gameid)) {
    update->gameid=0;
  }
   
  /* It's unlikely that the remote path is correct for us.
   * But kind of hard to tell what it should be.
   * Best answer I can think of is to force a particular format for local paths.
   * But let's check some things first:
   *  - If the incoming game's directory exists locally, keep it.
   *  - Or if it exists after replacing "/home/NNN/" with our username.
   * (incoming->base) we will keep verbatim no matter what else.
   */
  const char *rdir=0;
  int rdirc=db_string_get(&rdir,ra.db,incoming->dir);
  if (rdirc<1) return 0;
  char path[1024];
  if (rdirc>=sizeof(path)) return 0; // d'oh
  memcpy(path,rdir,rdirc);
  path[rdirc]=0;
  if (file_get_type(path)=='d') {
    // Hey awesome, our filesystems match.
    return 1;
  }
  if (ra_migrate_replace_home_ip(path,sizeof(path),ctx)>0) {
    if (file_get_type(path)=='d') {
      // OK, we have a directory like that, just a different username.
      if (!(update->dir=db_string_intern(ra.db,path,-1))) return 0;
      return 1;
    }
    // We were able to replace HOME but the new directory doesn't exist....
    // It could be anything, really. Probably better to work from scratch.
  }
  
  // Make up a local path for it. This is highly opinionated! Would be nice of us to allow some configuration. (TODO)
  if (!ctx->username||!ctx->username[0]) return 0;
  const char *platform=0;
  int platformc=db_string_get(&platform,ra.db,update->platform);
  if (platformc<1) return 0;
  
  // In my collection, usually, there's a directory under platform, "09" or the lowercase first letter of the basename.
  char prefix[2];
  int prefixc=0;
       if ((update->base[0]>='a')&&(update->base[0]<='z')) { prefix[0]=update->base[0]; prefixc=1; }
  else if ((update->base[0]>='A')&&(update->base[0]<='Z')) { prefix[0]=update->base[0]; prefixc=1; }
  else { prefix[0]='0'; prefix[1]='9'; prefixc=2; }
  
  int pathc=snprintf(path,sizeof(path),"/home/%s/rom/%.*s/%.*s",ctx->username,platformc,platform,prefixc,prefix);
  if ((pathc<1)||(pathc>=sizeof(path))) return 0;
  if (!(update->dir=db_string_intern(ra.db,path,pathc))) return 0;

  return 0;
}

/* Replace home directory in place.
 */
 
int ra_migrate_replace_home_ip(char *dst,int dsta,struct ra_migrate_context *ctx) {
  if (!ctx->username) return 0;
  if (!ctx->username[0]) return 0;
  if (dsta<6) return 0;
  if (memcmp(dst,"/home/",6)) return 0;
  int dstc=0; while (dst[dstc]) dstc++;
  const char *ruser=dst+6;
  int ruserc=0;
  while ((6+ruserc<dstc)&&(ruser[ruserc]!='/')) ruserc++;
  int luserc=0;
  while (ctx->username[luserc]) luserc++;
  if ((luserc==ruserc)&&!memcmp(ruser,ctx->username,ruserc)) return 0; // remote and local users same name
  int addc=luserc-ruserc;
  if (dstc+addc>=dsta) return 0; // doesn't fit
  memmove(dst+6+luserc,dst+6+ruserc,dstc-ruserc-6+1);
  memcpy(dst+6,ctx->username,luserc);
  return dstc+addc;
}

/* Acquire the game file for a record we're about to insert.
 */
 
int ra_migrate_copy_rom_file_if_needed(
  struct ra_migrate_context *ctx,
  const struct db_game *lgame,
  const struct db_game *rgame
) {

  // platform=native is an automatic noop success, whether the file exists or not.
  if (lgame->platform==ctx->str_native) return 0;

  char lpath[1024];
  int lpathc=db_game_get_path(lpath,sizeof(lpath),ra.db,lgame);
  if ((lpathc<1)||(lpathc>=sizeof(lpath))) return -1;
  if (file_get_type(lpath)=='f') return 0; // ok easy
  
  // Fetch from the remote.
  fprintf(stderr,"%s:%d: TODO: Add rom-download endpoint and resume here.\n",__FILE__,__LINE__);
  return -2;
  
  return 0;
}
