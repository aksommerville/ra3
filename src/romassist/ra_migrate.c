#include "ra_migrate_internal.h"

/* Cleanup.
 */

static void ra_migrate_context_cleanup(struct ra_migrate_context *ctx) {
  ra_minhttp_cleanup(&ctx->minhttp);
  sr_encoder_cleanup(&ctx->games);
  sr_encoder_cleanup(&ctx->lists);
  sr_encoder_cleanup(&ctx->launchers);
  sr_encoder_cleanup(&ctx->upgrades);
}

#if 0 //XXX move what we can to ra_migrate_bits.c

/* Extract one string field from a JSON object.
 */
 
static int ra_extract_json_string(char *dst,int dsta,const char *src,int srcc,const char *k,int kc) {
  if (!src) return 0;
  if (!k) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_object_start(&decoder)<0) return 0;
  const char *qk;
  int qkc;
  while ((qkc=sr_decode_json_next(&qk,&decoder))>0) {
    if ((qkc==kc)&&!memcmp(qk,k,kc)) {
      return sr_decode_json_string(dst,dsta,&decoder);
    } else {
      if (sr_decode_json_skip(&decoder)<0) return 0;
    }
  }
  return 0;
}

/* Check whether we have this file, download it if possible.
 * Return local path on success, zero if we decline (eg native game, can't assume it's just one file).
 * Input is the encoded game from the remote.
 */
 
static int ra_migrate_game_file(char *path,int patha,struct ra_migrate_context *ctx,const char *src,int srcc) {
  
  // If the platform is "native", don't bother.
  char platform[16];
  int platformc=ra_extract_json_string(platform,sizeof(platform),src,srcc,"platform",8);
  if ((platformc==6)&&!memcmp(platform,"native",6)) return 0;
  
  char rpath[1024];
  int rpathc=ra_extract_json_string(rpath,sizeof(rpath),src,srcc,"path",4);
  if ((rpathc<1)||(rpathc>=sizeof(rpath))) return 0;
  
  /* It's entirely possible that local and remote paths are the same, and we already have the file.
   * eg I have a bunch of machines around here with the same collection at /home/andy/rom/
   * That's obvious and easy to handle, so check it first.
   */
  if (file_get_type(rpath)=='f') {
    if (rpathc<=patha) memcpy(path,rpath,rpathc);
    if (rpathc<patha) path[rpathc]=0;
    return rpathc;
  }
  
  /* Synthesize a local path.
   * TODO This is extremely opinionated, not at all appropriate for general distribution.
   * We'll expect to put files at "/home/USER/rom/PLATFORM/PREFIX/BASE" where:
   *  - USER from environment.
   *  - PLATFORM from incoming JSON.
   *  - PREFIX is "09" or the lowercase first letter of BASE.
   *  - BASE from incoming JSON.
   * That's how mine are organized at least.
   */
  if ((platformc<1)||(platformc>sizeof(platform))) return 0;
  if (!ctx->username||!ctx->username[0]) return 0;
  const char *base=rpath+rpathc;
  int basec=0;
  while ((basec<rpathc)&&(base[-1]!='/')) { base--; basec++; }
  if (!basec) return 0;
  char prefix[2];
  int prefixc;
       if ((base[0]>='a')&&(base[0]<='z')) { prefix[0]=base[0]; prefixc=1; }
  else if ((base[0]>='A')&&(base[0]<='Z')) { prefix[0]=base[0]+0x20; prefixc=1; }
  else { prefix[0]='0'; prefix[1]='9'; prefixc=2; }
  char lpath[1024];
  int lpathc=snprintf(lpath,sizeof(lpath),"/home/%s/rom/%.*s/%.*s/%.*s",ctx->username,platformc,platform,prefixc,prefix,basec,base);
  if ((lpathc<1)||(lpathc>=sizeof(lpath))) return 0;
  
  /* One more thing: If a file happens to already exist at that path, assume it's the right one.
   */
  if (file_get_type(lpath)=='f') {
    if (lpathc<=patha) memcpy(path,lpath,lpathc);
    if (lpathc<patha) path[lpathc]=0;
    return lpathc;
  }
  
  fprintf(stderr,">>> TODO download '%s' and save at '%s'\n",rpath,lpath);
  
  return 0;
}

/* Fallback if ra_migrate_game_file returns zero:
 * If there's an upgrade of method "git+make" for this game, assume that we will be able to install it.
 * If so, compose the expected local path for it.
 */
 
static int ra_migrate_gitmake_upgrade_exists_for_game(char *path,int patha,struct ra_migrate_context *ctx,const struct db_game *game) {
  return 0;
}

/* Look for comments in this JSON game, and add to our DB if appropriate.
 * Beware that the gameid in (src) is not necessarily ours -- use the parameter (gameid).
 */
 
static int ra_migrate_game_comments_inner(struct ra_migrate_context *ctx,uint32_t gameid,struct sr_decoder *decoder) {
  struct db_comment *existing=0;
  int existingc=db_comments_get_by_gameid(&existing,ra.db,gameid);
  if (sr_decode_json_array_start(decoder)<0) return 0;
  while (sr_decode_json_next(0,decoder)>0) {
    int jsonctx=sr_decode_json_object_start(decoder);
    if (jsonctx<0) return -1;
    struct db_comment incoming={0};
    const char *k;
    int kc;
    while ((kc=sr_decode_json_next(&k,decoder))>0) {
      if ((kc==1)&&(k[0]=='k')) {
        if (db_decode_json_string(&incoming.k,ra.db,decoder)<0) return -1;
      } else if ((kc==1)&&(k[0]=='v')) {
        if (db_decode_json_string(&incoming.k,ra.db,decoder)<0) return -1;
      } else if ((kc==4)&&!memcmp(k,"time",4)) {
        char tmp[32];
        int tmpc=sr_decode_json_string(tmp,sizeof(tmp),decoder);
        if ((tmpc<0)||(tmpc>sizeof(tmp))) {
          if (sr_decode_json_skip(decoder)<0) return -1;
        } else {
          incoming.time=db_time_eval(tmp,tmpc);
        }
      } else {
        if (sr_decode_json_skip(decoder)<0) return -1;
      }
    }
    if (sr_decode_json_end(decoder,jsonctx)<0) return -1;
    ctx->commentcounts.incomingc++;
    
    // If any of the incoming fields is blank, skip it.
    if (!incoming.k||!incoming.v||!incoming.time) {
      ctx->commentcounts.ignorec++;
      continue;
    }
    
    // If we have an exact match for all three fields, skip it.
    int already=0;
    const struct db_comment *q=existing;
    int i=existingc;
    for (;i-->0;q++) {
      if ((q->k==incoming.k)&&(q->v==incoming.v)&&(q->time==incoming.time)) {
        already=1;
        break;
      }
    }
    if (already) {
      ctx->commentcounts.skipc++;
      continue;
    }
    
    // OK add it.
    incoming.gameid=gameid;
    if (!db_comment_insert(ra.db,&incoming)) return -1;
    ctx->commentcounts.addc++;
  }
  return 0;
}
 
static int ra_migrate_game_comments(struct ra_migrate_context *ctx,uint32_t gameid,const char *src,int srcc) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_object_start(&decoder)<0) return -1;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    if ((kc==8)&&!memcmp(k,"comments",8)) {
      return ra_migrate_game_comments_inner(ctx,gameid,&decoder);
    } else {
      if (sr_decode_json_skip(&decoder)<0) return -1;
    }
  }
  return 0;
}

/* Migrate games.
 */
 
static int ra_migrate_game(struct ra_migrate_context *ctx,const char *src,int srcc) {
  ctx->gamecounts.incomingc++;
  struct db_game game={0};
  if (db_game_decode(&game,ra.db,DB_FORMAT_json,src,srcc)<0) return -1;
  
  /* Determine whether we have this game already.
   * That's a subtle question.
   * If (gameid,base,platform) match, they're the same.
   * To match a different gameid, require exact match of (base,name,pubtime). Unless one pubtime is zero.
   */
  struct db_game *existing=db_game_get_by_id(ra.db,game.gameid);
  if (existing) {
    if (existing->platform!=game.platform) {
      existing=0; // different platforms, definitely not the same game.
    } else if (strncmp(game.base,existing->base,DB_GAME_BASE_LIMIT)) {
      existing=0; // different base names, nope. those shouldn't change much.
    } else {
      // ok they're the same
    }
  }
  if (!existing) { // No match by gameid, so check (base+name+pubtime).
    struct db_game *q=db_game_get_by_index(ra.db,0);
    int i=db_game_count(ra.db);
    for (;i-->0;q++) {
      if (q->pubtime&&game.pubtime&&(q->pubtime!=game.pubtime)) continue;
      if (strncmp(q->name,game.name,DB_GAME_NAME_LIMIT)) continue;
      if (strncmp(q->base,game.base,DB_GAME_BASE_LIMIT)) continue;
      existing=q;
      break;
    }
  }
  
  /* If we don't have it yet, try to download the ROM file.
   */
  if (!existing) {
    char path[1024];
    int pathc=ra_migrate_game_file(path,sizeof(path),ctx,src,srcc);
    if (!pathc) pathc=ra_migrate_gitmake_upgrade_exists_for_game(path,pathc,ctx,game.gameid);
    if (pathc<0) return pathc;
    if (!pathc) {
      fprintf(stderr,"  Game r%d(%.32s), ignoring because we don't seem able to download.\n",game.gameid,game.name);
      ctx->gamecounts.ignorec++;
      return 0;
    }
    fprintf(stderr,"%.*s: Downloaded game r%d(%.32s).\n",pathc,path,game.gameid,game.name);
    if (!(existing=db_game_insert(ra.db,&game))) return -1;
    if (db_game_set_path(ra.db,existing,path,pathc)<0) return -1;
    ctx->gamecounts.addc++;
    int err=ra_migrate_game_comments(ctx,existing->gameid,src,srcc);
    if (err<0) return err;
    return 0;
  }
  
  /* We already have what we've taken to be a match for this game.
   * If anything looks different, change it.
   * TODO This bit is debatable. We can't really say that the incoming data is more correct than ours. It's fair to skip here.
   */
  int dirty=(
    strncmp(game.name,existing->name,DB_GAME_NAME_LIMIT)||
    (game.platform!=existing->platform)||
    (game.author!=existing->author)||
    (game.genre!=existing->genre)||
    (game.flags!=existing->flags)||
    (game.rating!=existing->rating)||
    (game.pubtime!=existing->pubtime)
  );
  if (dirty) {
    game.gameid=existing->gameid;
    game.dir=existing->dir;
    memcpy(game.base,existing->base,DB_GAME_BASE_LIMIT);
    if (!(existing=db_game_update(ra.db,&game))) return -1;
    ctx->gamecounts.modc++;
  } else {
    ctx->gamecounts.skipc++;
  }
  int err=ra_migrate_game_comments(ctx,existing->gameid,src,srcc);
  if (err<0) return err;
  return 0;
}


#endif

/* Migrate games.
 */
 
static int ra_migrate_game(const char *src,int srcc,void *userdata) {
  struct ra_migrate_context *ctx=userdata;
  ctx->games.incomingc++;
  
  fprintf(stderr,"%s:%d:%s: %.*s\n",__FILE__,__LINE__,__func__,srcc,src);
  /*
    {
      "gameid":2,
      "name":"Vigilante 8",
      "platform":"gameboy",
      "author":"Vatical",
      "genre":"Racing",
      "flags":"player1",
      "rating":50,
      "path":"/home/andy/rom/gb/v/vigilante_8.gz",
      "comments":[
        {"time":"2023-10-21T18:09","k":"text","v":"Drive around a farm and shoot things."}
      ],
      "lists":[],
      "blobs":["/home/kiddo/proj/ra3/data/blob/0/2-scap-20221124185240.png"]
    }
  */
  
  struct db_game incoming={0};
  struct sr_decoder comments={0};
  struct sr_decoder blobs={0};
  // We also get lists and plays: Lists will be processed separately, and plays I think should not migrate.
  int err=ra_migrate_decode_game(&incoming,&comments,&blobs,ctx,src,srcc);
  if (err<0) return err;
  
  struct db_game *existing=ra_migrate_find_existing_game(&incoming);
  
  // If we already have it, check whether to update the header record.
  if (existing) {
    struct db_game update={0};
    if (ra_migrate_should_update_game(&update,ctx,existing,&incoming)>0) {
      fprintf(stderr,"UPDATE GAME: %.32s\n",update.name);
      if (!(existing=db_game_update(ra.db,&update))) return -1;
      ctx->gamecounts.modc++;
    } else {
      fprintf(stderr,"KEEP GAME: %.32s\n",existing->name);
      ctx->gamecounts.skipc++;
    }
    
  // If we don't already have it, check whether to insert.
  } else {
    struct db_game update={0};
    if (ra_migrate_should_insert_game(&update,ctx,&incoming)>0) {
      if ((err=ra_migrate_copy_rom_file_if_needed(ctx,&update,&incoming))<0) return err;
      fprintf(stderr,"INSERT GAME: %.32s\n",update.name);
      if (!(existing=db_game_insert(ra.db,&update))) return -1;
      ctx->gamecounts.addc++;
    } else {
      fprintf(stderr,"IGNORE GAME: %.32s\n",inocming.name);
      ctx->gamecounts.ignorec++;
      return 0; // And get out now; don't bother examining blobs and comments.
    }
  }
  
  //TODO comments
  //TODO blobs
  fprintf(stderr,"%s:%d: Resume here too\n",__FILE__,__LINE__);
  return -2;
  
  return 0;
}

/* Migrate launchers.
 */
 
static int ra_migrate_launcher(const char *src,int srcc,void *userdata) {
  struct ra_migrate_context *ctx=userdata;
  fprintf(stderr,"%s:%d:%s: %.*s\n",__FILE__,__LINE__,__func__,srcc,src);
  ctx->launchers.incomingc++;
  return 0;
}

/* Migrate lists.
 */
 
static int ra_migrate_list(const char *src,int srcc,void *userdata) {
  struct ra_migrate_context *ctx=userdata;
  fprintf(stderr,"%s:%d:%s: %.*s\n",__FILE__,__LINE__,__func__,srcc,src);
  ctx->lists.incomingc++;
  return 0;
}

/* Migrate upgrades.
 */
 
static int ra_migrate_upgrade(const char *src,int srcc,void *userdata) {
  struct ra_migrate_context *ctx=userdata;
  fprintf(stderr,"%s:%d:%s: %.*s\n",__FILE__,__LINE__,__func__,srcc,src);
  ctx->upgrades.incomingc++;
  return 0;
}

/* Migrate main, in context.
 */
 
static int ra_migrate_inner(struct ra_migrate_context *ctx) {
  int err;
  
  /* Get things started. Pull the whole remote database, and keep it in JSON as received.
   */
  if ((err=ra_migrate_introduce(ctx))<0) {
    if (err!=-2) fprintf(stderr,"Unspecified error starting migration wizard.\n");
    return -2;
  }
  if ((err=ra_migrate_connect(ctx))<0) {
    if (err!=-2) fprintf(stderr,"Unspecified error connecting to remote Romassist instance '%s'.\n",ra.migrate);
    return -2;
  }
  if ((err=ra_migrate_fetch(ctx))<0) {
    if (err!=-2) fprintf(stderr,"Unspecified error fetching content from remote '%s'.\n",ra.migrate);
    return -2;
  }
  
  /* Now the main operation. Order is important.
   */
  if ((err=sr_for_each_of_json_array(ctx->games.v,ctx->games.c,ra_migrate_game,ctx))<0) {
    if (err!=-2) fprintf(stderr,"Unspecified error procesing games.\n");
    return -2;
  }
  if ((err=sr_for_each_of_json_array(ctx->launchers.v,ctx->launchers.c,ra_migrate_launcher,ctx))<0) {
    if (err!=-2) fprintf(stderr,"Unspecified error processing launchers.\n");
    return -2;
  }
  if ((err=sr_for_each_of_json_array(ctx->lists.v,ctx->lists.c,ra_migrate_list,ctx))<0) {
    if (err!=-2) fprintf(stderr,"Unspecified error processing lists.\n");
    return -2;
  }
  if ((err=sr_for_each_of_json_array(ctx->upgrades.v,ctx->upgrades.c,ra_migrate_upgrade,ctx))<0) {
    if (err!=-2) fprintf(stderr,"Unspecified error processing upgrades.\n");
    return -2;
  }
  
  /* One last chance to detect errors before we commit.
   */
  if ((err=ra_migrate_validate_final(ctx))<0) {
    if (err!=-2) fprintf(stderr,"Unspecified error during final validation.\n");
    ra_migrate_report_changes_after_failed_commit(ctx);
    return -2;
  }
  
  /* Blobs and external files are already committed, but everything else is still just in memory.
   * Commit.
   */
  if (db_save(ra.db)<0) {
    ra_migrate_report_changes_after_failed_commit(ctx);
    return -2;
  }
  
  ra_migrate_report_changes(ctx);
  return 0;
}

/* Migrate, main entry point.
 */
 
int ra_migrate_main() {
  struct ra_migrate_context ctx={0};
  int err=ra_migrate_inner(&ctx);
  ra_migrate_context_cleanup(&ctx);
  return (err<0)?1:0;
}
