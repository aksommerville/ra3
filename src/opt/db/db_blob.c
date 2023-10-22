#include "db_internal.h"
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"
#include <time.h>

struct db_blob_ctx {
  int include_invalid;
  int (*cb)(uint32_t gameid,const char *type,int typec,const char *time,int timec,const char *path,void *userdata);
  void *userdata;
  struct db *db;
  uint32_t gameid;
};

/* Split blob basename.
 */
 
struct db_blob_base {
  uint32_t gameid;
  const char *type; int typec;
  const char *time; int timec;
};

static int db_blob_base_split(struct db_blob_base *dst,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0;
  
  dst->gameid=0;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    dst->gameid*=10;
    dst->gameid+=src[srcp++]-'0';
  }
  if ((srcp>=srcc)||(src[srcp++]!='-')) return -1;
  
  dst->type=src+srcp;
  dst->typec=0;
  while ((srcp<srcc)&&(src[srcp++]!='-')) dst->typec++;
  
  dst->time=src+srcp;
  dst->timec=0;
  while ((srcp<srcc)&&(src[srcp++]!='.')) dst->timec++;
  
  return 0;
}

/* List all blobs.
 */
 
static int db_blob_for_each_cb(const char *path,const char *base,char type,void *userdata) {
  struct db_blob_ctx *ctx=userdata;
  struct db_blob_base split={0};
  if (db_blob_base_split(&split,base,-1)<0) {
    if (!ctx->include_invalid) return 0;
  }
  db_blobcache_add(&ctx->db->blobcache,split.gameid,base);
  return ctx->cb(split.gameid,split.type,split.typec,split.time,split.timec,path,ctx->userdata);
}

static int db_blob_for_each_bucket_cb(const char *path,const char *base,char type,void *userdata) {
  int n=0,i=0;
  for (;base[i];i++) {
    if ((base[i]<'0')||(base[i]>'9')) return 0;
    n*=10;
    n+=base[i]-'0';
  }
  if (n%100) return 0;
  return dir_read(path,db_blob_for_each_cb,userdata);
}

static int db_blob_cache_cb(uint32_t gameid,const char *base,void *userdata) {
  struct db_blob_ctx *ctx=userdata;
  struct db_blob_base split={0};
  if (db_blob_base_split(&split,base,-1)<0) {
    if (!ctx->include_invalid) return 0;
  }
  char sep=path_separator();
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s%cblob%c%d%c%s",ctx->db->rootc,ctx->db->root,sep,sep,gameid-gameid%100,sep,base);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  return ctx->cb(split.gameid,split.type,split.typec,split.time,split.timec,path,ctx->userdata);
}
 
int db_blob_for_each(
  struct db *db,
  int include_invalid,
  int (*cb)(uint32_t gameid,const char *type,int typec,const char *time,int timec,const char *path,void *userdata),
  void *userdata
) {
  struct db_blob_ctx ctx={
    .include_invalid=include_invalid,
    .cb=cb,
    .userdata=userdata,
    .db=db,
  };

  int err,empty=0;
  if (err=db_blobcache_for_all(&empty,&db->blobcache,db_blob_cache_cb,&ctx)) return err;
  if (!empty) return 0;

  char sep=path_separator();
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s%cblob",db->rootc,db->root,sep);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  if (!file_get_type(path)) {
    // Blob buckets directory is missing or unreadable -- not an error, just means "no blobs".
    return 0;
  }
  return dir_read(path,db_blob_for_each_bucket_cb,&ctx);
}

/* List all blobs for one game.
 */
 
static int db_blob_for_gameid_cb(const char *path,const char *base,char type,void *userdata) {
  struct db_blob_ctx *ctx=userdata;
  struct db_blob_base split={0};
  if (db_blob_base_split(&split,base,-1)<0) {
    if (!ctx->include_invalid) return 0;
  }
  db_blobcache_add(&ctx->db->blobcache,split.gameid,base);
  if (split.gameid!=ctx->gameid) return 0;
  return ctx->cb(split.gameid,split.type,split.typec,split.time,split.timec,path,ctx->userdata);
}

static int db_blob_cache_gameid_cb(uint32_t gameid,const char *base,void *userdata) {
  struct db_blob_ctx *ctx=userdata;
  struct db_blob_base split={0};
  if (db_blob_base_split(&split,base,-1)<0) {
    if (!ctx->include_invalid) return 0;
  }
  if (split.gameid!=ctx->gameid) return 0;
  char sep=path_separator();
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s%cblob%c%d%c%s",ctx->db->rootc,ctx->db->root,sep,sep,gameid-gameid%100,sep,base);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  return ctx->cb(split.gameid,split.type,split.typec,split.time,split.timec,path,ctx->userdata);
}

int db_blob_for_gameid(
  struct db *db,
  uint32_t gameid,
  int include_invalid,
  int (*cb)(uint32_t gameid,const char *type,int typec,const char *time,int timec,const char *path,void *userdata),
  void *userdata
) {
  struct db_blob_ctx ctx={
    .include_invalid=include_invalid,
    .cb=cb,
    .userdata=userdata,
    .db=db,
    .gameid=gameid,
  };

  int err,empty=0;
  if (err=db_blobcache_for_gameid(&empty,&db->blobcache,gameid,db_blob_cache_gameid_cb,&ctx)) return err;
  if (!empty) return 0;
  
  char sep=path_separator();
  char path[1024];
  int pathc=snprintf(path,sizeof(path),
    "%.*s%cblob%c%d",
    db->rootc,db->root,sep,sep,gameid-gameid%100
  );
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  if (!file_get_type(path)) {
    // No problem if the directory doesn't exist; there just aren't any blobs.
    return 0;
  }
  return dir_read(path,db_blob_for_gameid_cb,&ctx);
}

/* Generate blob path.
 */
 
static int db_blob_compose_path_inner(
  struct sr_encoder *encoder,
  const struct db *db,
  uint32_t gameid,
  const char *type,int typec,
  const char *sfx,int sfxc
) {
  if (sr_encode_raw(encoder,db->root,db->rootc)<0) return -1;
  if (sr_encode_u8(encoder,path_separator())<0) return -1;
  if (sr_encode_raw(encoder,"blob",4)<0) return -1;
  if (sr_encode_u8(encoder,path_separator())<0) return -1;
  if (sr_encode_fmt(encoder,"%d",gameid-gameid%100)<0) return -1;
  if (sr_encoder_terminate(encoder)<0) return -1;
  if (dir_mkdirp(encoder->v)<0) return -1;
  if (sr_encode_u8(encoder,path_separator())<0) return -1;
  
  if (!type) typec=0; else if (typec<0) { typec=0; while (type[typec]) typec++; }
  if (!sfx) sfxc=0; else if (sfxc<0) { sfxc=0; while (sfx[sfxc]) sfxc++; }
  time_t t=time(0);
  struct tm tm={0};
  localtime_r(&t,&tm);
  
  if (sr_encode_fmt(encoder,
    "%d-%.*s-%04d%02d%02d%02d%02d%02d%.*s",
    gameid,typec,type,
    1900+tm.tm_year,1+tm.tm_mon,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec,
    sfxc,sfx
  )<0) return -1;
  
  if (sr_encoder_terminate(encoder)<0) return -1;
  
  // If the file already exists, increment the last digit of the timestamp.
  // Use letters after digits, and if we run out, that's a strong sign that we should panic.
  while (file_get_type(encoder->v)=='f') {
    int finalp=encoder->c-sfxc-1;
         if (encoder->v[finalp]=='z') return -1;
    else if (encoder->v[finalp]=='Z') encoder->v[finalp]='a';
    else if (encoder->v[finalp]=='9') encoder->v[finalp]='A';
    else encoder->v[finalp]++;
  }
  
  return 0;
}

char *db_blob_compose_path(
  const struct db *db,
  uint32_t gameid,
  const char *type,int typec,
  const char *sfx,int sfxc
) {
  struct sr_encoder encoder={0};
  if (db_blob_compose_path_inner(&encoder,db,gameid,type,typec,sfx,sfxc)<0) {
    sr_encoder_cleanup(&encoder);
    return 0;
  }
  return encoder.v;
}

/* Validate path.
 */
 
int db_blob_validate_path(
  const struct db *db,
  const char *path,int pathc
) {
  if (!path) return -1;
  if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  if (pathc<db->rootc+6) return -1;
  if (memcmp(path,db->root,db->rootc)) return -1;
  if (memcmp(path+db->rootc,"/blob/",6)) return -1;
  // Not getting super aggressive with this, but do check that there's no adjacent dots, and exactly one slash after "/blob/".
  int p=db->rootc+6;
  int slashc=0;
  for (;p<pathc;p++) {
    if ((path[p]=='.')&&(path[p-1]=='.')) return -1;
    if (path[p]=='/') slashc++;
  }
  if (slashc!=1) return -1;
  return 0;
}

/* Delete all blobs for one game.
 */
 
static int db_blob_delete_for_gameid_cb(uint32_t gameid,const char *type,int typec,const char *time,int timec,const char *path,void *userdata) {
  if (file_unlink(path)<0) return -1;
  return 0;
}

int db_blob_delete_for_gameid(struct db *db,uint32_t gameid) {
  return db_blob_for_gameid(db,gameid,1,db_blob_delete_for_gameid_cb,0);
}
