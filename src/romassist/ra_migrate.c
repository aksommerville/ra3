#include "ra_migrate_internal.h"

/* Cleanup.
 */

static void ra_migrate_context_cleanup(struct ra_migrate_context *ctx) {
  ra_minhttp_cleanup(&ctx->minhttp);
  sr_encoder_cleanup(&ctx->games);
  sr_encoder_cleanup(&ctx->lists);
  sr_encoder_cleanup(&ctx->launchers);
  sr_encoder_cleanup(&ctx->upgrades);
  if (ctx->idchangev) free(ctx->idchangev);
}

/* Migrate one comment, in the context of a game.
 */
 
struct ra_migrate_comment_context {
  struct ra_migrate_context *ctx;
  struct db_game *game; // resident locally
  // It's tempting to put resident comments here too, so we only have to fetch them once, but those might change during an add.
  // Safer to force the callback to fetch them for each incoming comment.
};

static int ra_migrate_comment(const char *src,int srcc,void *userdata) {
  struct ra_migrate_context *ctx=((struct ra_migrate_comment_context*)userdata)->ctx;
  struct db_game *game=((struct ra_migrate_comment_context*)userdata)->game;
  ctx->commentcounts.incomingc++;
  
  struct db_comment comment={0};
  if (db_comment_decode(&comment,ra.db,DB_FORMAT_json,src,srcc)<0) return -1;
  comment.gameid=game->gameid;
  
  /* Skip it if we have an identical comment: k,time,v
   * Otherwise add it exactly as presented, except for gameid. Mostly.
   */
  struct db_comment *existing=0;
  int i=db_comments_get_by_gameid(&existing,ra.db,game->gameid);
  for (;i-->0;existing++) {
    if (existing->k!=comment.k) continue;
    if (existing->time!=comment.time) continue;
    if (existing->v!=comment.v) continue;
    ctx->commentcounts.skipc++;
    return 0;
  }
  
  /* When (k) is "args", apply some adjustment.
   * These often contain paths on the host filesystem.
   */
  if ((comment.k==ctx->str_args)&&comment.v) {
    if (!(comment.v=ra_migrate_replace_paths_in_string(ctx,comment.v))) return -1;
  }
  
  if (!db_comment_insert(ra.db,&comment)) return -1;
  ctx->commentcounts.addc++;
  
  return 0;
}

/* Migrate one blob, in the context of a game.
 */
 
struct ra_migrate_blob_context {
  struct ra_migrate_context *ctx;
  struct db_game *game; // resident locally
};

static int ra_migrate_blob(const char *src,int srcc,void *userdata) {
  struct ra_migrate_context *ctx=((struct ra_migrate_blob_context*)userdata)->ctx;
  struct db_game *game=((struct ra_migrate_blob_context*)userdata)->game;
  ctx->blobcounts.incomingc++;
  
  char rpath[1024];
  int rpathc=sr_string_eval(rpath,sizeof(rpath),src,srcc);
  if ((rpathc<0)||(rpathc>=sizeof(rpath))) return -1;
  char lpath[1024];
  int lpathc=ra_migrate_local_path_for_blob(lpath,sizeof(lpath),ctx,game->gameid,rpath,rpathc);
  if ((lpathc<1)||(lpathc>=sizeof(lpath))) return -1;
  
  // File already exists? Great, we're done.
  if (file_get_type(lpath)=='f') {
    ctx->blobcounts.skipc++;
    return 0;
  }
  
  // Download from the remote.
  struct sr_encoder rsp={0};
  char req[1024];
  int reqc=snprintf(req,sizeof(req),"/api/blob?path=%.*s",rpathc,rpath); // don't bother url-encoding. just assume it doesn't contain '&'
  if ((reqc<1)||(reqc>=sizeof(req))) return -1;
  int status=ra_minhttp_request_sync(0,&rsp,&ctx->minhttp,"GET",req);
  if (status!=200) {
    fprintf(stderr,"Failed to download blob '%.*s' for game '%.32s', status %d.\n",rpathc,rpath,game->name,status);
    sr_encoder_cleanup(&rsp);
    return -1;
  }
  if ((dir_mkdirp_parent(lpath)<0)||(file_write(lpath,rsp.v,rsp.c)<0)) {
    fprintf(stderr,"%s: Failed to write %d-byte blob\n",lpath,rsp.c);
    sr_encoder_cleanup(&rsp);
    return -1;
  }
  fprintf(stderr,"%s: Wrote %d-byte blob for game '%.32s'\n",lpath,rsp.c,game->name);
  sr_encoder_cleanup(&rsp);
  ctx->blobcounts.addc++;
  
  return 0;
}

/* Migrate games.
 */
 
static int ra_migrate_game(const char *src,int srcc,void *userdata) {
  struct ra_migrate_context *ctx=userdata;
  ctx->gamecounts.incomingc++;
  
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
      if (!(existing=db_game_update(ra.db,&update))) return -1;
      ctx->gamecounts.modc++;
    } else {
      ctx->gamecounts.skipc++;
    }
    
  // If we don't already have it, check whether to insert.
  } else {
    struct db_game update={0};
    if (ra_migrate_should_insert_game(&update,ctx,&incoming)>0) {
      if ((err=ra_migrate_copy_rom_file_if_needed(ctx,&update,&incoming))<0) return err;
      if (err) ctx->romdownloadc++;
      if (!(existing=db_game_insert(ra.db,&update))) return -1;
      ctx->gamecounts.addc++;
    } else {
      ctx->gamecounts.ignorec++;
      if (ra_migrate_idchange_add(ctx,'g',0,incoming.gameid)<0) return -1;
      return 0; // And get out now; don't bother examining blobs and comments.
    }
  }
  if (existing->gameid!=incoming.gameid) {
    if (ra_migrate_idchange_add(ctx,'g',existing->gameid,incoming.gameid)<0) return -1;
  }
  
  // Take the comments and blobs.
  if (comments.c) {
    struct ra_migrate_comment_context subctx={
      .ctx=ctx,
      .game=existing,
    };
    if ((err=sr_for_each_of_json_array(comments.v,comments.c,ra_migrate_comment,&subctx))<0) return err;
  }  
  if (blobs.c) {
    struct ra_migrate_blob_context subctx={
      .ctx=ctx,
      .game=existing,
    };
    if ((err=sr_for_each_of_json_array(blobs.v,blobs.c,ra_migrate_blob,&subctx))<0) return err;
  }
  
  return 0;
}

/* Migrate launchers.
 */
 
static int ra_migrate_launcher(const char *src,int srcc,void *userdata) {
  struct ra_migrate_context *ctx=userdata;
  ctx->launchercounts.incomingc++;
  
  struct db_launcher incoming={0};
  if (db_launcher_decode(&incoming,ra.db,DB_FORMAT_json,src,srcc)<0) return -1;
  
  struct db_launcher update={0};
  struct db_launcher *existing=ra_migrate_consider_launcher(&update,ctx,&incoming);
  if (!existing) {
    if (!update.cmd) {
      ctx->launchercounts.ignorec++;
      if (ra_migrate_idchange_add(ctx,'a',0,incoming.launcherid)<0) return -1;
      return 0;
    }
    if (!(existing=db_launcher_insert(ra.db,&update))) return -1;
    ctx->launchercounts.addc++;
    // At this point, (cmd) is probably wrong. But we're going to let the user worry about that.
  } else if (!update.cmd) {
    ctx->launchercounts.skipc++;
  } else {
    if (!(existing=db_launcher_update(ra.db,&update))) return -1;
    ctx->launchercounts.modc++;
  }
  if (existing->launcherid!=incoming.launcherid) {
    if (ra_migrate_idchange_add(ctx,'a',existing->launcherid,incoming.launcherid)<0) return -1;
  }
  
  return 0;
}

/* Migrate lists.
 */
 
static int ra_migrate_list(const char *src,int srcc,void *userdata) {
  struct ra_migrate_context *ctx=userdata;
  ctx->listcounts.incomingc++;
  
  struct db_list incoming={0};
  if (db_list_decode(&incoming,ra.db,DB_FORMAT_json,src,srcc)<0) {
    db_list_cleanup(&incoming);
    return -1;
  }
  
  struct db_list update={0};
  struct db_list *existing=ra_migrate_consider_list(&update,ctx,&incoming);
  if (!existing) {
    if (!update.name) {
      ctx->listcounts.ignorec++;
      db_list_cleanup(&incoming);
      db_list_cleanup(&update);
      if (ra_migrate_idchange_add(ctx,'i',0,incoming.listid)<0) return -1;
      return 0;
    }
    if (!(existing=db_list_insert(ra.db,&update))) {
      db_list_cleanup(&incoming);
      db_list_cleanup(&update);
      return -1;
    }
    ctx->listcounts.addc++;
  } else if (!update.name) {
    ctx->listcounts.skipc++;
  } else {
    if (!(existing=db_list_update(ra.db,&update))) {
      db_list_cleanup(&incoming);
      db_list_cleanup(&update);
      return -1;
    }
    ctx->launchercounts.modc++;
  }
  
  if (existing->listid!=incoming.listid) {
    if (ra_migrate_idchange_add(ctx,'i',existing->listid,incoming.listid)<0) {
      db_list_cleanup(&incoming);
      db_list_cleanup(&update);
      return -1;
    }
  }
  db_list_cleanup(&incoming);
  db_list_cleanup(&update);
  return 0;
}

/* Migrate upgrades.
 */
 
static int ra_migrate_upgrade(const char *src,int srcc,void *userdata) {
  struct ra_migrate_context *ctx=userdata;
  ctx->upgradecounts.incomingc++;
  
  struct db_upgrade incoming={0};
  if (db_upgrade_decode(&incoming,ra.db,DB_FORMAT_json,src,srcc)<0) return -1;
  
  struct db_upgrade update={0};
  struct db_upgrade *existing=ra_migrate_consider_upgrade(&update,ctx,&incoming);
  if (!existing) {
    if (!update.method) {
      ctx->upgradecounts.ignorec++;
      if (ra_migrate_idchange_add(ctx,'u',0,incoming.upgradeid)<0) return -1;
      return 0;
    }
    if (!(existing=db_upgrade_insert(ra.db,&update))) return -1;
    ctx->upgradecounts.addc++;
  } else if (!update.method) {
    ctx->upgradecounts.skipc++;
  } else {
    if (!(existing=db_upgrade_update(ra.db,&update))) return -1;
    ctx->upgradecounts.modc++;
  }
  if (existing->upgradeid!=incoming.upgradeid) {
    if (ra_migrate_idchange_add(ctx,'u',existing->upgradeid,incoming.upgradeid)<0) return -1;
  }
  
  //TODO Would be really cool to clone repositories initially, for upgrades we've added.
  // Right now, I think that is both too complicated for my appetite, and possibly too presumptuous of us.
  
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
