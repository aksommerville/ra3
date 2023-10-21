#include "ra_internal.h"
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"

/* Read and parse a "detail" query param.
 */
 
static int ra_http_get_detail(struct http_xfer *req,int fallback) {
  char detailstr[16];
  int detailstrc=http_xfer_get_query_string(detailstr,sizeof(detailstr),req,"detail",6);
  if (detailstrc<1) return fallback;
  if (detailstrc>sizeof(detailstr)) return fallback;
  #define _(tag) if ((detailstrc==sizeof(#tag)-1)&&!memcmp(detailstr,#tag,detailstrc)) return DB_DETAIL_##tag;
  DB_DETAIL_FOR_EACH
  #undef _
  return fallback;
}

/* MIME magic.
 */
 
static const char *ra_http_guess_content_type(const uint8_t *src,int srcc,const char *path) {

  // If we recognize the suffix, trust it blindly.
  if (path) {
    const char *sfxsrc=0;
    int sfxsrcc=0;
    int i=0;
    for (;path[i];i++) {
      if (path[i]=='/') {
        sfxsrc=0;
        sfxsrcc=0;
      } else if (path[i]=='.') {
        sfxsrc=path+i+1;
        sfxsrcc=0;
      } else if (sfxsrc) {
        sfxsrcc++;
      }
    }
    char sfx[16];
    if ((sfxsrcc>0)&&(sfxsrcc<=sizeof(sfx))) {
      int i=sfxsrcc; while (i-->0) {
        if ((sfxsrc[i]>='A')&&(sfxsrc[i]<='Z')) sfx[i]=sfxsrc[i]+0x20;
        else sfx[i]=sfxsrc[i];
      }
      switch (sfxsrcc) {
        case 1: switch (sfx[0]) {
          } break;
        case 2: {
            if (!memcmp(sfx,"js",2)) return "application/javascript";
          } break;
        case 3: {
            if (!memcmp(sfx,"css",3)) return "text/css";
            if (!memcmp(sfx,"ico",3)) return "image/x-icon";
            if (!memcmp(sfx,"png",3)) return "image/png";
            if (!memcmp(sfx,"gif",3)) return "image/gif";
          } break;
        case 4: {
            if (!memcmp(sfx,"html",4)) return "text/html";
          } break;
      }
    }
  }
  
  // If it has an unambiguous signature, use it.
  if ((srcc>=8)&&!memcmp(src,"\x89PNG\r\n\x1a\n",8)) return "image/png";
  if ((srcc>=6)&&!memcmp(src,"GIF87a",6)) return "image/gif";
  if ((srcc>=6)&&!memcmp(src,"GIF89a",6)) return "image/gif";
  if ((srcc>=15)&&!memcmp(src,"<!DOCTYPE html>",15)) return "text/html";
  
  // Generic text or binary based on provided serial (limit 256 bytes). Default text.
  if (srcc>256) srcc=256;
  for (;srcc-->0;src++) {
    if (*src==0x09) continue;
    if (*src==0x0a) continue;
    if (*src==0x0d) continue;
    if (*src<0x20) return "application/octet-stream";
    if (*src>0x7e) return "application/octet-stream";
  }
  return "text/plain";
}

/* GET static file, path verified.
 */
 
static int ra_http_serve_static(struct http_xfer *req,struct http_xfer *rsp,const char *path) {
  void *serial=0;
  int serialc=file_read(&serial,path);
  if (serialc<0) return http_xfer_set_status(rsp,404,"Not found.");
  http_xfer_set_header(rsp,"Content-Type",12,ra_http_guess_content_type(serial,serialc,path),-1);
  int err=http_xfer_append_body(rsp,serial,serialc);
  free(serial);
  if (err<0) return -1;
  return http_xfer_set_status(rsp,200,"OK");
}

/* GET static file.
 */
 
int ra_http_static(struct http_xfer *req,struct http_xfer *rsp,void *userdata) {
  if (!ra.htdocs) return http_xfer_set_status(rsp,500,"Static file service disabled.");
  const char *rpath=0;
  int rpathc=http_xfer_get_path(&rpath,req);
  if ((rpathc<1)||(rpath[0]!='/')) return http_xfer_set_status(rsp,400,"Invalid request path.");
  
  // One special thing only: "/" => "/index.html". Otherwise they get exactly what they ask for.
  if ((rpathc==1)&&(rpath[0]=='/')) {
    rpath="/index.html";
    rpathc=11;
  }
  
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s%.*s",ra.htdocs,rpathc,rpath);
  if ((pathc<1)||(pathc>=sizeof(path))) return http_xfer_set_status(rsp,400,"Invalid request path.");
  char *real=realpath(path,0);
  int htdocsc=0; while (ra.htdocs[htdocsc]) htdocsc++;
  if (!real||memcmp(real,ra.htdocs,htdocsc)) {
    if (real) free(real);
    return http_xfer_set_status(rsp,404,"Not found.");
  }
  int err=ra_http_serve_static(req,rsp,real);
  free(real);
  return err;
}

/* Helper for all resource-oriented GET calls by (index,count).
 * Returns >0 on success, <0 for real errors, or 0 if no "index" param.
 */
 
static int ra_http_get_by_index(
  struct http_xfer *req,struct http_xfer *rsp,
  int objlen,
  int (*get_total)(const struct db *db),
  const void *(*get_record)(const struct db *db,int p),
  int (*encode)(struct sr_encoder *dst,struct db *db,const void *record,int format,int detail)
) {
  int index,count=1;
  if (http_xfer_get_query_int(&index,req,"index",5)<0) return 0;
  struct sr_encoder *dst=http_xfer_get_body_encoder(rsp);
  int jsonctx=sr_encode_json_array_start(dst,0,0);
  if (index>=0) {
    http_xfer_get_query_int(&count,req,"count",5);
    if (count<0) count=0;
    int total=get_total(ra.db);
    if (index>total-count) count=total-index;
    const uint8_t *record=get_record(ra.db,index);
    if (record) {
      int detail=ra_http_get_detail(req,DB_DETAIL_record);
      for (;count--;record+=objlen) {
        if (sr_encode_json_setup(dst,0,0)<0) return -1;
        if (encode(dst,ra.db,record,DB_FORMAT_json,detail)<0) return -1;
      }
    }
  }
  if (sr_encode_json_array_end(dst,jsonctx)<0) return -1;
  return 1;
}

/* Helper for PATCH calls, where we need the record ID from payload, before fully decoding the payload.
 */
 
static uint32_t ra_http_extract_json_id(const char *src,int srcc,const char *idk,int idkc) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_object_start(&decoder)<0) return 0;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    if ((kc==idkc)&&!memcmp(k,idk,kc)) {
      int id;
      if (sr_decode_json_int(&id,&decoder)<0) return 0;
      return id;
    }
    if (sr_decode_json_skip(&decoder)<0) return 0;
  }
  return 0;
}

/* GET /api/meta/flags
 */
 
static int ra_http_get_flags(struct http_xfer *req,struct http_xfer *rsp) {
  struct sr_encoder *dst=http_xfer_get_body_encoder(rsp);
  int jsonctx=sr_encode_json_array_start(dst,0,0);
  uint32_t flag=1;
  for (;flag;flag<<=1) {
    const char *name=db_flag_repr(flag);
    sr_encode_json_string(dst,0,0,name,-1);
  }
  return sr_encode_json_array_end(dst,jsonctx);
}

/* GET /api/meta/platform
 * GET /api/meta/author
 * GET /api/meta/genre
 */
 
static int ra_http_finish_histogram(struct http_xfer *req,struct http_xfer *rsp,struct db_histogram *hist,int status) {
  if (status>=0) {
    struct sr_encoder *dst=http_xfer_get_body_encoder(rsp);
    if (db_histogram_encode(dst,ra.db,hist,DB_FORMAT_json,ra_http_get_detail(req,DB_DETAIL_id))<0) return -1;
  }
  db_histogram_cleanup(hist);
  return status;
}
 
static int ra_http_get_platform(struct http_xfer *req,struct http_xfer *rsp) {
  struct db_histogram hist={0};
  return ra_http_finish_histogram(req,rsp,&hist,db_histogram_platform(&hist,ra.db));
}
 
static int ra_http_get_author(struct http_xfer *req,struct http_xfer *rsp) {
  struct db_histogram hist={0};
  return ra_http_finish_histogram(req,rsp,&hist,db_histogram_author(&hist,ra.db));
}
 
static int ra_http_get_genre(struct http_xfer *req,struct http_xfer *rsp) {
  struct db_histogram hist={0};
  return ra_http_finish_histogram(req,rsp,&hist,db_histogram_genre(&hist,ra.db));
}
  
/* GET /api/game/count
 */
 
static int ra_http_count_game(struct http_xfer *req,struct http_xfer *rsp) {
  return sr_encode_json_int(http_xfer_get_body_encoder(rsp),0,0,db_game_count(ra.db));
}

/* GET /api/game
 */

static int ra_http_get_game(struct http_xfer *req,struct http_xfer *rsp) {
  int err=ra_http_get_by_index(req,rsp,sizeof(struct db_game),db_game_count,(void*)db_game_get_by_index,(void*)db_game_encode);
  if (err) return err;
  
  int gameid;
  if (http_xfer_get_query_int(&gameid,req,"gameid",6)>=0) {
    int detail=ra_http_get_detail(req,DB_DETAIL_record);
    struct sr_encoder *dst=http_xfer_get_body_encoder(rsp);
    const struct db_game *game=db_game_get_by_id(ra.db,gameid);
    if (!game) return http_xfer_set_status(rsp,404,"Not found");
    return db_game_encode(dst,ra.db,game,DB_FORMAT_json,detail);
  }
  
  return http_xfer_set_status(rsp,400,"Expected 'index' or 'gameid'");
}

/* PUT /api/game
 */
 
static int ra_http_put_game(struct http_xfer *req,struct http_xfer *rsp) {
  const char *src=0;
  int srcc=http_xfer_get_body(&src,req);
  if (srcc<0) return -1;
  struct db_game scratch={0};
  if (db_game_decode(&scratch,ra.db,DB_FORMAT_json,src,srcc)<0) return http_xfer_set_status(rsp,400,"Malformed game");
  const struct db_game *game=db_game_insert(ra.db,&scratch);
  if (!game) {
    if (db_game_get_by_id(ra.db,scratch.gameid)) return http_xfer_set_status(rsp,400,"gameid %d already in use",scratch.gameid);
    return -1;
  }
  return db_game_encode(http_xfer_get_body_encoder(rsp),ra.db,game,DB_FORMAT_json,ra_http_get_detail(req,DB_DETAIL_record));
}
  
/* PATCH /api/game
 */
 
static int ra_http_patch_game(struct http_xfer *req,struct http_xfer *rsp) {
  const char *src=0;
  int srcc=http_xfer_get_body(&src,req);
  if (srcc<0) return -1;
  uint32_t gameid=ra_http_extract_json_id(src,srcc,"gameid",6);
  if (!gameid) return http_xfer_set_status(rsp,400,"gameid required");
  const struct db_game *pre=db_game_get_by_id(ra.db,gameid);
  if (!pre) return http_xfer_set_status(rsp,404,"Not found");
  struct db_game scratch;
  memcpy(&scratch,pre,sizeof(struct db_game));
  if (db_game_decode(&scratch,ra.db,DB_FORMAT_json,src,srcc)<0) return http_xfer_set_status(rsp,400,"Malformed game");
  const struct db_game *game=db_game_update(ra.db,game);
  if (!game) return -1;
  return db_game_encode(http_xfer_get_body_encoder(rsp),ra.db,game,DB_FORMAT_json,ra_http_get_detail(req,DB_DETAIL_record));
}

/* DELETE /api/game
 */
 
static int ra_http_delete_game(struct http_xfer *req,struct http_xfer *rsp) {
  int gameid=0;
  if (http_xfer_get_query_int(&gameid,req,"gameid",6)<0) return http_xfer_set_status(rsp,400,"gameid required");
  return db_game_delete(ra.db,gameid);
}

/* GET /api/comment/count
 */
 
static int ra_http_count_comment(struct http_xfer *req,struct http_xfer *rsp) {
  return sr_encode_json_int(http_xfer_get_body_encoder(rsp),0,0,db_comment_count(ra.db));
}

/* GET /api/comment
 */

static int ra_http_get_comment(struct http_xfer *req,struct http_xfer *rsp) {
  int err=ra_http_get_by_index(req,rsp,sizeof(struct db_comment),db_comment_count,(void*)db_comment_get_by_index,(void*)db_comment_encode);
  if (err) return err;
  
  int gameid;
  if (http_xfer_get_query_int(&gameid,req,"gameid",6)>=0) {
    uint32_t mintime=0;
    char arg[64];
    int argc=http_xfer_get_query_string(arg,sizeof(arg),req,"time",4);
    if ((argc>0)&&(argc<=sizeof(arg))) {
      mintime=db_time_eval(arg,argc);
    }
    uint32_t k=0;
    argc=http_xfer_get_query_string(arg,sizeof(arg),req,"k",1);
    if ((argc>0)&&(argc<=sizeof(arg))) {
      if (!(k=db_string_lookup(ra.db,arg,argc))) {
        // Short circuit. (k) not interned means there can't be any comments matching it.
        return http_xfer_set_body(rsp,"[]",2);
      }
    }
    struct db_comment *comment=0;
    int commentc=db_comments_get_by_gameid(&comment,ra.db,gameid);
    struct sr_encoder *dst=http_xfer_get_body_encoder(rsp);
    int jsonctx=sr_encode_json_array_start(dst,0,0);
    for (;commentc-->0;comment++) {
      if (comment->time<mintime) continue;
      if (k&&(comment->k!=k)) continue;
      if (sr_encode_json_setup(dst,0,0)<0) return -1;
      if (db_comment_encode(dst,ra.db,comment,DB_FORMAT_json,DB_DETAIL_record)<0) return -1;
    }
    return sr_encode_json_array_end(dst,jsonctx);
  }
  
  return http_xfer_set_status(rsp,400,"Expected 'index' or 'gameid'");
}
  
/* PUT /api/comment
 */

static int ra_http_put_comment(struct http_xfer *req,struct http_xfer *rsp) {
  const char *src=0;
  int srcc=http_xfer_get_body(&src,req);
  if (srcc<0) return -1;
  struct db_comment scratch={0};
  if (db_comment_decode(&scratch,ra.db,DB_FORMAT_json,src,srcc)<0) return http_xfer_set_status(rsp,400,"Malformed comment");
  const struct db_comment *comment=db_comment_insert(ra.db,&scratch);
  if (!comment) return -1;
  return db_comment_encode(http_xfer_get_body_encoder(rsp),ra.db,comment,DB_FORMAT_json,DB_DETAIL_record);
}

/* PATCH /api/comment
 */
 
static int ra_http_patch_comment(struct http_xfer *req,struct http_xfer *rsp) {
  const char *src=0;
  int srcc=http_xfer_get_body(&src,req);
  if (srcc<0) return -1;
  struct db_comment scratch={0};
  if (db_comment_decode(&scratch,ra.db,DB_FORMAT_json,src,srcc)<0) return http_xfer_set_status(rsp,400,"Malformed comment");
  // There's only one field (v) that could change in a comment, the other 3 are the primary key.
  // So we're violating PATCH semantics if (v) wasn't provided in the input, but whatever.
  if (db_comment_insert(ra.db,&scratch)<0) return -1;
  return db_comment_encode(http_xfer_get_body_encoder(rsp),ra.db,&scratch,DB_FORMAT_json,DB_DETAIL_record);
}

/* DELETE /api/comment
 */
 
static int ra_http_delete_comment(struct http_xfer *req,struct http_xfer *rsp) {
  int gameid;
  if (http_xfer_get_query_int(&gameid,req,"gameid",6)<0) return http_xfer_set_status(rsp,400,"gameid required");
  uint32_t time,k;
  char arg[64];
  int argc=http_xfer_get_query_string(arg,sizeof(arg),req,"time",4);
  if ((argc<0)||(argc>sizeof(arg))) return http_xfer_set_status(rsp,400,"time required");
  time=db_time_eval(arg,argc);
  argc=http_xfer_get_query_string(arg,sizeof(arg),req,"k",1);
  if ((argc<0)||(argc>sizeof(arg))) return http_xfer_set_status(rsp,400,"k required");
  k=db_string_intern(ra.db,arg,argc);
  struct db_comment comment={.gameid=gameid,.time=time,.k=k};
  return db_comment_delete(ra.db,&comment);
}

/* GET /api/play/count
 */
 
static int ra_http_count_play(struct http_xfer *req,struct http_xfer *rsp) {
  return sr_encode_json_int(http_xfer_get_body_encoder(rsp),0,0,db_play_count(ra.db));
}

/* GET /api/play
 */

static int ra_http_get_play(struct http_xfer *req,struct http_xfer *rsp) {
  int err=ra_http_get_by_index(req,rsp,sizeof(struct db_play),db_play_count,(void*)db_play_get_by_index,(void*)db_play_encode);
  if (err) return err;
  
  int gameid;
  if (http_xfer_get_query_int(&gameid,req,"gameid",6)<0) return http_xfer_set_status(rsp,400,"gameid required");
  uint32_t since=0;
  char arg[32];
  int argc=http_xfer_get_query_string(arg,sizeof(arg),req,"since",5);
  if ((argc>0)&&(argc<=sizeof(arg))) since=db_time_eval(arg,argc);
  struct sr_encoder *dst=http_xfer_get_body_encoder(rsp);
  int jsonctx=sr_encode_json_array_start(dst,0,0);
  struct db_play *play=0;
  int playc=db_plays_get_by_gameid(&play,ra.db,gameid);
  for (;playc-->0;play) {
    if (play->time<since) continue;
    if (sr_encode_json_setup(dst,0,0)<0) return -1;
    if (db_play_encode(dst,ra.db,play,DB_FORMAT_json,DB_DETAIL_record)<0) return -1;
  }
  return sr_encode_json_array_end(dst,jsonctx);
}

/* PUT /api/play
 */
 
static int ra_http_put_play(struct http_xfer *req,struct http_xfer *rsp) {
  const char *src=0;
  int srcc=http_xfer_get_body(&src,req);
  if (srcc<0) return -1;
  struct db_play scratch={0};
  if (db_play_decode(&scratch,ra.db,DB_FORMAT_json,src,srcc)<0) return http_xfer_set_status(rsp,400,"Malformed play");
  if (!db_game_get_by_id(ra.db,scratch.gameid)) return http_xfer_set_status(rsp,404,"Game %d not found",scratch.gameid);
  const struct db_play *play=db_play_insert(ra.db,&scratch);
  if (!play) return -1;
  return db_play_encode(http_xfer_get_body_encoder(rsp),ra.db,play,DB_FORMAT_json,DB_DETAIL_record);
}

/* PATCH /api/play
 */

static int ra_http_patch_play(struct http_xfer *req,struct http_xfer *rsp) {
  const char *src=0;
  int srcc=http_xfer_get_body(&src,req);
  if (srcc<0) return -1;
  struct db_play scratch={0};
  if (db_play_decode(&scratch,ra.db,DB_FORMAT_json,src,srcc)<0) return http_xfer_set_status(rsp,400,"Malformed play");
  // This is not a true PATCH, but same as comments, we only have one mutable field.
  const struct db_play *play=db_play_update(ra.db,&scratch);
  if (!play) return -1;
  return db_play_encode(http_xfer_get_body_encoder(rsp),ra.db,play,DB_FORMAT_json,DB_DETAIL_record);
}

/* DELETE /api/play
 */
 
static int ra_http_delete_play(struct http_xfer *req,struct http_xfer *rsp) {
  int gameid;
  if (http_xfer_get_query_int(&gameid,req,"gameid",6)<0) return http_xfer_set_status(rsp,400,"gameid required");
  char arg[32];
  int argc=http_xfer_get_query_string(arg,sizeof(arg),req,"time",4);
  if ((argc<1)||(argc>sizeof(arg))) return http_xfer_set_status(rsp,400,"time required");
  uint32_t time=db_time_eval(arg,argc);
  return db_play_delete(ra.db,gameid,time);
}

/* POST /api/play/terminate
 */

static int ra_http_terminate_play(struct http_xfer *req,struct http_xfer *rsp) {
  int gameid;
  if (http_xfer_get_query_int(&gameid,req,"gameid",6)<0) return http_xfer_set_status(rsp,400,"gameid required");
  const struct db_play *play=db_play_finish(ra.db,gameid);
  if (!play) return http_xfer_set_status(rsp,404,"No unfinished play for game %d",gameid);
  return db_play_encode(http_xfer_get_body_encoder(rsp),ra.db,play,DB_FORMAT_json,DB_DETAIL_record);
}

/* GET /api/launcher/count
 */

static int ra_http_count_launcher(struct http_xfer *req,struct http_xfer *rsp) {
  return sr_encode_json_int(http_xfer_get_body_encoder(rsp),0,0,db_launcher_count(ra.db));
}

/* GET /api/launcher
 */

static int ra_http_get_launcher(struct http_xfer *req,struct http_xfer *rsp) {
  int err=ra_http_get_by_index(req,rsp,sizeof(struct db_launcher),db_launcher_count,(void*)db_launcher_get_by_index,(void*)db_launcher_encode);
  if (err) return err;
  int id;
  
  if (http_xfer_get_query_int(&id,req,"launcherid",10)>=0) {
    const struct db_launcher *launcher=db_launcher_get_by_id(ra.db,id);
    if (!launcher) return http_xfer_set_status(rsp,404,"Not found");
    return db_launcher_encode(http_xfer_get_body_encoder(rsp),ra.db,launcher,DB_FORMAT_json,DB_DETAIL_record);
  }
  
  if (http_xfer_get_query_int(&id,req,"gameid",6)>=0) {
    const struct db_launcher *launcher=db_launcher_for_gameid(ra.db,id);
    if (!launcher) return http_xfer_set_status(rsp,404,"No such game or unable to determine launcher");
    return db_launcher_encode(http_xfer_get_body_encoder(rsp),ra.db,launcher,DB_FORMAT_json,DB_DETAIL_record);
  }
  
  return http_xfer_set_status(rsp,400,"Expected 'index', 'launcherid', or 'gameid'");
}
  
/* PUT /api/launcher
 */
 
static int ra_http_put_launcher(struct http_xfer *req,struct http_xfer *rsp) {
  const char *src=0;
  int srcc=http_xfer_get_body(&src,req);
  if (srcc<0) return -1;
  struct db_launcher scratch={0};
  if (db_launcher_decode(&scratch,ra.db,DB_FORMAT_json,src,srcc)<0) return http_xfer_set_status(rsp,400,"Malformed launcher");
  const struct db_launcher *launcher=db_launcher_insert(ra.db,&scratch);
  if (!launcher) {
    if (db_launcher_get_by_id(ra.db,scratch.launcherid)) {
      return http_xfer_set_status(rsp,400,"Launcherid %d in use",scratch.launcherid);
    }
    return -1;
  }
  return db_launcher_encode(http_xfer_get_body_encoder(rsp),ra.db,launcher,DB_FORMAT_json,DB_DETAIL_record);
}

/* PATCH /api/launcher
 */
 
static int ra_http_patch_launcher(struct http_xfer *req,struct http_xfer *rsp) {
  const char *src=0;
  int srcc=http_xfer_get_body(&src,req);
  if (srcc<0) return -1;
  uint32_t launcherid=ra_http_extract_json_id(src,srcc,"launcherid",10);
  if (!launcherid) return http_xfer_set_status(rsp,400,"launcherid required");
  const struct db_launcher *prev=db_launcher_get_by_id(ra.db,launcherid);
  if (!prev) return http_xfer_set_status(rsp,404,"Not found");
  struct db_launcher scratch=*prev;
  if (db_launcher_decode(&scratch,ra.db,DB_FORMAT_json,src,srcc)<0) return http_xfer_set_status(rsp,400,"Malformed launcher");
  const struct db_launcher *launcher=db_launcher_update(ra.db,&scratch);
  if (!launcher) return -1;
  return db_launcher_encode(http_xfer_get_body_encoder(rsp),ra.db,launcher,DB_FORMAT_json,DB_DETAIL_record);
}

/* DELETE /api/launcher
 */
 
static int ra_http_delete_launcher(struct http_xfer *req,struct http_xfer *rsp) {
  int launcherid=0;
  if (http_xfer_get_query_int(&launcherid,req,"launcherid",10)<0) return http_xfer_set_status(rsp,400,"launcherid required");
  return db_launcher_delete(ra.db,launcherid);
}

/* GET /api/list/count
 */
 
static int ra_http_count_list(struct http_xfer *req,struct http_xfer *rsp) {
  return sr_encode_json_int(http_xfer_get_body_encoder(rsp),0,0,db_list_count(ra.db));
}

/* GET /api/list
 */
 
static int ra_http_get_list(struct http_xfer *req,struct http_xfer *rsp) {
  struct sr_encoder *dst=http_xfer_get_body_encoder(rsp);
  int detail=ra_http_get_detail(req,DB_DETAIL_name);
  
  // Can't use ra_http_get_by_index, as lists are not stored contiguously in memory.
  int index;
  if (http_xfer_get_query_int(&index,req,"index",5)>=0) {
    int count=1;
    http_xfer_get_query_int(&count,req,"count",5);
    int jsonctx=sr_encode_json_array_start(dst,0,0);
    if (jsonctx<0) return -1;
    for (;count-->0;index++) {
      const struct db_list *list=db_list_get_by_index(ra.db,index);
      if (!list) break;
      if (sr_encode_json_setup(dst,0,0)<0) return -1;
      if (db_list_encode(dst,ra.db,list,DB_FORMAT_json,detail)<0) return -1;
    }
    return sr_encode_json_array_end(dst,jsonctx);
  }
  
  // Plain old get-one-resource.
  int id;
  if (http_xfer_get_query_int(&id,req,"listid",6)>=0) {
    const struct db_list *list=db_list_get_by_id(ra.db,id);
    if (!list) return http_xfer_set_status(rsp,404,"Not found");
    return db_list_encode(dst,ra.db,list,DB_FORMAT_json,detail);
  }
  
  // Get multiple lists by gameid.
  if (http_xfer_get_query_int(&id,req,"gameid",6)>=0) {
    int jsonctx=sr_encode_json_array_start(dst,0,0);
    if (jsonctx<0) return -1;
    if (db_game_get_by_id(ra.db,id)) { // No sense checking every list, if the game doesn't exist.
      for (index=0;;index++) {
        const struct db_list *list=db_list_get_by_index(ra.db,index);
        if (!list) break;
        if (!db_list_has(list,id)) continue;
        if (sr_encode_json_setup(dst,0,0)<0) return -1;
        if (db_list_encode(dst,ra.db,list,DB_FORMAT_json,detail)<0) return -1;
      }
    }
    return sr_encode_json_array_end(dst,jsonctx);
  }
  
  return http_xfer_set_status(rsp,400,"Expected 'index', 'listid', or 'gameid'");
}

/* PUT /api/list
 */
 
static int ra_http_put_list(struct http_xfer *req,struct http_xfer *rsp) {
  const char *src=0;
  int srcc=http_xfer_get_body(&src,req);
  if (srcc<0) return -1;
  struct db_list *scratch=db_list_copy_nonresident(0);
  if (!scratch) return -1;
  if (db_list_decode(scratch,ra.db,DB_FORMAT_json,src,srcc)<0) {
    db_list_del(scratch);
    return -1;
  }
  const struct db_list *list=db_list_insert(ra.db,scratch);
  db_list_del(scratch);
  if (!list) return -1;
  return db_list_encode(http_xfer_get_body_encoder(rsp),ra.db,list,DB_FORMAT_json,ra_http_get_detail(req,DB_DETAIL_id));
}

/* PATCH /api/list
 */
 
static int ra_http_patch_list(struct http_xfer *req,struct http_xfer *rsp) {
  const char *src=0;
  int srcc=http_xfer_get_body(&src,req);
  if (srcc<0) return -1;
  uint32_t listid=ra_http_extract_json_id(src,srcc,"listid",6);
  if (!listid) return http_xfer_set_status(rsp,400,"listid required");
  struct db_list *list=db_list_get_by_id(ra.db,listid);
  if (!list) return http_xfer_set_status(rsp,404,"List %d not found",listid);
  if (db_list_decode(list,ra.db,DB_FORMAT_json,src,srcc)<0) return -1;
  return db_list_encode(http_xfer_get_body_encoder(rsp),ra.db,list,DB_FORMAT_json,ra_http_get_detail(req,DB_DETAIL_id));
}

/* DELETE /api/list
 */
 
static int ra_http_delete_list(struct http_xfer *req,struct http_xfer *rsp) {
  int listid;
  if (http_xfer_get_query_int(&listid,req,"listid",6)<0) return http_xfer_set_status(rsp,400,"listid required");
  return db_list_delete(ra.db,listid);
}

/* POST /api/list/add
 */

static int ra_http_list_add(struct http_xfer *req,struct http_xfer *rsp) {
  int listid,gameid;
  if (http_xfer_get_query_int(&listid,req,"listid",6)<0) return http_xfer_set_status(rsp,400,"listid required");
  if (http_xfer_get_query_int(&gameid,req,"gameid",6)<0) return http_xfer_set_status(rsp,400,"gameid required");
  if (!db_game_get_by_id(ra.db,gameid)) return http_xfer_set_status(rsp,404,"Game %d not found",gameid);
  struct db_list *list=db_list_get_by_id(ra.db,listid);
  if (!list) return http_xfer_set_status(rsp,404,"List %d not found",listid);
  if (db_list_append(ra.db,list,gameid)<0) return http_xfer_set_status(rsp,500,"Failed to add game to list");
  return db_list_encode(http_xfer_get_body_encoder(rsp),ra.db,list,DB_FORMAT_json,ra_http_get_detail(req,DB_DETAIL_id));
}

/* POST /api/list/remove
 */
 
static int ra_http_list_remove(struct http_xfer *req,struct http_xfer *rsp) {
  int listid,gameid;
  if (http_xfer_get_query_int(&listid,req,"listid",6)<0) return http_xfer_set_status(rsp,400,"listid required");
  if (http_xfer_get_query_int(&gameid,req,"gameid",6)<0) return http_xfer_set_status(rsp,400,"gameid required");
  struct db_list *list=db_list_get_by_id(ra.db,listid);
  if (!list) return http_xfer_set_status(rsp,404,"List %d not found",listid);
  db_list_remove(ra.db,list,gameid);
  return db_list_encode(http_xfer_get_body_encoder(rsp),ra.db,list,DB_FORMAT_json,ra_http_get_detail(req,DB_DETAIL_id));
}

/* GET /api/blob/all
 * GET /api/blob
 */
 
static int ra_http_list_blobs_cb(uint32_t gameid,const char *type,int typec,const char *time,int timec,const char *path,void *userdata) {
  if (sr_encode_json_string(userdata,0,0,path,-1)<0) return -1;
  return 0;
}
 
static int ra_http_all_blob(struct http_xfer *req,struct http_xfer *rsp) {
  struct sr_encoder *dst=http_xfer_get_body_encoder(rsp);
  int jsonctx=sr_encode_json_array_start(dst,0,0);
  if (jsonctx<0) return -1;
  if (db_blob_for_each(ra.db,1,ra_http_list_blobs_cb,dst)<0) return -1;
  return sr_encode_json_array_end(dst,jsonctx);
}

static int ra_http_get_blob(struct http_xfer *req,struct http_xfer *rsp) {
  struct sr_encoder *dst=http_xfer_get_body_encoder(rsp);
  
  int gameid;
  if (http_xfer_get_query_int(&gameid,req,"gameid",6)>=0) {
    int jsonctx=sr_encode_json_array_start(dst,0,0);
    if (jsonctx<0) return -1;
    if (db_blob_for_gameid(ra.db,gameid,1,ra_http_list_blobs_cb,dst)<0) return -1;
    return sr_encode_json_array_end(dst,jsonctx);
  }
  
  char path[1024];
  int pathc=http_xfer_get_query_string(path,sizeof(path),req,"path",4);
  if ((pathc>0)&&(pathc<sizeof(path))) {
    if (db_blob_validate_path(ra.db,path,pathc)<0) return http_xfer_set_status(rsp,404,"Not found");
    void *serial=0;
    int serialc=file_read(&serial,path);
    if (serialc<0) return http_xfer_set_status(rsp,404,"Not found");
    int err=sr_encode_raw(dst,serial,serialc);
    if (err>=0) http_xfer_set_header(rsp,"Content-Type",12,ra_http_guess_content_type(serial,serialc,path),-1);
    free(serial);
    return err;
  }
  
  return http_xfer_set_status(rsp,400,"Expected 'gameid' or 'path'");
}

/* PUT /api/blob
 */
 
static int ra_http_put_blob(struct http_xfer *req,struct http_xfer *rsp) {
  int gameid;
  if (http_xfer_get_query_int(&gameid,req,"gameid",6)<0) return http_xfer_set_status(rsp,400,"gameid required");
  if (!db_game_get_by_id(ra.db,gameid)) return http_xfer_set_status(rsp,404,"Game %d not found",gameid);
  const void *serial=0;
  int serialc=http_xfer_get_body(&serial,req);
  if (serialc<0) return -1;
  char type[32];
  int typec=http_xfer_get_query_string(type,sizeof(type),req,"type",4);
  if ((typec<0)||(typec>sizeof(type))) typec=0;
  char sfx[16];
  int sfxc=http_xfer_get_query_string(sfx,sizeof(sfx),req,"sfx",3);
  if ((sfxc<0)||(sfxc>sizeof(sfx))) sfxc=0;
  char *path=db_blob_compose_path(ra.db,gameid,type,typec,sfx,sfxc);
  if (!path) return http_xfer_set_status(rsp,500,"Error composing blob path");
  int err=file_write(path,serial,serialc);
  if (err<0) {
    free(path);
    return -1;
  }
  err=sr_encode_json_string(http_xfer_get_body_encoder(rsp),0,0,path,-1);
  free(path);
  return err;
}

/* DELETE /api/blob
 */
 
static int ra_http_delete_blob(struct http_xfer *req,struct http_xfer *rsp) {
  char path[1024];
  int pathc=http_xfer_get_query_string(path,sizeof(path),req,"path",4);
  if ((pathc<1)||(pathc>=sizeof(path))) return http_xfer_set_status(rsp,404,"Not found");
  if (db_blob_validate_path(ra.db,path,pathc)<0) return http_xfer_set_status(rsp,404,"Not found");
  if (file_unlink(path)<0) return http_xfer_set_status(rsp,404,"Not found");
  return 0;
}

/* POST /api/query
 */
 
static int ra_http_query(struct http_xfer *req,struct http_xfer *rsp) {
  struct db_query *query=db_query_new(ra.db);
  if (!query) return -1;
  if (http_xfer_for_query(req,db_query_add_parameter,query)<0) {
    db_query_del(query);
    return -1;
  }
  if (db_query_finish(http_xfer_get_body_encoder(rsp),query)<0) {
    db_query_del(query);
    return -1;
  }
  int pagec=db_query_get_page_count(query);
  if (pagec) {
    char text[16];
    int textc=sr_decsint_repr(text,sizeof(text),pagec);
    if ((textc>0)&&(textc<=sizeof(text))) http_xfer_set_header(rsp,"X-Page-Count",12,text,textc);
  }
  db_query_del(query);
  return 0;
}

/* POST /api/launch
 */
 
static int ra_http_launch(struct http_xfer *req,struct http_xfer *rsp) {
  int gameid;
  if (http_xfer_get_query_int(&gameid,req,"gameid",6)<0) return http_xfer_set_status(rsp,400,"gameid required");
  const struct db_game *game=db_game_get_by_id(ra.db,gameid);
  if (!game) return http_xfer_set_status(rsp,404,"Not found");
  const struct db_launcher *launcher=db_launcher_for_gameid(ra.db,gameid);
  if (!launcher) return http_xfer_set_status(rsp,500,"No suitable launcher");
  
  fprintf(stderr,"%s:%d: TODO! Launch game %d with launcher %d.\n",__FILE__,__LINE__,gameid,launcher->launcherid);
  
  struct db_play playref={
    .gameid=gameid,
    .time=db_time_now(),
  };
  const struct db_play *play=db_play_insert(ra.db,&playref);
  if (!play) play=&playref;
  return db_play_encode(http_xfer_get_body_encoder(rsp),ra.db,play,DB_FORMAT_json,DB_DETAIL_record);
}

/* POST /api/terminate
 */
 
static int ra_http_terminate(struct http_xfer *req,struct http_xfer *rsp) {
  //TODO Terminate running game, and db_play_finish
  return http_xfer_set_status(rsp,500,"TODO %s",__func__);//TODO
}

/* Log call. We only log after the fact. Could be a problem if you want to trace servlet failures?
 */
 
static void ra_http_log_response(struct http_xfer *req,struct http_xfer *rsp) {
  const char *method=http_method_repr(http_xfer_get_method(req));
  const char *rpath=0;
  int rpathc=http_xfer_get_path(&rpath,req);
  int status=http_xfer_get_status(rsp);
  const void *dummy=0;
  int reqbodyc=http_xfer_get_body(&dummy,req);
  int rspbodyc=http_xfer_get_body(&dummy,rsp);
  fprintf(stderr,"%s %.*s <= %d b => %d, %d b\n",method,rpathc,rpath,reqbodyc,status,rspbodyc);
}
  
/* REST calls, wrapper.
 * So I don't have to bugger around with Status and Content-Type in each handler.
 */
 
static int ra_http_wrap_call(int (*servlet)(struct http_xfer *req,struct http_xfer *rsp),struct http_xfer *req,struct http_xfer *rsp) {
  if (servlet(req,rsp)<0) {
    if (!http_xfer_get_status(rsp)) http_xfer_set_status(rsp,500,"Unspecified error");
    http_xfer_set_body(rsp,0,0);
  } else {
    if (http_xfer_get_header(0,rsp,"Content-Type",12)<1) {
      http_xfer_set_header(rsp,"Content-Type",12,"application/json",16);
    }
    if (!http_xfer_get_status(rsp)) {
      http_xfer_set_status(rsp,200,"OK");
    }
  }
  ra_http_log_response(req,rsp);
  return 0;
}

/* REST calls, main dispatch.
 */
 
int ra_http_api(struct http_xfer *req,struct http_xfer *rsp,void *userdata) {
  const char *rpath=0;
  int rpathc=http_xfer_get_path(&rpath,req);
  int method=http_xfer_get_method(req);
  
  #define _(m,p,f) if ((method==HTTP_METHOD_##m)&&(rpathc==sizeof(p)-1)&&!memcmp(rpath,p,rpathc)) return ra_http_wrap_call(f,req,rsp);
  
  _(GET,"/api/meta/flags",ra_http_get_flags)
  _(GET,"/api/meta/platform",ra_http_get_platform)
  _(GET,"/api/meta/author",ra_http_get_author)
  _(GET,"/api/meta/genre",ra_http_get_genre)
  
  _(GET,"/api/game/count",ra_http_count_game)
  _(GET,"/api/game",ra_http_get_game)
  _(PUT,"/api/game",ra_http_put_game)
  _(PATCH,"/api/game",ra_http_patch_game)
  _(DELETE,"/api/game",ra_http_delete_game)
  
  _(GET,"/api/comment/count",ra_http_count_comment)
  _(GET,"/api/comment",ra_http_get_comment)
  _(PUT,"/api/comment",ra_http_put_comment)
  _(PATCH,"/api/comment",ra_http_patch_comment)
  _(DELETE,"/api/comment",ra_http_delete_comment)
  
  _(GET,"/api/play/count",ra_http_count_play)
  _(GET,"/api/play",ra_http_get_play)
  _(PUT,"/api/play",ra_http_put_play)
  _(PATCH,"/api/play",ra_http_patch_play)
  _(DELETE,"/api/play",ra_http_delete_play)
  _(POST,"/api/play/terminate",ra_http_terminate_play)
  
  _(GET,"/api/launcher/count",ra_http_count_launcher)
  _(GET,"/api/launcher",ra_http_get_launcher)
  _(PUT,"/api/launcher",ra_http_put_launcher)
  _(PATCH,"/api/launcher",ra_http_patch_launcher)
  _(DELETE,"/api/launcher",ra_http_delete_launcher)
  
  _(GET,"/api/list/count",ra_http_count_list)
  _(GET,"/api/list",ra_http_get_list)
  _(PUT,"/api/list",ra_http_put_list)
  _(PATCH,"/api/list",ra_http_patch_list)
  _(DELETE,"/api/list",ra_http_delete_list)
  _(POST,"/api/list/add",ra_http_list_add)
  _(POST,"/api/list/remove",ra_http_list_remove)
  
  _(GET,"/api/blob/all",ra_http_all_blob)
  _(GET,"/api/blob",ra_http_get_blob)
  _(PUT,"/api/blob",ra_http_put_blob)
  _(DELETE,"/api/blob",ra_http_delete_blob)
  
  _(POST,"/api/query",ra_http_query)
  _(POST,"/api/launch",ra_http_launch)
  _(POST,"/api/terminate",ra_http_terminate)
  
  #undef _
  return http_xfer_set_status(rsp,404,"Not found");
}
