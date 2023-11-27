#define _GNU_SOURCE 1 /* for qsort_r */
#include "db_internal.h"
#include "opt/serial/serial.h"

/* Random pick.
 */
 
#define DB_TICKS_PER_WEEK (60*24*7) /* a tick is one minute */
 
static uint32_t db_query_random_score(struct db *db,const struct db_game *game,uint32_t now) {
  
  // Rating is simple, just clamp it to 1..100.
  uint32_t rating=game->rating;
  if (rating<1) rating=1;
  else if (rating>100) rating=100;
  
  // Age takes some doing...
  uint32_t age=100;
  struct db_play *play;
  int playc=db_plays_get_by_gameid(&play,db,game->gameid);
  if (playc>0) {
    play+=playc-1;
    age=(now-play->time)/DB_TICKS_PER_WEEK;
  }
  if (age<1) age=1;
  else if (age>100) age=100;
  
  // And presto!
  return rating*age;
}
 
uint32_t db_query_choose_random(struct db *db,const uint32_t *gameidv,int gameidc) {
  if (gameidc<1) return 0;
  
  struct db_game *gamev=malloc(sizeof(struct db_game)*gameidc);
  if (!gamev) return 0;
  int gamec=0;
  
  uint32_t ratinghi=0,ratinglo=UINT32_MAX;
  uint32_t stringid_launcher=db_string_lookup(db,"launcher",8);
  uint32_t stringid_never=db_string_lookup(db,"never",5);
  for (;gameidc-->0;gameidv++) {
    const struct db_game *game=db_game_get_by_id(db,*gameidv);
    if (!game) continue;
    
    // Eliminate any with "faulty".
    if (game->flags&DB_FLAG_faulty) continue;
    
    // Eliminate any with "launcher=never" (on the exact string, and don't worry about whether it's the last "launcher" comment).
    if (stringid_launcher&&stringid_never) {
      int ok=1;
      struct db_comment *comment;
      int commentc=db_comments_get_by_gameid(&comment,db,game->gameid);
      for (;commentc-->0;comment++) {
        if (comment->k!=stringid_launcher) continue;
        if (comment->v!=stringid_never) continue;
        ok=0;
        break;
      }
      if (!ok) continue;
    }
    
    if (game->rating>ratinghi) ratinghi=game->rating;
    else if (game->rating<ratinglo) ratinglo=game->rating;
    gamev[gamec++]=*game;
  }
  
  // Eliminate any (rating<10), if there's at least one (>=10).
  if ((ratinglo<10)&&(ratinghi>=10)) {
    int i=gamec; while (i-->0) {
      if (gamev[i].rating<10) {
        gamec--;
        memmove(gamev+i,gamev+i+1,sizeof(struct db_game)*(gamec-i));
      }
    }
  }
  
  /* Score each game based on rating and playtime. Write the score into (rating).
   * Don't worry: This is a copy of the game list, we can write freely.
   */
  uint32_t total=0;
  uint32_t now=db_time_now();
  struct db_game *game=gamev;
  int i=gamec;
  for (;i-->0;game++) {
    game->rating=db_query_random_score(db,game,now);
    total+=game->rating;
  }
  
  // Select randomly.
  uint32_t gameid=0;
  if (total>0) {
    uint32_t choice=rand()%total;
    for (game=gamev,i=gamec;i-->0;game++) {
      if (choice<game->rating) {
        gameid=game->gameid;
        break;
      }
      choice-=game->rating;
    }
  }
  
  free(gamev);
  return gameid;
}

/* Search for a query string in a search space that may be nul-terminated, or a fixed limit.
 * Query must be normalized: Lowercase letters, digits, and space with no leading or trailing space.
 */
 
static int db_strsearch_1(const char *a,const char *b,int c) {
  for (;c-->0;a++,b++) {
    char bch=*b;
         if ((bch>='a')&&(bch<='z')) ;
    else if ((bch>='A')&&(bch<='Z')) bch+=0x20;
    else if ((bch>='0')&&(bch<='9')) ;
    else bch=' ';
    if (*a!=bch) return 0;
  }
  return 1;
}
 
static int db_strsearch_limited(const char *q,int qc,const char *text,int textc) {
  while (textc&&!text[textc-1]) textc--;
  int textp=0,stopp=textc-qc;
  for (;textp<=stopp;textp++) {
    if (db_strsearch_1(q,text+textp,qc)) return 1;
  }
  return 0;
}

/* Search in a string resource.
 */
 
static int db_strsearch_stringid(const char *q,int qc,struct db *db,uint32_t stringid) {
  if (!stringid) return 0;
  const char *text=0;
  int textc=db_string_get(&text,db,stringid);
  int i=textc-qc;
  while (i>=0) {
    if (db_strsearch_1(q,text+i,qc)) return 1;
    i--;
  }
  return 0;
}

/* Search among all comments for one game.
 */
 
static int db_strsearch_comments(const char *q,int qc,const struct db *db,uint32_t gameid) {
  struct db_comment *comment;
  int commentc=db_comments_get_by_gameid(&comment,db,gameid);
  for (;commentc-->0;comment++) {
    const char *src;
    int srcc=db_string_get(&src,db,comment->v);
    if (db_strsearch_limited(q,qc,src,srcc)) return 1;
  }
  return 0;
}

/* Loose text query.
 */

struct db_list *db_query_text(
  struct db *db,
  struct db_list *src,
  const char *q,int qc
) {
  if (!q) qc=0; else if (qc<0) { qc=0; while (q[qc]) qc++; }
  struct db_list *dst=db_list_copy_nonresident(0);
  if (!dst) return 0;
  
  /* Query text must be lower case letters, digits, and space, nothing else.
   * Trim leading and trailing space, and turn all punctuation into spaces.
   * We'll also enforce a length limit post-normalization, I think 32 is plenty.
   */
  char norm[32];
  int normc=0,i;
  for (;i<qc;i++) {
    if (normc>=sizeof(norm)) return dst; // query too long, pretend nothing matched
    if ((q[i]>='a')&&(q[i]<='z')) norm[normc++]=q[i];
    else if ((q[i]>='0')&&(q[i]<='9')) norm[normc++]=q[i];
    else if ((q[i]>='A')&&(q[i]<='Z')) norm[normc++]=q[i]+0x20;
    else if (!normc) ;
    else if (norm[normc-1]==' ') ;
    else norm[normc++]=' ';
  }
  if (normc&&(norm[normc-1]==' ')) normc--;
  
  const struct db_game *game;
  if (src) {
    dst->sorted=src->sorted;
    if (src->gameidc!=src->gamec) db_list_gamev_populate(db,src);
    game=src->gamev;
    i=src->gamec;
  } else {
    dst->sorted=1;
    game=db->games.v;
    i=db->games.c;
  }
  for (;i-->0;game++) {
    if (
      db_strsearch_limited(norm,normc,game->name,sizeof(game->name))||
      db_strsearch_limited(norm,normc,game->base,sizeof(game->base))||
      db_strsearch_stringid(norm,normc,db,game->author)||
      db_strsearch_comments(norm,normc,db,game->gameid)
    ) {
      if (db_list_append(db,dst,game->gameid)<0) {
        db_list_del(dst);
        return 0;
      }
    }
  }
  return dst;
}

/* Structured query against game header.
 */

struct db_list *db_query_header(
  struct db *db,
  struct db_list *src,
  const char *platform,int platformc,
  const char *author,int authorc,
  const char *genre,int genrec,
  uint32_t flags_require,
  uint32_t flags_forbid,
  uint32_t rating_lo,
  uint32_t rating_hi,
  uint32_t pubtime_lo,
  uint32_t pubtime_hi
) {
  struct db_list *dst=db_list_copy_nonresident(0);
  if (!dst) return 0;
  const struct db_game *game;
  int i;
  if (src) {
    dst->sorted=src->sorted;
    if (src->gameidc!=src->gamec) db_list_gamev_populate(db,src);
    game=src->gamev;
    i=src->gamec;
  } else {
    dst->sorted=1;
    game=db->games.v;
    i=db->games.c;
  }
  
  // Check for invalid combinations of integer criteria, to short circuit.
  if (flags_require&flags_forbid) return dst;
  if (rating_lo>rating_hi) return dst;
  if (pubtime_lo>pubtime_hi) return dst;
  
  // If any string lookup fails, we're done, nothing can match.
  if (!platform) platformc=0; else if (platformc<0) { platformc=0; while (platform[platformc]) platformc++; }
  uint32_t s_platform=db_string_lookup(db,platform,platformc);
  if (platformc&&!s_platform) return dst;
  if (!author) authorc=0; else if (authorc<0) { authorc=0; while (author[authorc]) authorc++; }
  uint32_t s_author=db_string_lookup(db,author,authorc);
  if (authorc&&!s_author) return dst;
  if (!genre) genrec=0; else if (genrec<0) { genrec=0; while (genre[genrec]) genrec++; }
  uint32_t s_genre=db_string_lookup(db,genre,genrec);
  if (genrec&&!s_genre) return dst;
  
  for (;i-->0;game++) {
  
    if (s_platform&&(game->platform!=s_platform)) continue;
    if (s_author&&(game->author!=s_author)) continue;
    if (s_genre&&(game->genre!=s_genre)) continue;
    if (game->flags&flags_forbid) continue;
    if ((game->flags&flags_require)!=flags_require) continue;
    if (game->rating<rating_lo) continue;
    if (game->rating>rating_hi) continue;
    if (game->pubtime<pubtime_lo) continue;
    if (game->pubtime>pubtime_hi) continue;
    
    if (db_list_append(db,dst,game->gameid)<0) {
      db_list_del(dst);
      return 0;
    }
  }
  return dst;
}

struct db_list *db_query_header_prelookupped(
  struct db *db,
  struct db_list *src,
  uint32_t platform,
  uint32_t author,
  uint32_t genre,
  uint32_t flags_require,
  uint32_t flags_forbid,
  uint32_t rating_lo,
  uint32_t rating_hi,
  uint32_t pubtime_lo,
  uint32_t pubtime_hi
) {
  struct db_list *dst=db_list_copy_nonresident(0);
  if (!dst) return 0;
  const struct db_game *game;
  int i;
  if (src) {
    dst->sorted=src->sorted;
    if (src->gameidc!=src->gamec) db_list_gamev_populate(db,src);
    game=src->gamev;
    i=src->gamec;
  } else {
    dst->sorted=1;
    game=db->games.v;
    i=db->games.c;
  }
  
  // Check for invalid combinations of integer criteria, to short circuit.
  if (flags_require&flags_forbid) return dst;
  if (rating_lo>rating_hi) return dst;
  if (pubtime_lo>pubtime_hi) return dst;
  
  for (;i-->0;game++) {
  
    if (platform&&(game->platform!=platform)) continue;
    if (author&&(game->author!=author)) continue;
    if (genre&&(game->genre!=genre)) continue;
    if (game->flags&flags_forbid) continue;
    if ((game->flags&flags_require)!=flags_require) continue;
    if (game->rating<rating_lo) continue;
    if (game->rating>rating_hi) continue;
    if (game->pubtime<pubtime_lo) continue;
    if (game->pubtime>pubtime_hi) continue;
    
    if (db_list_append(db,dst,game->gameid)<0) {
      db_list_del(dst);
      return 0;
    }
  }
  return dst;
}

/* Generic query.
 */

struct db_list *db_query_generic(
  struct db *db,
  struct db_list *src,
  int (*filter)(const struct db_game *game,void *userdata),
  void *userdata
) {
  struct db_list *dst=db_list_copy_nonresident(0);
  if (!dst) return 0;
  const struct db_game *game;
  int i;
  if (src) {
    dst->sorted=src->sorted;
    if (src->gameidc!=src->gamec) db_list_gamev_populate(db,src);
    game=src->gamev;
    i=src->gamec;
  } else {
    dst->sorted=1;
    game=db->games.v;
    i=db->games.c;
  }
  for (;i-->0;game++) {
    if (filter(game,userdata)) {
      if (db_list_append(db,dst,game->gameid)<0) {
        db_list_del(dst);
        return 0;
      }
    }
  }
  return dst;
}

/* List AND
 */
 
struct db_list *db_query_list_and(struct db *db,const struct db_list *a,const struct db_list *b) {
  struct db_list *dst=db_list_copy_nonresident(0);
  if (!dst) return 0;
  if (a->sorted&&b->sorted) {
    dst->sorted=1;
    int ap=0,bp=0;
    const uint32_t *agameid=a->gameidv,*bgameid=b->gameidv;
    while ((ap<a->gameidc)&&(bp<b->gameidc)) {
      if (*agameid<*bgameid) { ap++; agameid++; }
      else if (*agameid>*bgameid) { bp++; bgameid++; }
      else {
        if (db_list_append(db,dst,*agameid)<0) {
          db_list_del(dst);
          return 0;
        }
        ap++;
        bp++;
        agameid++;
        bgameid++;
      }
    }
  } else {
    const uint32_t *gameid;
    int i;
    for (i=a->gameidc,gameid=a->gameidv;i-->0;gameid++) {
      if (!db_list_has(b,*gameid)) continue;
      if (db_list_append(db,dst,*gameid)<0) {
        db_list_del(dst);
        return 0;
      }
    }
  }
  return dst;
}

/* List OR
 */
 
struct db_list *db_query_list_or(struct db *db,const struct db_list *a,const struct db_list *b) {
  struct db_list *dst=db_list_copy_nonresident(0);
  if (!dst) return 0;
  if (a->sorted&&b->sorted) {
    dst->sorted=1;
    int ap=0,bp=0;
    const uint32_t *agameid=a->gameidv,*bgameid=b->gameidv;
    while (1) {
      #define ADDFROM(which) { \
        if (db_list_append(0,dst,*which##gameid)<0) { \
          db_list_del(dst); \
          return 0; \
        } \
        which##p++; \
        which##gameid++; \
      }
      if (ap>=a->gameidc) ADDFROM(b)
      else if (bp>=b->gameidc) ADDFROM(a)
      else if (*agameid<*bgameid) ADDFROM(a)
      else if (*agameid>*bgameid) ADDFROM(b)
      else {
        ADDFROM(a)
        bp++;
        bgameid++;
      }
      #undef ADDFROM
    }
  } else {
    const uint32_t *gameid;
    int i;
    for (i=a->gameidc,gameid=a->gameidv;i-->0;gameid++) {
      if (db_list_append(db,dst,*gameid)<0) {
        db_list_del(dst);
        return 0;
      }
    }
    for (i=b->gameidc,gameid=b->gameidv;i-->0;gameid++) {
      if (db_list_append(db,dst,*gameid)<0) {
        db_list_del(dst);
        return 0;
      }
    }
  }
  return dst;
}

/* List NOT
 */
 
struct db_list *db_query_list_not(struct db *db,const struct db_list *from,const struct db_list *remove) {
  struct db_list *dst=db_list_copy_nonresident(0);
  if (!dst) return 0;
  if (from->sorted&&remove->sorted) {
    dst->sorted=1;
    int ap=0,bp=0;
    for (;ap<from->gameidc;ap++) {
      uint32_t gameid=from->gameidv[ap];
      while ((bp<remove->gameidc)&&(remove->gameidv[bp]<gameid)) bp++;
      if ((bp<remove->gameidc)&&(remove->gameidv[bp]==gameid)) {
        bp++;
      } else {
        if (db_list_append(0,dst,gameid)<0) {
          db_list_del(dst);
          return 0;
        }
      }
    }
  } else {
    const uint32_t *gameid=from->gameidv;
    int i=from->gameidc;
    for (;i-->0;gameid++) {
      if (db_list_has(remove,*gameid)) continue;
      if (db_list_append(0,dst,*gameid)<0) {
        db_list_del(dst);
        return 0;
      }
    }
  }
  return dst;
}

/* Paginate list in place.
 */
 
int db_list_paginate(struct db_list *list,int page_size,int page_index) {
  if (page_size<1) return -1;
  int pagec=(list->gameidc+page_size-1)/page_size;
  if ((page_index<0)||(page_index>=pagec)) {
    db_list_clear(0,list);
    return pagec;
  }
  int fromp=page_index*page_size;
  int nc=list->gameidc-fromp;
  if (nc>page_size) nc=page_size;
  memmove(list->gameidv,list->gameidv+fromp,sizeof(uint32_t)*nc);
  if (list->gamec==list->gameidc) {
    memmove(list->gamev,list->gamev+fromp,sizeof(struct db_game)*nc);
    list->gamec=nc;
  } else {
    list->gamec=0;
  }
  list->gameidc=nc;
  return pagec;
}

/* Cleanup histogram.
 */
 
void db_histogram_cleanup(struct db_histogram *hist) {
  if (hist->v) free(hist->v);
}

/* Histogram primitives.
 */

int db_histogram_search(const struct db_histogram *hist,uint32_t stringid) {
  int lo=0,hi=hist->c;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    uint32_t q=hist->v[ck].stringid;
         if (stringid<q) hi=ck;
    else if (stringid>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

struct db_histogram_entry *db_histogram_insert(struct db_histogram *hist,int p,uint32_t stringid) {
  if ((p<0)||(p>hist->c)) return 0;
  if (p&&(stringid<=hist->v[p-1].stringid)) return 0;
  if ((p<hist->c)&&(stringid>=hist->v[p].stringid)) return 0;
  if (hist->c>=hist->a) {
    int na=hist->a+32;
    if (na>INT_MAX/sizeof(struct db_histogram_entry)) return 0;
    void *nv=realloc(hist->v,sizeof(struct db_histogram_entry)*na);
    if (!nv) return 0;
    hist->v=nv;
    hist->a=na;
  }
  struct db_histogram_entry *entry=hist->v+p;
  memmove(entry+1,entry,sizeof(struct db_histogram_entry)*(hist->c-p));
  hist->c++;
  entry->stringid=stringid;
  entry->count=0;
  return entry;
}

int db_histogram_add(struct db_histogram *hist,uint32_t stringid,int addc) {
  struct db_histogram_entry *entry;
  int p=db_histogram_search(hist,stringid);
  if (p<0) {
    p=-p-1;
    if (!(entry=db_histogram_insert(hist,p,stringid))) return -1;
  } else {
    entry=hist->v+p;
  }
  if (addc>0) {
    if (entry->count>UINT32_MAX-addc) entry->count=UINT32_MAX;
    else entry->count+=addc;
  }
  return 0;
}

/* Sort histogram.
 */
 
static int db_histogram_cmp_alpha(const struct db_histogram_entry *a,const struct db_histogram_entry *b,const struct db *db) {
  const char *av,*bv;
  int ac=db_string_get(&av,db,a->stringid); if (ac<0) ac=0;
  int bc=db_string_get(&bv,db,b->stringid); if (bc<0) bc=0;
  int cmpc=(ac<bc)?ac:bc;
  int cmp=memcmp(av,bv,cmpc);
  if (cmp) return cmp;
  return (ac<bc)?-1:(ac>bc)?1:0;
}

static int db_histogram_cmp_c_desc(const struct db_histogram_entry *a,const struct db_histogram_entry *b,const struct db *db) {
  return b->count-a->count;
}
 
static void db_histogram_sort(
  struct db_histogram *hist,
  int (*cb)(const struct db_histogram_entry *a,const struct db_histogram_entry *b,const struct db *db),
  const struct db *db
) {
  qsort_r(hist->v,hist->c,sizeof(struct db_histogram_entry),(void*)cb,(void*)db);
}
 
void db_histogram_sort_alpha(struct db_histogram *hist,const struct db *db) {
  db_histogram_sort(hist,db_histogram_cmp_alpha,db);
}

void db_histogram_sort_c_desc(struct db_histogram *hist,const struct db *db) {
  db_histogram_sort(hist,db_histogram_cmp_c_desc,db);
}

/* Generate histograms.
 */
 
int db_histogram_platform(struct db_histogram *hist,const struct db *db) {
  const struct db_game *game=db->games.v;
  int i=db->games.c;
  for (;i-->0;game++) {
    if (db_histogram_add(hist,game->platform,1)<0) return -1;
  }
  const struct db_launcher *launcher=db->launchers.v;
  for (i=db->launchers.c;i-->0;launcher++) {
    if (!launcher->platform) continue;
    if (db_histogram_add(hist,launcher->platform,0)<0) return -1;
  }
  return 0;
}

int db_histogram_author(struct db_histogram *hist,const struct db *db) {
  const struct db_game *game=db->games.v;
  int i=db->games.c;
  for (;i-->0;game++) {
    if (db_histogram_add(hist,game->author,1)<0) return -1;
  }
  return 0;
}

int db_histogram_genre(struct db_histogram *hist,const struct db *db) {
  const struct db_game *game=db->games.v;
  int i=db->games.c;
  for (;i-->0;game++) {
    if (db_histogram_add(hist,game->genre,1)<0) return -1;
  }
  return 0;
}

int db_histogram_rating(struct db_histogram *hist,const struct db *db,int bucket_size) {
  if (bucket_size<1) bucket_size=1;
  const struct db_game *game=db->games.v;
  int i=db->games.c;
  for (;i-->0;game++) {
    if (db_histogram_add(hist,(game->rating/bucket_size)*bucket_size,1)<0) return -1;
  }
  return 0;
}

int db_histogram_pubtime(struct db_histogram *hist,const struct db *db) {
  const struct db_game *game=db->games.v;
  int i=db->games.c;
  for (;i-->0;game++) {
    if (db_histogram_add(hist,DB_YEAR_FROM_TIME(game->pubtime),1)<0) return -1;
  }
  return 0;
}

/* Encode histogram.
 */

int db_histogram_encode(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_histogram *hist,
  int format,
  int detail
) {
  switch (format) {
    case 0:
    case DB_FORMAT_json: {
        int jsonctx=sr_encode_json_array_start_no_setup(dst);
        if (jsonctx<0) return -1;
        const struct db_histogram_entry *entry=hist->v;
        int i=hist->c;
        if (detail==DB_DETAIL_id) {
          for (;i-->0;entry++) {
            int subctx=sr_encode_json_object_start(dst,0,0);
            if (subctx<0) return -1;
            if (sr_encode_json_int(dst,"v",1,entry->stringid)<0) return -1;
            if (sr_encode_json_int(dst,"c",1,entry->count)<0) return -1;
            if (sr_encode_json_object_end(dst,subctx)<0) return -1;
          }
        } else if (detail==DB_DETAIL_name) {
          for (;i-->0;entry++) {
            if (db_encode_json_string(dst,db,0,0,entry->stringid)<0) return -1;
          }
        } else {
          for (;i-->0;entry++) {
            int subctx=sr_encode_json_object_start(dst,0,0);
            if (subctx<0) return -1;
            if (db_encode_json_string(dst,db,"v",1,entry->stringid)<0) return -1;
            if (sr_encode_json_int(dst,"c",1,entry->count)<0) return -1;
            if (sr_encode_json_object_end(dst,subctx)<0) return -1;
          }
        }
        if (sr_encode_json_array_end(dst,jsonctx)<0) return -1;
      } return 0;
  }
  return -1;
}
