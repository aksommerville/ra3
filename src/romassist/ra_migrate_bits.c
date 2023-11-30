/* ra_migrate_bits.c
 * Standalone pieces of migration logic.
 * The operation is pretty complicated, I'm splitting into two files just for clarity.
 * Every function here should be a pretty discrete operation.
 */

#include "ra_migrate_internal.h"

/* ID change table.
 */
 
static int ra_migrate_idchangev_search(const struct ra_migrate_context *ctx,char type,uint32_t rid) {
  int lo=0,hi=ctx->idchangec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct ra_migrate_idchange *q=ctx->idchangev+ck;
         if (type<q->type) hi=ck;
    else if (type>q->type) lo=ck+1;
    else if (rid<q->rid) hi=ck;
    else if (rid>q->rid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

int ra_migrate_idchange_add(struct ra_migrate_context *ctx,char type,uint32_t lid,uint32_t rid) {
  int p=ra_migrate_idchangev_search(ctx,type,rid);
  if (p>=0) {
    if (ctx->idchangev[p].lid==lid) return 0;
    return -1;
  }
  p=-p-1;
  if (ctx->idchangec>=ctx->idchangea) {
    int na=ctx->idchangea+256;
    if (na>INT_MAX/sizeof(struct ra_migrate_idchange)) return -1;
    void *nv=realloc(ctx->idchangev,sizeof(struct ra_migrate_idchange)*na);
    if (!nv) return -1;
    ctx->idchangev=nv;
    ctx->idchangea=na;
  }
  struct ra_migrate_idchange *idchange=ctx->idchangev+p;
  memmove(idchange+1,idchange,sizeof(struct ra_migrate_idchange)*(ctx->idchangec-p));
  ctx->idchangec++;
  idchange->type=type;
  idchange->rid=rid;
  idchange->lid=lid;
  return 0;
}

uint32_t ra_migrate_local_from_remote_id(const struct ra_migrate_context *ctx,char type,uint32_t rid) {
  int p=ra_migrate_idchangev_search(ctx,type,rid);
  if (p<0) return rid;
  return ctx->idchangev[p].lid;
}

uint32_t ra_migrate_remote_from_local_id(const struct ra_migrate_context *ctx,char type,uint32_t lid) {
  int p=ra_migrate_idchangev_search(ctx,type,0);
  if (p<0) p=-p-1;
  const struct ra_migrate_idchange *idchange=ctx->idchangev+p;
  for (;p<ctx->idchangec;p++,idchange++) {
    if (idchange->type!=type) break;
    if (idchange->lid==lid) return idchange->rid;
  }
  return lid;
}

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
  ctx->blobcounts.beforec=0; // not sure we can get an answer for this without a lot of work. it's not important
  
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
    "GET","/api/game?index=0&count=1000000&detail=blobs"
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
  
  if (ctx->gamecounts.addc) {
    fprintf(stderr,"Added %d games, from the %d present on remote.\n",ctx->gamecounts.addc,ctx->gamecounts.incomingc);
  } else {
    fprintf(stderr,"No games added, from %d candidates.\n",ctx->gamecounts.incomingc);
  }
  if (ctx->gamecounts.modc) {
    fprintf(stderr,"Modified %d existing games.\n",ctx->gamecounts.modc);
  }
  if (ctx->gamecounts.ignorec) {
    fprintf(stderr,"%d games were ignored; usually this means they are native games with no upgrade method.\n",ctx->gamecounts.ignorec);
  }
  
  int minoraddc=ctx->listcounts.addc+ctx->launchercounts.addc+ctx->upgradecounts.addc+ctx->commentcounts.addc;
  int minormodc=ctx->listcounts.modc+ctx->launchercounts.modc+ctx->upgradecounts.modc+ctx->commentcounts.modc;
  if (minoraddc||minormodc) {
    fprintf(stderr,"Added %d minor records (list, launcher, upgrade, comment), and modified %d.\n",minoraddc,minormodc);
  } else {
    fprintf(stderr,"No change to minor records (list, launcher, upgrade, comment).\n");
  }
  
  if (ctx->blobcounts.addc) {
    fprintf(stderr,"Added %d blobs (typically screencaps)\n",ctx->blobcounts.addc);
  }
  
  if (ctx->romdownloadc) {
    fprintf(stderr,"Downloaded %d new ROM files.\n",ctx->romdownloadc);
  }
  
  fprintf(stderr,"Total incoming TCP data: %d bytes\n",ctx->minhttp.rcvtotal);
}

/* Final report: DB failure case.
 */
 
void ra_migrate_report_changes_after_failed_commit(struct ra_migrate_context *ctx) {
  fprintf(stderr,"!!! ERROR !!! Failed to save database after migration !!!\n");
  int ok=1;
  if (ctx->blobcounts.addc) {
    fprintf(stderr,"%d blobs were added but might now be invalid.\n",ctx->blobcounts.addc);
    ok=0;
  }
  if (ctx->romdownloadc) {
    fprintf(stderr,"%d rom files were downloaded and are now orphaned.\n",ctx->romdownloadc);
    ok=0;
  }
  if (ok) fprintf(stderr,"No unreversible actions appear to have been taken; you're in the same state as before the migration attempt.\n");
  else fprintf(stderr,"The bulk of the database should still be in its pre-migration state.\n");
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
        blobs->c=vc;
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
  if (existing=db_game_get_by_id(ra.db,incoming->gameid)) {
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

  return 1;
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
  fprintf(stderr,"%s: Downloading ROM for '%.32s'...\n",lpath,lgame->name);
  struct sr_encoder rsp={0};
  char req[64];
  int reqc=snprintf(req,sizeof(req),"/api/game/file?gameid=%d",rgame->gameid);
  if ((reqc<1)||(reqc>=sizeof(req))) return -1;
  int err=ra_minhttp_request_sync(0,&rsp,&ctx->minhttp,"GET",req);
  if (err!=200) {
    fprintf(stderr,"...download failed! Status %d\n",err);
    sr_encoder_cleanup(&rsp);
    return -2;
  }
  if ((dir_mkdirp_parent(lpath)<0)||(file_write(lpath,rsp.v,rsp.c)<0)) {
    fprintf(stderr,"%s: Failed to write file for game '%.32s', %d bytes\n",lpath,lgame->name,rsp.c);
    sr_encoder_cleanup(&rsp);
    return -2;
  }
  sr_encoder_cleanup(&rsp);
  
  return 1;
}

/* For a blob path from the remote, determine a suitable local path.
 */
 
int ra_migrate_local_path_for_blob(char *dst,int dsta,struct ra_migrate_context *ctx,int gameid,const char *rpath,int rpathc) {
  /* Blob paths are "DBROOT/blob/CENTURY/GAMEID-...".
   * DBROOT, CENTURY, and GAMEID are all subject to change here, and we don't need to read them from (rpath).
   * So all we need from (rpath) is the stuff after GAMEID.
   */
  int dashp=-1;
  int i=rpathc;
  while (i-->0) {
    if (rpath[i]=='/') break;
    if (rpath[i]=='-') dashp=i;
  }
  if (dashp<0) return -1; // (rpath) does not contain a dash in its basename. That's actually a hard error, db requires it.
  const char *sfx=rpath+dashp; // includes the dash
  int sfxc=rpathc-dashp;
  const char *dbroot=db_get_root(ra.db);
  if (!dbroot) return -1;
  int century=(gameid/100)*100;
  int dstc=snprintf(dst,dsta,"%s/blob/%d/%d%.*s",dbroot,century,gameid,sfxc,sfx);
  if ((dstc<1)||(dstc>=dsta)) return -1;
  if (db_blob_validate_path(ra.db,dst,dstc)<0) return -1; // just to be on the safe side, but this should always be ok
  return dstc;
}

/* Examine an incoming launcher.
 */
 
struct db_launcher *ra_migrate_consider_launcher(
  struct db_launcher *update,
  struct ra_migrate_context *ctx,
  struct db_launcher *incoming
) {

  /* Consider launchers the same if their name matches.
   * Search on ID first for efficiency only.
   */
  struct db_launcher *existing=db_launcher_get_by_id(ra.db,incoming->launcherid);
  if (existing&&(existing->name==incoming->name)) {
    // cool
  } else {
    existing=0;
    struct db_launcher *q=db_launcher_get_by_index(ra.db,0);
    int i=db_launcher_count(ra.db);
    for (;i-->0;q++) {
      if (q->name==incoming->name) {
        existing=q;
        break;
      }
    }
  }
  
  /* Don't have it yet? Add verbatim, only tweak ID if necessary.
   * (cmd) will probably need changed, but I'm leaving that to the user.
   */
  if (!existing) {
    memcpy(update,incoming,sizeof(struct db_launcher));
    if (db_launcher_get_by_id(ra.db,incoming->launcherid)) update->launcherid=0;
    return 0;
  }
  
  /* Take incoming changes only if we were zero before.
   */
  if (
    (!existing->name&&incoming->name)||
    (!existing->platform&&incoming->platform)||
    (!existing->suffixes&&incoming->suffixes)||
    (!existing->cmd&&incoming->cmd)||
    (!existing->desc&&incoming->desc)
  ) {
    memcpy(update,incoming,sizeof(struct db_launcher));
    update->launcherid=existing->launcherid;
  }
  return existing;
}

/* Nonzero if (to) has anything that (from) doesn't.
 */
 
static int ra_migrate_list_has_additions(const struct db_list *from,const struct db_list *to) {
  int i=0; for (;i<to->gameidc;i++) {
    if (!db_list_has(from,to->gameidv[i])) return 1;
  }
  return 0;
}

/* Examine an incoming list.
 */
 
struct db_list *ra_migrate_consider_list(
  struct db_list *update,
  struct ra_migrate_context *ctx,
  struct db_list *incoming
) {
  
  /* Lists match if their names match exactly.
   * Search by ID first only as an optimization.
   */
  struct db_list *existing=db_list_get_by_id(ra.db,incoming->listid);
  if (existing&&(existing->name==incoming->name)) {
    // cool
  } else {
    int i=0; for (;;i++) {
      if (!(existing=db_list_get_by_index(ra.db,i))) break;
      if (existing->name==incoming->name) break;
    }
  }
  
  /* If we don't have it already, add it verbatim.
   * Lists are live objects, so we can't just memcpy it.
   */
  if (!existing) {
    if (db_list_get_by_id(ra.db,incoming->listid)) update->listid=0;
    else update->listid=incoming->listid;
    update->name=incoming->name;
    update->desc=incoming->desc;
    update->sorted=incoming->sorted;
    int i=0; for (;i<incoming->gameidc;i++) {
      db_list_append(ra.db,update,incoming->gameidv[i]);
    }
    return 0;
  }
  
  /* Call it dirty if the header fields changed or any games were added.
   * Don't take in removals or reordering.
   */
  if (
    (existing->name==incoming->name)&&
    (existing->desc==incoming->desc)&&
    (existing->sorted==incoming->sorted)&&
    !ra_migrate_list_has_additions(existing,incoming)
  ) return existing; // clean
  
  /* Dirty. Populate (update).
   * I'm going to cheat a little and just append everything.
   * This takes care of checking for duplicates, and satisfies our "add-only" policy, but it also can foul up the order.
   * Since we're fouling it anyway, set update->sorted=1 so it can at least happen efficiently.
   */
  update->listid=existing->listid;
  update->name=incoming->name;
  update->desc=incoming->desc;
  update->sorted=1;
  int i;
  for (i=0;i<existing->gameidc;i++) db_list_append(ra.db,update,existing->gameidv[i]);
  for (i=0;i<incoming->gameidc;i++) db_list_append(ra.db,update,incoming->gameidv[i]);
  
  return existing;
}

/* Examine an incoming upgrade.
 */
 
struct db_upgrade *ra_migrate_consider_upgrade(
  struct db_upgrade *update,
  struct ra_migrate_context *ctx,
  struct db_upgrade *incoming
) {
  
  /* Translate the game and launcher IDs.
   * Games and launchers are both processed before upgrades, so they should be translatable.
   * If either comes up zero when the input was nonzero, or has a value but there's no record for it, abort.
   */
  uint32_t rgameid=incoming->gameid?ra_migrate_local_from_remote_id(ctx,'u',incoming->gameid):0;
  uint32_t rlauncherid=incoming->launcherid?ra_migrate_local_from_remote_id(ctx,'u',incoming->launcherid):0;
  if (incoming->gameid&&!db_game_get_by_id(ra.db,rgameid)) return 0;
  if (incoming->launcherid&&!db_launcher_get_by_id(ra.db,rlauncherid)) return 0;
  
  /* Do we already have this one?
   * We can consider upgrades equivalent if they refer to the same game or launcher.
   */
  struct db_upgrade *existing=db_upgrade_get_by_id(ra.db,incoming->upgradeid);
  if (existing) {
    if (
      (existing->gameid!=rgameid)||
      (existing->launcherid!=rlauncherid)
    ) {
      // ooh not the same
      existing=0;
    }
  }
  if (!existing) {
    struct db_upgrade *q=db_upgrade_get_by_index(ra.db,0);
    int i=db_upgrade_count(ra.db);
    for (;i-->0;q++) {
      if (
        (q->gameid==rgameid)&&
        (q->launcherid==rlauncherid)
      ) {
        // aha but this one is the same
        existing=q;
        break;
      }
    }
  }
  
  /* If we have one, call it changed if anything absent in ours is present in theirs.
   * A difference between two nonzero values, we have to skip it.
   */
  if (existing) {
    if (
      (!existing->name&&incoming->name)||
      (!existing->desc&&incoming->desc)||
      (!existing->method&&incoming->method)||
      (!existing->param&&incoming->param)
      // Not considered: upgradeid,gameid,launcherid,depend,checktime,buildtime,status
    ) {
      memcpy(update,existing,sizeof(struct db_upgrade));
      update->name=incoming->name;
      update->desc=incoming->desc;
      update->method=incoming->method;
      update->param=incoming->param;
    }
    return existing;
  }
  
  /* We don't have one. Do we want to add it? Yes.
   */
  memcpy(update,incoming,sizeof(struct db_upgrade));
  if (db_upgrade_get_by_id(ra.db,incoming->upgradeid)) update->upgradeid=0;
  update->gameid=rgameid;
  update->launcherid=rlauncherid;
  if (incoming->depend) { // Might have to zero (depend), if it's a record we haven't processed yet.
    uint32_t dp=ra_migrate_local_from_remote_id(ctx,'u',incoming->depend);
    if (!dp) {
      update->depend=0;
    } else if (!db_upgrade_get_by_id(ra.db,dp)) {
      update->depend=0;
    }
  }
  // (checktime,buildtime,status): Relevant only to each host, don't copy.
  update->checktime=0;
  update->buildtime=0;
  update->status=0;
  
  return 0;
}
