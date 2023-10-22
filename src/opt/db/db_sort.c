#include "db_internal.h"

/* Sort names.
 */
 
int db_sort_eval(int *descend,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (descend) *descend=0;
  if (srcc<1) return DB_SORT_none;
  if (src[0]=='-') {
    src++;
    srcc--;
    if (descend) *descend=1;
  } else if (src[0]=='+') {
    src++;
    srcc--;
  }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(src,#tag,srcc)) return DB_SORT_##tag;
  DB_SORT_FOR_EACH
  #undef _
  return -1;
}

/* Sort by gameid.
 */
 
static int db_cmp_gameid(const void *a,const void *b) {
  return *(int32_t*)a-*(int32_t*)b;
}
 
static int db_cmp_gameid_rev(const void *a,const void *b) {
  return *(int32_t*)b-*(int32_t*)a;
}

/* Sort by header fields.
 */
 
static int db_cmp_name(const struct db_game *a,const struct db_game *b) {
  int ap=0,bp=0;
  while (1) {
  
    // If one terminates, it wins. Both terminate, break and compare IDs.
    if ((ap>=sizeof(a->name))||!a->name[ap]) {
      if ((bp>=sizeof(a->name))||!b->name[bp]) break;
      return -1;
    } else if ((bp>=sizeof(b->name))||!b->name[bp]) {
      return 1;
    }
    
    // Force letters lowercase, keep digits, and all else is space.
    // On a space, skip it and try again. Spaces and punctuation don't count at all.
    int ach=(unsigned char)(a->name[ap]);
         if ((ach>='A')&&(ach<='Z')) ach+=0x20;
    else if ((ach>='a')&&(ach<='z')) ;
    else if ((ach>='0')&&(ach<='9')) ;
    else { ap++; continue; }
    int bch=(unsigned char)(b->name[bp]);
         if ((bch>='A')&&(bch<='Z')) bch+=0x20;
    else if ((bch>='a')&&(bch<='z')) ;
    else if ((bch>='0')&&(bch<='9')) ;
    else { bp++; continue; }
    ap++;
    bp++;
    
    if (ach<bch) return -1;
    if (ach>bch) return 1;
  }
  return (int32_t)a->gameid-(int32_t)b->gameid;
}
 
static int db_cmp_name_rev(const struct db_game *a,const struct db_game *b) {
  return db_cmp_name(b,a);
}
 
static int db_cmp_pubtime(const struct db_game *a,const struct db_game *b) {
  int cmp;
  if (cmp=(int32_t)a->pubtime-(int32_t)b->pubtime) return cmp;
  return (int32_t)a->gameid-(int32_t)b->gameid;
}
 
static int db_cmp_pubtime_rev(const struct db_game *a,const struct db_game *b) {
  int cmp;
  if (cmp=(int32_t)b->pubtime-(int32_t)a->pubtime) return cmp;
  return (int32_t)b->gameid-(int32_t)a->gameid;
}
 
static int db_cmp_rating(const struct db_game *a,const struct db_game *b) {
  int cmp;
  if (cmp=(int32_t)a->rating-(int32_t)b->rating) return cmp;
  return (int32_t)a->gameid-(int32_t)b->gameid;
}
 
static int db_cmp_rating_rev(const struct db_game *a,const struct db_game *b) {
  int cmp;
  if (cmp=(int32_t)b->rating-(int32_t)a->rating) return cmp;
  return (int32_t)b->gameid-(int32_t)a->gameid;
}

/* Sort by name, pubtime, or rating: All necessary content lives in the header.
 */
 
static int db_list_sort_header(struct db *db,struct db_list *list,int sort,int descend) {
  if (db_list_gamev_populate(db,list)<0) return -1;
  if (list->gameidc!=list->gamec) return -1;
  void *cmp;
  switch (sort) {
    case DB_SORT_name: cmp=descend?db_cmp_name_rev:db_cmp_name; break;
    case DB_SORT_pubtime: cmp=descend?db_cmp_pubtime_rev:db_cmp_pubtime; break;
    case DB_SORT_rating: cmp=descend?db_cmp_rating_rev:db_cmp_rating; break;
    default: return -1;
  }
  qsort(list->gamev,list->gamec,sizeof(struct db_game),cmp);
  uint32_t *id=list->gameidv;
  const struct db_game *game=list->gamev;
  int i=list->gamec;
  for (;i-->0;id++,game++) *id=game->gameid;
  return 0;
}

/* Scoring of individual games.
 */
 
static uint32_t db_score_playtime(struct db *db,uint32_t gameid) {
  struct db_play *play=0;
  int playc=db_plays_get_by_gameid(&play,db,gameid);
  uint32_t playtime=0;
  for (;playc-->0;play++) {
    // Regardless of the sort order, this sort refers to the most recent play.
    if (play->time>playtime) playtime=play->time;
  }
  return playtime;
}

static uint32_t db_score_playcount(struct db *db,uint32_t gameid) {
  struct db_play *dummy;
  return db_plays_get_by_gameid(&dummy,db,gameid);
}

static int db_cb_blob_exists(uint32_t gameid,const char *type,int typec,const char *time,int timec,const char *path,void *userdata) {
  (*(uint32_t*)userdata)+=10;
  return 1;
}

static uint32_t db_score_fullness(struct db *db,uint32_t gameid) {
  uint32_t score=0;
  
  // Header: One point per field, except critical ones are 5.
  const struct db_game *game=db_game_get_by_id(db,gameid);
  if (game) {
    if (game->platform) score+=5;
    if (game->author) score++;
    if (game->genre) score++;
    if (game->flags) score++;
    if (game->rating) score++;
    if (game->pubtime) score++;
    if (game->dir) score+=5;
    if (game->name[0]) score+=5;
    if (game->base[0]) score+=5;
  }
  
  // 10 points if at least one exists, for comments and blobs.
  struct db_comment *dummy;
  if (db_comments_get_by_gameid(&dummy,db,gameid)>0) score+=10;
  db_blob_for_gameid(db,gameid,0,db_cb_blob_exists,&score);
  
  return score;
}

/* Sort by playtime, playcount, or fullness: Parallel scoring required.
 */
 
struct db_score {
  uint32_t gameid;
  uint32_t score;
};

static int db_cmp_score(const struct db_score *a,const struct db_score *b) {
  int cmp;
  if (cmp=(int32_t)a->score-(int32_t)b->score) return cmp;
  return (int32_t)a->gameid-(int32_t)b->gameid;
}
 
static int db_list_sort_score(struct db *db,struct db_list *list,int sort,int descend) {
  struct db_score *scorev=malloc(sizeof(struct db_score)*list->gameidc);
  if (!scorev) return -1;
  
  struct db_score *score=scorev;
  uint32_t *gameid=list->gameidv;
  int i=list->gameidc;
  switch (sort) {
    case DB_SORT_playtime: for (;i-->0;score++,gameid++) { score->gameid=*gameid; score->score=db_score_playtime(db,*gameid); } break;
    case DB_SORT_playcount: for (;i-->0;score++,gameid++) { score->gameid=*gameid; score->score=db_score_playcount(db,*gameid); } break;
    case DB_SORT_fullness: for (;i-->0;score++,gameid++) { score->gameid=*gameid; score->score=db_score_fullness(db,*gameid); } break;
    default: free(scorev); return -1;
  }
  
  if (descend) {
    for (score=scorev,i=list->gameidc;i-->0;score++) score->score=UINT32_MAX-score->score;
  }
  qsort(scorev,list->gameidc,sizeof(struct db_score),(void*)db_cmp_score);
  
  for (score=scorev,gameid=list->gameidv,i=list->gameidc;i-->0;score++,gameid++) *gameid=score->gameid;
  list->gamec=0;
  free(scorev);
  return 0;
}

/* Sort, main entry point.
 */
  
int db_list_sort_auto(struct db *db,struct db_list *list,int sort,int descend) {
  if (!list) return -1;
  if (list->gameidc<2) return 0;
  
  // "none" is the easiest: Do nothing.
  if (sort<=DB_SORT_none) return 0;
  
  // "id" also very easy: Drop game headers and qsort the id list.
  if (sort<=DB_SORT_id) {
    list->gamec=0;
    qsort(list->gameidv,list->gameidc,sizeof(uint32_t),descend?db_cmp_gameid_rev:db_cmp_gameid);
    return 0;
  }
  
  // "name", "pubtime", "rating" we like to populate gamev, then everything we need is there.
  if (sort<=DB_SORT_rating) {
    return db_list_sort_header(db,list,sort,descend);
  }
  
  // "playtime", "playcount", and "fullness" require examination of other tables, and building a parallel score array.
  if (sort<=DB_SORT_fullness) {
    return db_list_sort_score(db,list,sort,descend);
  }
  
  return -1;
}
