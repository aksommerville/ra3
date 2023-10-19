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

/* /api/meta/**, dispatch.
 */
 
static int ra_http_api_meta(struct http_xfer *req,struct http_xfer *rsp,void *userdata) {
  if (http_xfer_get_method(req)!=HTTP_METHOD_GET) return http_xfer_set_status(rsp,405,"Method not supported");
  const char *rpath=0;
  int rpathc=http_xfer_get_path(&rpath,req);
  struct sr_encoder encoder={0};
  struct db_histogram histogram={0};
  int err=0;
  int detail=ra_http_get_detail(req,DB_DETAIL_id);
  
  if ((rpathc==15)&&!memcmp(rpath,"/api/meta/flags",15)) {
    sr_encode_json_array_start(&encoder,0,0);
    uint32_t bit=1;
    for (;bit;bit<<=1) {
      sr_encode_json_string(&encoder,0,0,db_flag_repr(bit),-1);
    }
    err=sr_encode_json_array_end(&encoder,0);
    
  } else if ((rpathc==18)&&!memcmp(rpath,"/api/meta/platform",18)) {
    db_histogram_platform(&histogram,ra.db);
    err=db_histogram_encode(&encoder,ra.db,&histogram,DB_FORMAT_json,detail);
    
  } else if ((rpathc==16)&&!memcmp(rpath,"/api/meta/author",16)) {
    db_histogram_author(&histogram,ra.db);
    err=db_histogram_encode(&encoder,ra.db,&histogram,DB_FORMAT_json,detail);
    
  } else if ((rpathc==15)&&!memcmp(rpath,"/api/meta/genre",15)) {
    db_histogram_genre(&histogram,ra.db);
    err=db_histogram_encode(&encoder,ra.db,&histogram,DB_FORMAT_json,detail);
    
  } else {
    return http_xfer_set_status(rsp,404,"Not found.");
  }
  
  db_histogram_cleanup(&histogram);
  if (err<0) {
    sr_encoder_cleanup(&encoder);
    return http_xfer_set_status(rsp,500,"Error encoding metadata");
  }
  http_xfer_set_body(rsp,encoder.v,encoder.c);
  sr_encoder_cleanup(&encoder);
  http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
  return http_xfer_set_status(rsp,200,"OK");
}

/* /api/game/**, dispatch.
 */
 
static int ra_http_api_game(struct http_xfer *req,struct http_xfer *rsp,void *userdata) {

  const char *rpath=0;
  int rpathc=http_xfer_get_path(&rpath,req);
  int method=http_xfer_get_method(req);
  
  if ((method==HTTP_METHOD_GET)&&(rpathc==15)&&!memcmp(rpath,"/api/game/count",15)) {
    char serial[16];
    int serialc=sr_decsint_repr(serial,sizeof(serial),db_game_count(ra.db));
    http_xfer_set_body(rsp,serial,serialc);
    http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
    return http_xfer_set_status(rsp,200,"OK");
  }
  if ((rpathc!=9)||memcmp(rpath,"/api/game",9)) return http_xfer_set_status(rsp,404,"Not found.");
  int detail=ra_http_get_detail(req,DB_DETAIL_record);
  switch (method) {
  
    case HTTP_METHOD_GET: {
        int index=0;
        if (http_xfer_get_query_int(&index,req,"index",5)>=0) {
          int count=1;
          http_xfer_get_query_int(&count,req,"count",5);
          char sep='[';
          struct sr_encoder encoder={0};
          for (;count-->0;index++) {
            struct db_game *game=db_game_get_by_index(ra.db,index);
            if (!game) break;
            http_xfer_append_body(rsp,&sep,1); sep=',';
            encoder.c=0;
            db_game_encode(&encoder,ra.db,game,DB_FORMAT_json,detail);
            http_xfer_append_body(rsp,encoder.v,encoder.c);
          }
          sr_encoder_cleanup(&encoder);
          if (sep=='[') http_xfer_append_body(rsp,"[]",2);
          else http_xfer_append_body(rsp,"]",1);
          http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
          return http_xfer_set_status(rsp,200,"OK");
        }
        int gameid;
        if (http_xfer_get_query_int(&gameid,req,"id",2)>=0) {
          struct db_game *game=db_game_get_by_id(ra.db,gameid);
          if (!game) return http_xfer_set_status(rsp,404,"Not found.");
          struct sr_encoder encoder={0};
          db_game_encode(&encoder,ra.db,game,DB_FORMAT_json,detail);
          http_xfer_set_body(rsp,encoder.v,encoder.c);
          sr_encoder_cleanup(&encoder);
          http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
          return http_xfer_set_status(rsp,200,"OK");
        }
      } break;
      
    case HTTP_METHOD_PUT: {
        int gameid=0;
        http_xfer_get_query_int(&gameid,req,"id",2);
        struct db_game *game=db_game_get_by_id(ra.db,gameid); // ok if null, in fact that's highly normal.
        const void *serial=0;
        int serialc=http_xfer_get_body(&serial,req);
        if (!(game=db_game_patch_json(ra.db,game,serial,serialc))) {
          return http_xfer_set_status(rsp,500,"Failed to update game");
        }
        struct sr_encoder encoder={0};
        db_game_encode(&encoder,ra.db,game,DB_FORMAT_json,detail);
        http_xfer_set_body(rsp,encoder.v,encoder.c);
        sr_encoder_cleanup(&encoder);
        http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
        return http_xfer_set_status(rsp,200,"OK");
      }
      
    case HTTP_METHOD_PATCH: {
        int gameid=0;
        http_xfer_get_query_int(&gameid,req,"id",2);
        struct db_game *game=db_game_get_by_id(ra.db,gameid);
        if (!game) return http_xfer_set_status(rsp,404,"Not found.");
        const void *serial=0;
        int serialc=http_xfer_get_body(&serial,req);
        if (!(game=db_game_patch_json(ra.db,game,serial,serialc))) {
          return http_xfer_set_status(rsp,500,"Failed to update game");
        }
        struct sr_encoder encoder={0};
        db_game_encode(&encoder,ra.db,game,DB_FORMAT_json,detail);
        http_xfer_set_body(rsp,encoder.v,encoder.c);
        sr_encoder_cleanup(&encoder);
        http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
        return http_xfer_set_status(rsp,200,"OK");
      }
      
    case HTTP_METHOD_DELETE: {
        int gameid=0;
        if (http_xfer_get_query_int(&gameid,req,"id",2)<0) return http_xfer_set_status(rsp,400,"Invalid gameid");
        if (db_game_delete(ra.db,gameid)<0) return http_xfer_set_status(rsp,500,"Failed to delete game");
        return http_xfer_set_status(rsp,200,"OK");
      }
  }
  return http_xfer_set_status(rsp,404,"Not found.");
}

/* /api/comment/**, dispatch.
 */
 
static int ra_http_api_comment(struct http_xfer *req,struct http_xfer *rsp,void *userdata) {

  const char *rpath=0;
  int rpathc=http_xfer_get_path(&rpath,req);
  int method=http_xfer_get_method(req);
  
  if ((method==HTTP_METHOD_GET)&&(rpathc==18)&&!memcmp(rpath,"/api/comment/count",18)) {
    char serial[16];
    int serialc=sr_decsint_repr(serial,sizeof(serial),db_comment_count(ra.db));
    http_xfer_set_body(rsp,serial,serialc);
    http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
    return http_xfer_set_status(rsp,200,"OK");
  }
  
  if ((rpathc!=12)||memcmp(rpath,"/api/comment",12)) return http_xfer_set_status(rsp,404,"Not found.");
  switch (method) {
  
    case HTTP_METHOD_GET: {
        int index;
        if (http_xfer_get_query_int(&index,req,"index",5)>=0) {
          int count=1;
          http_xfer_get_query_int(&count,req,"count",5);
          char sep='[';
          for (;count-->0;index++) {
            const struct db_comment *comment=db_comment_get_by_index(ra.db,index);
            if (!comment) break;
            http_xfer_append_body(rsp,&sep,1); sep=',';
            struct sr_encoder encoder={0};
            db_comment_encode(&encoder,ra.db,comment,DB_FORMAT_json);
            http_xfer_append_body(rsp,encoder.v,encoder.c);
            sr_encoder_cleanup(&encoder);
          }
          if (sep=='[') http_xfer_append_body(rsp,"[]",2);
          else http_xfer_append_body(rsp,"]",1);
          http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
          return http_xfer_set_status(rsp,200,"OK");
        }
        int gameid;
        if (http_xfer_get_query_int(&gameid,req,"gameid",6)>=0) {
          char scratch[64];
          int scratchc=http_xfer_get_query_string(scratch,sizeof(scratch),req,"time",4);
          uint32_t time=0;
          if ((scratchc>0)&&(scratchc<=sizeof(scratch))) time=db_time_eval(scratch,scratchc);
          scratchc=http_xfer_get_query_string(scratch,sizeof(scratch),req,"k",1);
          uint32_t k=0;
          if ((scratchc>0)&&(scratchc<=sizeof(scratch))) {
            if (!(k=db_string_lookup(ra.db,scratch,scratchc))) k=UINT32_MAX; // poison if not interned
          }
          char sep='[';
          struct db_comment *comment=0;
          int commentc=db_comments_get_by_gameid(&comment,ra.db,gameid);
          for (;commentc-->0;comment++) {
            if (time&&(comment->time!=time)) continue;
            if (k&&(comment->k!=k)) continue;
            http_xfer_append_body(rsp,&sep,1); sep=',';
            struct sr_encoder encoder={0};
            db_comment_encode(&encoder,ra.db,comment,DB_FORMAT_json);
            http_xfer_append_body(rsp,encoder.v,encoder.c);
            sr_encoder_cleanup(&encoder);
          }
          if (sep=='[') http_xfer_append_body(rsp,"[]",2);
          else http_xfer_append_body(rsp,"]",1);
          http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
          return http_xfer_set_status(rsp,200,"OK");
        }
      } break;
      
    case HTTP_METHOD_PUT: {
        int gameid=0;
        http_xfer_get_query_int(&gameid,req,"gameid",6);
        if (!db_game_get_by_id(ra.db,gameid)) return http_xfer_set_status(rsp,404,"Game %d not found",gameid);
        char tmp[1024];
        int tmpc=http_xfer_get_query_string(tmp,sizeof(tmp),req,"k",1);
        if ((tmpc<1)||(tmpc>sizeof(tmp))) return http_xfer_set_status(rsp,400,"Invalid k");
        uint32_t k=db_string_intern(ra.db,tmp,tmpc);
        if (!k) return http_xfer_set_status(rsp,500,"Failed to intern string");
        tmpc=http_xfer_get_query_string(tmp,sizeof(tmp),req,"v",1);
        if ((tmpc<1)||(tmpc>sizeof(tmp))) return http_xfer_set_status(rsp,400,"Invalid v");
        uint32_t v=db_string_intern(ra.db,tmp,tmpc);
        struct db_comment *comment=db_comment_insert(ra.db,gameid,0,k);
        if (!comment) return http_xfer_set_status(rsp,500,"Failed to create comment");
        comment->v=v;
        struct sr_encoder encoder={0};
        db_comment_encode(&encoder,ra.db,comment,DB_FORMAT_json);
        http_xfer_set_body(rsp,encoder.v,encoder.c);
        sr_encoder_cleanup(&encoder);
        http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
        return http_xfer_set_status(rsp,200,"OK");
      }
      
    case HTTP_METHOD_PATCH: {
        int gameid=0;
        http_xfer_get_query_int(&gameid,req,"gameid",6);
        if (!db_game_get_by_id(ra.db,gameid)) return http_xfer_set_status(rsp,404,"Game %d not found",gameid);
        char tmp[1024];
        int tmpc=http_xfer_get_query_string(tmp,sizeof(tmp),req,"k",1);
        if ((tmpc<1)||(tmpc>sizeof(tmp))) return http_xfer_set_status(rsp,400,"Invalid k");
        uint32_t k=db_string_lookup(ra.db,tmp,tmpc);
        if (!k) return http_xfer_set_status(rsp,500,"Failed to intern string");
        tmpc=http_xfer_get_query_string(tmp,sizeof(tmp),req,"time",4);
        if ((tmpc<1)||(tmpc>sizeof(tmp))) return http_xfer_set_status(rsp,400,"Invalid time");
        uint32_t time=db_time_eval(tmp,tmpc);
        tmpc=http_xfer_get_query_string(tmp,sizeof(tmp),req,"v",1);
        if ((tmpc<1)||(tmpc>sizeof(tmp))) return http_xfer_set_status(rsp,400,"Invalid v");
        uint32_t v=db_string_intern(ra.db,tmp,tmpc);
        
        struct db_comment *comment=0;
        int commentc=db_comments_get_by_gameid(&comment,ra.db,gameid);
        for (;commentc-->0;comment++) {
          if (comment->time!=time) continue;
          if (comment->k!=k) continue;
          comment->v=v;
          db_comment_dirty(ra.db,comment);
          struct sr_encoder encoder={0};
          db_comment_encode(&encoder,ra.db,comment,DB_FORMAT_json);
          http_xfer_set_body(rsp,encoder.v,encoder.c);
          sr_encoder_cleanup(&encoder);
          http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
          return http_xfer_set_status(rsp,200,"OK");
        }
        return http_xfer_set_status(rsp,404,"Comment not found");
      }
      
    case HTTP_METHOD_DELETE: {
        int gameid=0;
        http_xfer_get_query_int(&gameid,req,"gameid",6);
        if (!db_game_get_by_id(ra.db,gameid)) return http_xfer_set_status(rsp,404,"Game %d not found",gameid);
        char tmp[64];
        int tmpc=http_xfer_get_query_string(tmp,sizeof(tmp),req,"k",1);
        if ((tmpc<1)||(tmpc>sizeof(tmp))) return http_xfer_set_status(rsp,400,"Invalid k");
        uint32_t k=db_string_lookup(ra.db,tmp,tmpc);
        if (!k) return http_xfer_set_status(rsp,500,"Failed to intern string");
        tmpc=http_xfer_get_query_string(tmp,sizeof(tmp),req,"time",4);
        if ((tmpc<1)||(tmpc>sizeof(tmp))) return http_xfer_set_status(rsp,400,"Invalid time");
        uint32_t time=db_time_eval(tmp,tmpc);
        struct db_comment *comment=0;
        int commentc=db_comments_get_by_gameid(&comment,ra.db,gameid);
        for (;commentc-->0;comment++) {
          if (comment->time!=time) continue;
          if (comment->k!=k) continue;
          db_comment_delete(ra.db,comment);
          return http_xfer_set_status(rsp,200,"OK");
        }
        return http_xfer_set_status(rsp,404,"Comment not found");
      }
  }
  return http_xfer_set_status(rsp,404,"Not found.");
}

/* /api/play/**, dispatch.
 */
 
static int ra_http_api_play(struct http_xfer *req,struct http_xfer *rsp,void *userdata) {
  
  const char *rpath=0;
  int rpathc=http_xfer_get_path(&rpath,req);
  int method=http_xfer_get_method(req);
  
  if ((method==HTTP_METHOD_GET)&&(rpathc==15)&&!memcmp(rpath,"/api/play/count",15)) {
    char serial[16];
    int serialc=sr_decsint_repr(serial,sizeof(serial),db_play_count(ra.db));
    http_xfer_set_body(rsp,serial,serialc);
    http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
    return http_xfer_set_status(rsp,200,"OK");
  }
  
  if ((method==HTTP_METHOD_POST)&&(rpathc==19)&&!memcmp(rpath,"/api/play/terminate",19)) {
    int gameid=0;
    if (http_xfer_get_query_int(&gameid,req,"gameid",6)<0) return http_xfer_set_status(rsp,400,"Invalid gameid");
    struct db_play *play=db_play_finish(ra.db,gameid);
    if (!play) return http_xfer_set_status(rsp,400,"Game not found, or not in progress");
    struct sr_encoder encoder={0};
    db_play_encode(&encoder,ra.db,play,DB_FORMAT_json);
    http_xfer_set_body(rsp,encoder.v,encoder.c);
    sr_encoder_cleanup(&encoder);
    http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
    return http_xfer_set_status(rsp,200,"OK");
  }
  
  if ((rpathc==9)&&!memcmp(rpath,"/api/play",9)) {
    switch (method) {
    
      case HTTP_METHOD_GET: {
          int index;
          if (http_xfer_get_query_int(&index,req,"index",5)>=0) {
            int count=1;
            http_xfer_get_query_int(&count,req,"count",5);
            char sep='[';
            struct sr_encoder encoder={0};
            for (;count-->0;index++) {
              struct db_play *play=db_play_get_by_index(ra.db,index);
              if (!play) break;
              encoder.c=0;
              db_play_encode(&encoder,ra.db,play,DB_FORMAT_json);
              http_xfer_append_body(rsp,&sep,1); sep=',';
              http_xfer_append_body(rsp,encoder.v,encoder.c);
            }
            if (sep=='[') http_xfer_append_body(rsp,"[]",2);
            else http_xfer_append_body(rsp,"]",1);
            sr_encoder_cleanup(&encoder);
            http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
            return http_xfer_set_status(rsp,200,"OK");
          }
          int gameid;
          if (http_xfer_get_query_int(&gameid,req,"gameid",6)>=0) {
            uint32_t starttime=0;
            char since[32];
            int sincec=http_xfer_get_query_string(since,sizeof(since),req,"since",5);
            if ((sincec>0)&&(sincec<=sizeof(since))) starttime=db_time_eval(since,sincec);
            struct db_play *play=0;
            int playc=db_plays_get_by_gameid(&play,ra.db,gameid);
            char sep='[';
            struct sr_encoder encoder={0};
            for (;playc-->0;play++) {
              if (play->time<starttime) continue;
              encoder.c=0;
              db_play_encode(&encoder,ra.db,play,DB_FORMAT_json);
              http_xfer_append_body(rsp,&sep,1); sep=',';
              http_xfer_append_body(rsp,encoder.v,encoder.c);
            }
            if (sep=='[') http_xfer_append_body(rsp,"[]",2);
            else http_xfer_append_body(rsp,"]",1);
            sr_encoder_cleanup(&encoder);
            http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
            return http_xfer_set_status(rsp,200,"OK");
          }
        } break;
        
      case HTTP_METHOD_PUT: {
          int gameid;
          if (http_xfer_get_query_int(&gameid,req,"gameid",6)<0) return http_xfer_set_status(rsp,400,"Invalid gameid");
          if (!db_game_get_by_id(ra.db,gameid)) return http_xfer_set_status(rsp,404,"Game %d not found",gameid);
          int dur=0;
          http_xfer_get_query_int(&dur,req,"dur",3);
          struct db_play *play=db_play_insert(ra.db,gameid);
          if (!play) return http_xfer_set_status(rsp,500,"Failed to create play record");
          play->dur_m=dur;
          struct sr_encoder encoder={0};
          db_play_encode(&encoder,ra.db,play,DB_FORMAT_json);
          http_xfer_append_body(rsp,encoder.v,encoder.c);
          sr_encoder_cleanup(&encoder);
          http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
          return http_xfer_set_status(rsp,200,"OK");
        }
        
      case HTTP_METHOD_PATCH: {
          int gameid,dur;
          if (
            (http_xfer_get_query_int(&gameid,req,"gameid",6)<0)||
            (http_xfer_get_query_int(&dur,req,"dur",3)<0)
          ) return http_xfer_set_status(rsp,400,"Expected gameid and dur");
          uint32_t time=0;
          char timestr[32];
          int timestrc=http_xfer_get_query_string(timestr,sizeof(timestr),req,"time",4);
          if ((timestrc<1)||(timestrc>sizeof(timestr))||!(time=db_time_eval(timestr,timestrc))) {
            return http_xfer_set_status(rsp,400,"Invalid time");
          }
          struct db_play *play=db_play_get_by_gameid_time(ra.db,gameid,time);
          if (!play) return http_xfer_set_status(rsp,404,"Play not found for gameid=%d,time=%d",gameid,time);
          db_play_set_dur_m(ra.db,play,dur);
          struct sr_encoder encoder={0};
          db_play_encode(&encoder,ra.db,play,DB_FORMAT_json);
          http_xfer_append_body(rsp,encoder.v,encoder.c);
          sr_encoder_cleanup(&encoder);
          http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
          return http_xfer_set_status(rsp,200,"OK");
        }
        
      case HTTP_METHOD_DELETE: {
          int gameid;
          if (http_xfer_get_query_int(&gameid,req,"gameid",6)<0) return http_xfer_set_status(rsp,400,"Invalid gameid");
          uint32_t time=0;
          char timestr[32];
          int timestrc=http_xfer_get_query_string(timestr,sizeof(timestr),req,"time",4);
          if ((timestrc<1)||(timestrc>sizeof(timestr))||!(time=db_time_eval(timestr,timestrc))) {
            return http_xfer_set_status(rsp,400,"Invalid time");
          }
          if (db_play_delete(ra.db,gameid,time)<0) return http_xfer_set_status(rsp,500,"Failed to delete");
          return http_xfer_set_status(rsp,200,"OK");
        }
    }
  }
  return http_xfer_set_status(rsp,404,"Not found.");
}

/* /api/launcher/**, dispatch.
 */
 
static uint32_t ra_http_extract_launcherid(const char *src,int srcc) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_object_start(&decoder)<0) return 0;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    if ((kc==9)&&!memcmp(k,"launcherid",10)) {
      int id=0;
      if (sr_decode_json_int(&id,&decoder)<0) return -1;
      return id;
    }
    sr_decode_json_skip(&decoder);
  }
  return 0;
}
 
static int ra_http_api_launcher(struct http_xfer *req,struct http_xfer *rsp,void *userdata) {

  const char *rpath=0;
  int rpathc=http_xfer_get_path(&rpath,req);
  int method=http_xfer_get_method(req);
  
  if ((rpathc==19)&&!memcmp(rpath,"/api/launcher/count",19)&&(method==HTTP_METHOD_GET)) {
    char dst[16];
    int dstc=sr_decsint_repr(dst,sizeof(dst),db_launcher_count(ra.db));
    if ((dstc<0)||(dstc>sizeof(dst))) return -1;
    http_xfer_set_body(rsp,dst,dstc);
    http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
    return http_xfer_set_status(rsp,200,"OK");
  }
  
  // Everything else the path is "/api/launcher".
  if ((rpathc!=13)||memcmp(rpath,"/api/launcher",13)) return http_xfer_set_status(rsp,404,"Not found.");
  switch (method) {
  
    case HTTP_METHOD_GET: {
        int index,id;
        if (http_xfer_get_query_int(&index,req,"index",5)>=0) { // Multiple launchers by index.
          struct sr_encoder encoder={0};
          int count=1;
          http_xfer_get_query_int(&count,req,"count",5);
          char sep='[';
          for (;count-->0;index++) {
            struct db_launcher *launcher=db_launcher_get_by_index(ra.db,index);
            if (!launcher) break;
            http_xfer_append_body(rsp,&sep,1); sep=',';
            encoder.c=0;
            db_launcher_encode(&encoder,ra.db,launcher,DB_FORMAT_json);
            http_xfer_append_body(rsp,encoder.v,encoder.c);
          }
          sr_encoder_cleanup(&encoder);
          if (sep=='[') http_xfer_append_body(rsp,"[]",2);
          else http_xfer_append_body(rsp,"]",1);
          http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
          return http_xfer_set_status(rsp,200,"OK");
        }
        if (http_xfer_get_query_int(&id,req,"launcherid",10)>=0) { // Single launcher by ID.
          struct db_launcher *launcher=db_launcher_get_by_id(ra.db,id);
          if (!launcher) return http_xfer_set_status(rsp,404,"Launcher %d not found",id);
          struct sr_encoder encoder={0};
          db_launcher_encode(&encoder,ra.db,launcher,DB_FORMAT_json);
          http_xfer_set_body(rsp,encoder.v,encoder.c);
          sr_encoder_cleanup(&encoder);
          http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
          return http_xfer_set_status(rsp,200,"OK");
        }
        if (http_xfer_get_query_int(&id,req,"gameid",6)>=0) { // Single launcher by Game ID.
          struct db_launcher *launcher=db_launcher_for_gameid(ra.db,id);
          if (!launcher) return http_xfer_set_status(rsp,404,"Launcher for game %d not found",id);
          struct sr_encoder encoder={0};
          db_launcher_encode(&encoder,ra.db,launcher,DB_FORMAT_json);
          http_xfer_set_body(rsp,encoder.v,encoder.c);
          sr_encoder_cleanup(&encoder);
          http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
          return http_xfer_set_status(rsp,200,"OK");
        }
      } break;
      
    case HTTP_METHOD_PUT: {
        struct db_launcher *launcher=db_launcher_insert(ra.db,0);
        if (!launcher) return http_xfer_set_status(rsp,500,"Failed to create launcher");
        const void *serial=0;
        int serialc=http_xfer_get_body(&serial,req);
        if (db_launcher_patch_json(ra.db,launcher,serial,serialc)<0) {
          db_launcher_delete(ra.db,launcher->launcherid);
          return http_xfer_set_status(rsp,400,"Failed to apply launcher JSON");
        }
        struct sr_encoder encoder={0};
        db_launcher_encode(&encoder,ra.db,launcher,DB_FORMAT_json);
        http_xfer_set_body(rsp,encoder.v,encoder.c);
        sr_encoder_cleanup(&encoder);
        http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
        return http_xfer_set_status(rsp,200,"OK");
      }
      
    case HTTP_METHOD_PATCH: {
        const void *serial=0;
        int serialc=http_xfer_get_body(&serial,req);
        uint32_t launcherid=ra_http_extract_launcherid(serial,serialc);
        if (!launcherid) return http_xfer_set_status(rsp,400,"Invalid launcherid");
        struct db_launcher *launcher=db_launcher_get_by_id(ra.db,launcherid);
        if (!launcher) return http_xfer_set_status(rsp,404,"Launcher %d not found",launcherid);
        if (db_launcher_patch_json(ra.db,launcher,serial,serialc)<0) {
          return http_xfer_set_status(rsp,400,"Failed to apply launcher JSON");
        }
        struct sr_encoder encoder={0};
        db_launcher_encode(&encoder,ra.db,launcher,DB_FORMAT_json);
        http_xfer_set_body(rsp,encoder.v,encoder.c);
        sr_encoder_cleanup(&encoder);
        http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
        return http_xfer_set_status(rsp,200,"OK");
      }
      
    case HTTP_METHOD_DELETE: {
        int launcherid;
        if (http_xfer_get_query_int(&launcherid,req,"launcherid",10)<0) {
          return http_xfer_set_status(rsp,400,"Invalid launcherid");
        }
        if (db_launcher_delete(ra.db,launcherid)<0) return http_xfer_set_status(rsp,500,"Failed to delete launcher");
        return http_xfer_set_status(rsp,200,"OK");
      }
      
  }
  return http_xfer_set_status(rsp,404,"Not found.");
}

/* /api/list/**
 */
 
static int ra_http_list_respond_single(struct http_xfer *req,struct http_xfer *rsp,struct db_list *list) {
  int detail=ra_http_get_detail(req,DB_DETAIL_id);
  struct sr_encoder encoder={0};
  if (db_list_encode(&encoder,ra.db,list,DB_FORMAT_json,detail)<0) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  http_xfer_set_body(rsp,encoder.v,encoder.c);
  sr_encoder_cleanup(&encoder);
  http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
  return http_xfer_set_status(rsp,200,"OK");
}
 
static int ra_http_api_list(struct http_xfer *req,struct http_xfer *rsp,void *userdata) {
  const char *rpath=0;
  int rpathc=http_xfer_get_path(&rpath,req);
  int method=http_xfer_get_method(req);
  
  if ((rpathc==9)&&!memcmp(rpath,"/api/list",9)) {
    switch (method) {
    
      case HTTP_METHOD_GET: {
          int index,count=1,listid,gameid;
          int detail=ra_http_get_detail(req,DB_DETAIL_id);
          
          // Multiple lists by index:
          if (http_xfer_get_query_int(&index,req,"index",5)>=0) {
            http_xfer_get_query_int(&count,req,"count",5);
            char sep='[';
            for (;count-->0;index++) {
              struct db_list *list=db_list_get_by_index(ra.db,index);
              if (!list) break;
              struct sr_encoder encoder={0};
              db_list_encode(&encoder,ra.db,list,DB_FORMAT_json,detail);
              http_xfer_append_body(rsp,&sep,1); sep=',';
              http_xfer_append_body(rsp,encoder.v,encoder.c);
              sr_encoder_cleanup(&encoder);
            }
            if (sep=='[') http_xfer_append_body(rsp,"[]",2);
            else http_xfer_append_body(rsp,"]",1);
            http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
            return http_xfer_set_status(rsp,200,"OK");
          }
          
          // Single list by ID:
          if (http_xfer_get_query_int(&listid,req,"listid",6)>=0) {
            struct db_list *list=db_list_get_by_id(ra.db,listid);
            if (!list) return http_xfer_set_status(rsp,404,"List not found");
            return ra_http_list_respond_single(req,rsp,list);
          }
          
          // Multiple lists by gameid:
          if (http_xfer_get_query_int(&gameid,req,"gameid",6)>=0) {
            char sep='[';
            if (db_game_get_by_id(ra.db,gameid)) { // no sense checking the lists if there's no such game
              int index=0;
              for (;;index++) {
                struct db_list *list=db_list_get_by_index(ra.db,index);
                if (!list) break;
                if (!db_list_has(list,gameid)) continue;
                struct sr_encoder encoder={0};
                db_list_encode(&encoder,ra.db,list,DB_FORMAT_json,detail);
                http_xfer_append_body(rsp,&sep,1); sep=',';
                http_xfer_append_body(rsp,encoder.v,encoder.c);
                sr_encoder_cleanup(&encoder);
              }
            }
            if (sep=='[') http_xfer_append_body(rsp,"[]",2);
            else http_xfer_append_body(rsp,"]",1);
            http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
            return http_xfer_set_status(rsp,200,"OK");
          }
          
        } break;
        
      case HTTP_METHOD_PUT: {
          int listid=0;
          http_xfer_get_query_int(&listid,req,"listid",6); // legal but rare
          struct db_list *list=0;
          if (!(list=db_list_insert(ra.db,listid))) return http_xfer_set_status(rsp,400,"List ID in use, or failed to create");
          const char *body=0;
          int bodyc=http_xfer_get_body(&body,req);
          if (bodyc<1) return http_xfer_set_status(rsp,400,"Expected JSON body");
          if (db_list_patch_json(ra.db,list,body,bodyc)<0) {
            return http_xfer_set_status(rsp,500,"Failed to patch list");
          }
          return ra_http_list_respond_single(req,rsp,list);
        }
        
      case HTTP_METHOD_PATCH: {
          int listid=0;
          http_xfer_get_query_int(&listid,req,"listid",6);
          struct db_list *list=db_list_get_by_id(ra.db,listid);
          if (!list) return http_xfer_set_status(rsp,400,"Invalid listid");
          const char *body=0;
          int bodyc=http_xfer_get_body(&body,req);
          if (bodyc<1) return http_xfer_set_status(rsp,400,"Expected JSON body");
          if (db_list_patch_json(ra.db,list,body,bodyc)<0) {
            return http_xfer_set_status(rsp,500,"Failed to patch list");
          }
          return ra_http_list_respond_single(req,rsp,list);
        }
        
      case HTTP_METHOD_DELETE: {
          int listid=0;
          http_xfer_get_query_int(&listid,req,"listid",6);
          if (db_list_delete(ra.db,listid)<0) return http_xfer_set_status(rsp,500,"Failed to delete list");
          return http_xfer_set_status(rsp,200,"OK");
        }
    }
    return http_xfer_set_status(rsp,405,"Method not allowed");
  }
  
  if ((rpathc==15)&&!memcmp(rpath,"/api/list/count",15)&&(method==HTTP_METHOD_GET)) {
    char body[16];
    int bodyc=sr_decsint_repr(body,sizeof(body),db_list_count(ra.db));
    http_xfer_set_body(rsp,body,bodyc);
    http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
    return http_xfer_set_status(rsp,200,"OK");
  }
  
  if ((rpathc==13)&&!memcmp(rpath,"/api/list/add",13)&&(method==HTTP_METHOD_POST)) {
    int listid=0,gameid=0;
    if (http_xfer_get_query_int(&listid,req,"listid",6)<0) return http_xfer_set_status(rsp,400,"Invalid listid");
    if (http_xfer_get_query_int(&gameid,req,"gameid",6)<0) return http_xfer_set_status(rsp,400,"Invalid gameid");
    struct db_list *list=db_list_get_by_id(ra.db,listid);
    if (!list) return http_xfer_set_status(rsp,404,"List %d not found",listid);
    if (!db_game_get_by_id(ra.db,gameid)) return http_xfer_set_status(rsp,404,"Game %d not found",gameid);
    if (db_list_append(ra.db,list,gameid)<0) return http_xfer_set_status(rsp,500,"Failed to add game to list");
    return ra_http_list_respond_single(req,rsp,list);
  }
  
  if ((rpathc==16)&&!memcmp(rpath,"/api/list/remove",16)&&(method==HTTP_METHOD_POST)) {
    int listid=0,gameid=0;
    if (http_xfer_get_query_int(&listid,req,"listid",6)<0) return http_xfer_set_status(rsp,400,"Invalid listid");
    if (http_xfer_get_query_int(&gameid,req,"gameid",6)<0) return http_xfer_set_status(rsp,400,"Invalid gameid");
    struct db_list *list=db_list_get_by_id(ra.db,listid);
    if (!list) return http_xfer_set_status(rsp,404,"List %d not found",listid);
    db_list_remove(ra.db,list,gameid);
    return ra_http_list_respond_single(req,rsp,list);
  }
  
  return http_xfer_set_status(rsp,404,"Not found.");
}

/* /api/blob/**, dispatch.
 */

static int ra_http_api_blob_cb_list(uint32_t gameid,const char *type,int typec,const char *time,int timec,const char *path,void *userdata) {
  struct http_xfer *rsp=userdata;
  if (http_xfer_append_bodyf(rsp,"\"%s\",",path)<0) return -1;
  return 0;
}
 
static int ra_http_api_blob(struct http_xfer *req,struct http_xfer *rsp,void *userdata) {
  int method=http_xfer_get_method(req);
  const char *rpath=0;
  int rpathc=http_xfer_get_path(&rpath,req);
  
  if ((rpathc==13)&&!memcmp(rpath,"/api/blob/all",13)&&(method==HTTP_METHOD_GET)) {
    if (http_xfer_append_body(rsp,"[",1)<0) return -1;
    db_blob_for_each(ra.db,0,ra_http_api_blob_cb_list,rsp);
    if (http_xfer_append_body(rsp,"\"\"]",3)<0) return -1;
    http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
    return http_xfer_set_status(rsp,200,"OK");
  }
  
  if ((rpathc==9)&&!memcmp(rpath,"/api/blob",9)) switch (method) {
  
    case HTTP_METHOD_GET: {
        char bpath[1024];
        int bpathc=http_xfer_get_query_string(bpath,sizeof(bpath),req,"path",4);
        if ((bpathc>0)&&(bpathc<sizeof(bpath))) { // "?path": Return content for one blob.
          if (db_blob_validate_path(ra.db,bpath,bpathc)<0) return http_xfer_set_status(rsp,404,"Not found.");
          void *serial=0;
          int serialc=file_read(&serial,bpath);
          if (serialc<0) return http_xfer_set_status(rsp,404,"Not found.");
          http_xfer_append_body(rsp,serial,serialc);
          http_xfer_set_header(rsp,"Content-Type",12,ra_http_guess_content_type(serial,serialc,bpath),-1);
          free(serial);
          return http_xfer_set_status(rsp,200,"OK");
        }
        int gameid=0;
        if (http_xfer_get_query_int(&gameid,req,"gameid",6)>=0) { // "?gameid": List blobs for one game.
          char type[16];
          int typec=http_xfer_get_query_string(type,sizeof(type),req,"type",4);
          if ((typec<0)||(typec>sizeof(type))) typec=0;
          //TODO Currently extracting (type) and then ignoring it. Current design, it's not useful, but either support it or don't pretend to!
          if (http_xfer_append_body(rsp,"[",1)<0) return -1;
          db_blob_for_gameid(ra.db,gameid,0,ra_http_api_blob_cb_list,rsp);
          if (http_xfer_append_body(rsp,"\"\"]",3)<0) return -1;
          http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
          return http_xfer_set_status(rsp,200,"OK");
        }
      } break;

    case HTTP_METHOD_PUT: {
        int gameid;
        if ((http_xfer_get_query_int(&gameid,req,"gameid",6)<0)||(gameid<1)) {
          return http_xfer_set_status(rsp,400,"Invalid gameid");
        }
        char type[16];
        int typec=http_xfer_get_query_string(type,sizeof(type),req,"type",4);
        if ((typec<1)||(typec>sizeof(type))) {
          return http_xfer_set_status(rsp,400,"Invalid blob type");
        }
        char sfx[16];
        int sfxc=http_xfer_get_query_string(sfx,sizeof(sfx),req,"sfx",3);
        if ((sfxc<0)||(sfxc>sizeof(sfx))) sfxc=0;
        char *bpath=db_blob_compose_path(ra.db,gameid,type,typec,sfx,sfxc);
        if (!bpath) return http_xfer_set_status(rsp,400,"Failed to compose blob path");
        const void *serial=0;
        int serialc=http_xfer_get_body(&serial,req);
        if (serialc<0) serialc=0;
        int err=file_write(bpath,serial,serialc);
        if (err<0) {
          free(bpath);
          return http_xfer_set_status(rsp,500,"Failed to write file, %d bytes",serialc);
        }
        http_xfer_append_body(rsp,"\"",1);
        http_xfer_append_body(rsp,bpath,-1);
        http_xfer_append_body(rsp,"\"",1);
        free(bpath);
        http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
        return http_xfer_set_status(rsp,200,"OK");
      }
      
    case HTTP_METHOD_DELETE: {
        char bpath[1024];
        int bpathc=http_xfer_get_query_string(bpath,sizeof(bpath),req,"path",4);
        if ((bpathc<1)||(bpathc>=sizeof(bpath))) return http_xfer_set_status(rsp,400,"Invalid path");
        if (db_blob_validate_path(ra.db,bpath,bpathc)<0) return http_xfer_set_status(rsp,400,"Invalid path");
        if (file_unlink(bpath)<0) return http_xfer_set_status(rsp,500,"Error unlinking file");
        return http_xfer_set_status(rsp,200,"OK");
      }
  }
  
  return http_xfer_set_status(rsp,404,"Not found.");
}

/* Split a range string "LO..HI".
 * If the elapsis is omitted, we return the same thing for (lo) and (hi).
 * If one endpoint is omitted, we return it empty.
 */
 
static int ra_range_split(void *lopp,int *loc,void *hipp,int *hic,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0;
  const char *lv=src+srcp;
  int lc=0;
  while ((srcp<srcc)&&(src[srcp]!='.')) { srcp++; lc++; }
  if (srcp>=srcc) { // single scalar
    *(const void**)lopp=lv;
    *(const void**)hipp=lv;
    *loc=lc;
    *hic=lc;
    return 0;
  }
  if ((srcp>srcc-2)||(src[srcp]!='.')||(src[srcp+1]!='.')) return -1;
  const char *hv=src+srcp+2;
  int hc=srcc-srcp-2;
  *(const void**)lopp=lv;
  *(const void**)hipp=hv;
  *loc=lc;
  *hic=hc;
  return 0;
}

/* /api/query
 */
 
struct ra_http_query_context {
  struct db_list *list;
  int empty; // nonzero if we can short-circuit, we know the result will be empty.
  char text[128];
  int textc;
  uint32_t platform,author,genre;
  uint32_t flags,notflags;
  uint32_t ratinglo,ratinghi;
  uint32_t pubtimelo,pubtimehi;
  int detail,limit,page,sort;
};

static void ra_http_query_context_cleanup(struct ra_http_query_context *ctx) {
  db_list_del(ctx->list);
}

static int ra_http_api_query_cb(const char *k,int kc,const char *v,int vc,void *userdata) {
  struct ra_http_query_context *ctx=userdata;
  
  // list may appear more than once, and the various lists get ANDed together.
  // If a list is not found, or its length reaches zero, we can short-circuit.
  if ((kc==4)&&!memcmp(k,"list",4)) {
    struct db_list *srclist=db_list_get_by_string(ra.db,v,vc);
    if (!srclist) {
      ctx->empty=1;
      return 1;
    }
    struct db_list *nlist;
    if (ctx->list) nlist=db_query_list_and(ra.db,srclist,ctx->list);
    else nlist=db_list_copy_nonresident(srclist);
    if (!nlist) return -1;
    db_list_del(ctx->list);
    ctx->list=nlist;
    if (!ctx->list->gameidc) {
      ctx->empty=1;
      return 1;
    }
    return 0;
  }
  
  // text may only appear once, and has a length limit.
  if ((kc==4)&&!memcmp(k,"text",4)) {
    if (ctx->textc) return -1;
    if (vc>sizeof(ctx->text)) return -1;
    memcpy(ctx->text,v,vc);
    ctx->textc=vc;
    return 0;
  }
  
  // (platform,author,genre): If lookup fails, we short-circuit.
  if ((kc==8)&&!memcmp(k,"platform",8)) {
    if (ctx->platform) return -1;
    if (!(ctx->platform=db_string_lookup(ra.db,v,vc))) {
      ctx->empty=1;
      return 1;
    }
    return 0;
  }
  if ((kc==6)&&!memcmp(k,"author",6)) {
    if (ctx->author) return -1;
    if (!(ctx->author=db_string_lookup(ra.db,v,vc))) {
      ctx->empty=1;
      return 1;
    }
    return 0;
  }
  if ((kc==5)&&!memcmp(k,"genre",5)) {
    if (ctx->genre) return -1;
    if (!(ctx->genre=db_string_lookup(ra.db,v,vc))) {
      ctx->empty=1;
      return 1;
    }
    return 0;
  }
  
  // (flags,notflags): If they overlap, we short-circuit.
  if ((kc==5)&&!memcmp(k,"flags",5)) {
    if (ctx->flags) return -1;
    if (db_flags_eval(&ctx->flags,v,vc)<0) return -1;
    if (ctx->flags&ctx->notflags) {
      ctx->empty=1;
      return 1;
    }
    return 0;
  }
  if ((kc==8)&&!memcmp(k,"notflags",8)) {
    if (ctx->notflags) return -1;
    if (db_flags_eval(&ctx->notflags,v,vc)<0) return -1;
    if (ctx->flags&ctx->notflags) {
      ctx->empty=1;
      return 1;
    }
    return 0;
  }
  
  // rating can be an explicit range, open range, or exact value.
  if ((kc==6)&&!memcmp(k,"rating",6)) {
    if (ctx->ratinglo||ctx->ratinghi) return -1;
    const char *lo=0,*hi=0;
    int loc=0,hic=0,n;
    if (ra_range_split(&lo,&loc,&hi,&hic,v,vc)<0) return -1;
    if (!loc) ctx->ratinglo=0;
    else if (sr_int_eval(&n,lo,loc)<2) return -1;
    else ctx->ratinglo=n;
    if (!hic) ctx->ratinghi=UINT32_MAX;
    else if (sr_int_eval(&n,hi,hic)<2) return -1;
    else ctx->ratinghi=n;
    if (ctx->ratinglo>ctx->ratinghi) {
      ctx->empty=1;
      return 1;
    }
    return 0;
  }
  
  // pubtime works like rating, with the extra complication that each endpoint can cover some range.
  if ((kc==7)&&!memcmp(k,"pubtime",7)) {
    if (ctx->pubtimelo||ctx->pubtimehi) return -1;
    const char *lo=0,*hi=0;
    int loc=0,hic=0;
    if (ra_range_split(&lo,&loc,&hi,&hic,v,vc)<0) return -1;
    if ((loc==hic)&&!memcmp(lo,hi,loc)) {
      ctx->pubtimelo=db_time_eval(lo,loc);
      ctx->pubtimehi=db_time_eval_upper(lo,loc);
    } else {
      if (loc) ctx->pubtimelo=db_time_eval(lo,loc);
      else ctx->pubtimelo=0;
      if (hic) ctx->pubtimehi=db_time_eval_upper(hi,hic);
      else ctx->pubtimehi=UINT32_MAX;
    }
    if (ctx->pubtimelo>ctx->pubtimehi) {
      ctx->empty=1;
      return 1;
    }
    return 0;
  }
  
  // (detail,limit,page,sort), pretty much anything goes.
  if ((kc==6)&&!memcmp(k,"detail",6)) {
    #define _(tag) if ((vc==sizeof(#tag)-1)&&!memcmp(v,#tag,vc)) { ctx->detail=DB_DETAIL_##tag; return 0; }
    DB_DETAIL_FOR_EACH
    #undef _
    return -1;
  }
  if ((kc==5)&&!memcmp(k,"limit",5)) {
    if (sr_int_eval(&ctx->limit,v,vc)<2) return -1;
    if (ctx->limit<1) return -1;
    return 0;
  }
  if ((kc==4)&&!memcmp(k,"page",4)) {
    if (sr_int_eval(&ctx->page,v,vc)<2) return -1;
    // Tempting to short-circuit if it's negative, but that's incorrect: We still need to determine the page count to return.
    return 0;
  }
  if ((kc==4)&&!memcmp(k,"sort",4)) {
    //TODO Sorting query results.
    return 0;
  }
  
  return 0;
}
 
static int ra_http_api_query(struct http_xfer *req,struct http_xfer *rsp,void *userdata) {
  if (http_xfer_get_method(req)!=HTTP_METHOD_POST) return http_xfer_set_status(rsp,405,"Method not allowed");
  
  struct ra_http_query_context ctx={
    .detail=DB_DETAIL_record,
    .limit=100,
    .page=1,
    .sort=0,//TODO?
  };
  if (http_xfer_for_query(req,ra_http_api_query_cb,&ctx)<0) {
    ra_http_query_context_cleanup(&ctx);
    return http_xfer_set_status(rsp,400,"Failed to compose query.");
  }
  
  /* Easy to propose an impossible request that will always return empty.
   * We detect a lot of these, and avoid hitting the db at all.
   */
  if (ctx.empty) {
    http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
    http_xfer_append_body(rsp,"[]",2);
    ra_http_query_context_cleanup(&ctx);
    return http_xfer_set_status(rsp,200,"OK");
  }
  
  /* If there's any content for the header query, run that first, so the text query has a smaller search space.
   * (and note that we've already done the list queries, during param processing).
   */
  if (
    ctx.platform||ctx.author||ctx.genre||
    ctx.flags||ctx.notflags||
    ctx.ratinglo||ctx.ratinghi||
    ctx.pubtimelo||ctx.pubtimehi
  ) {
    struct db_list *nlist=db_query_header_prelookupped(
      ra.db,ctx.list,
      ctx.platform,ctx.author,ctx.genre,
      ctx.flags,ctx.notflags,
      ctx.ratinglo,ctx.ratinghi,
      ctx.pubtimelo,ctx.pubtimehi
    );
    if (!nlist) {
      ra_http_query_context_cleanup(&ctx);
      return http_xfer_set_status(rsp,500,"Error executing query.");
    }
    db_list_del(ctx.list);
    ctx.list=nlist;
  }
  
  /* Do the expensive text search, if requested.
   */
  if (ctx.textc) {
    struct db_list *nlist=db_query_text(ra.db,ctx.list,ctx.text,ctx.textc);
    if (!nlist) {
      ra_http_query_context_cleanup(&ctx);
      return http_xfer_set_status(rsp,500,"Error executing query.");
    }
    db_list_del(ctx.list);
    ctx.list=nlist;
  }
  
  /* If we still don't have a list (as opposed to an empty list), they must not have provided any criteria.
   * That means they are asking for the entire database. Give it to them.
   */
  if (!ctx.list) {
    if (!(ctx.list=db_list_everything(ra.db))) {
      ra_http_query_context_cleanup(&ctx);
      return http_xfer_set_status(rsp,500,"Error executing query.");
    }
  }
  
  /* Sort per request.
   * TODO
   */
   
  /* Paginate if requested.
   */
  if (ctx.limit) {
    int pagec=db_list_paginate(ctx.list,ctx.limit,ctx.page-1);
    if (pagec<0) {
      ra_http_query_context_cleanup(&ctx);
      return http_xfer_set_status(rsp,500,"Improbable pagination error.");
    }
    char pagecstr[16];
    int pagecstrc=sr_decsint_repr(pagecstr,sizeof(pagecstr),pagec);
    http_xfer_set_header(rsp,"X-Page-Count",12,pagecstr,pagecstrc);
  }
  
  /* And finally, encode and queue up the results.
   */
  struct sr_encoder body={0};//TODO Can we update the http unit to expose body encoders? Here we end up with the content in memory twice.
  if (db_list_encode_array(&body,ra.db,ctx.list,DB_FORMAT_json,ctx.detail)<0) {
    ra_http_query_context_cleanup(&ctx);
    sr_encoder_cleanup(&body);
    return http_xfer_set_status(rsp,500,"Error encoding output.");
  }
  http_xfer_append_body(rsp,body.v,body.c);
  sr_encoder_cleanup(&body);
  http_xfer_set_header(rsp,"Content-Type",12,"application/json",-1);
  
  ra_http_query_context_cleanup(&ctx);
  return http_xfer_set_status(rsp,200,"OK");
}

/* /api/launch
 */
 
static int ra_http_api_launch(struct http_xfer *req,struct http_xfer *rsp,void *userdata) {
  if (http_xfer_get_method(req)!=HTTP_METHOD_POST) return http_xfer_set_status(rsp,405,"Method not allowed");
  int gameid=0;
  http_xfer_get_query_int(&gameid,req,"gameid",6);
  if (gameid<1) return http_xfer_set_status(rsp,400,"Invalid gameid");
  const struct db_game *game=db_game_get_by_id(ra.db,gameid);
  if (!game) return http_xfer_set_status(rsp,404,"Game %d not found.",gameid);
  const struct db_launcher *launcher=db_launcher_for_gameid(ra.db,gameid);
  if (!launcher) return http_xfer_set_status(rsp,500,"Unable to determine launcher for game %d.",gameid);

  //TODO launch!
  return http_xfer_set_status(rsp,500,"TODO: %s %d",__func__,gameid);
}

/* REST calls, any method and path.
 */
 
int ra_http_api(struct http_xfer *req,struct http_xfer *rsp,void *userdata) {
  const char *path=0;
  int pathc=http_xfer_get_path(&path,req);
  
  // A few exact top-level paths.
  if ((pathc==10)&&!memcmp(path,"/api/query",10)) return ra_http_api_query(req,rsp,userdata);
  if ((pathc==11)&&!memcmp(path,"/api/launch",11)) return ra_http_api_launch(req,rsp,userdata);
  
  // Match prefixes and dispatch further for the rest.
  if ((pathc>=9)&&!memcmp(path,"/api/meta",9)) return ra_http_api_meta(req,rsp,userdata);
  if ((pathc>=9)&&!memcmp(path,"/api/game",9)) return ra_http_api_game(req,rsp,userdata);
  if ((pathc>=12)&&!memcmp(path,"/api/comment",12)) return ra_http_api_comment(req,rsp,userdata);
  if ((pathc>=9)&&!memcmp(path,"/api/play",9)) return ra_http_api_play(req,rsp,userdata);
  if ((pathc>=13)&&!memcmp(path,"/api/launcher",13)) return ra_http_api_launcher(req,rsp,userdata);
  if ((pathc>=9)&&!memcmp(path,"/api/list",9)) return ra_http_api_list(req,rsp,userdata);
  if ((pathc>=9)&&!memcmp(path,"/api/blob",9)) return ra_http_api_blob(req,rsp,userdata);
  
  return http_xfer_set_status(rsp,404,"Not found.");
}

/* New WebSocket connection.
 */
 
int ra_ws_connect(struct http_socket *sock,void *userdata) {
  fprintf(stderr,"%s\n",__func__);
  return -1;
}

int ra_ws_disconnect(struct http_socket *sock,void *userdata) {
  fprintf(stderr,"%s\n",__func__);
  return -1;
}

int ra_ws_message(struct http_socket *sock,int type,const void *v,int c,void *userdata) {
  fprintf(stderr,"%s\n",__func__);
  return -1;
}
