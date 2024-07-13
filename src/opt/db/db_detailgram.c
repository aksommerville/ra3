#include "db_internal.h"

/* Cleanup.
 */
 
void db_detailgram_cleanup(struct db_detailgram *dg) {
  if (!dg) return;
  if (dg->entryv) free(dg->entryv);
  memset(dg,0,sizeof(struct db_detailgram));
}

/* Entry list primitives.
 */
 
static int db_detailgram_search(const struct db_detailgram *dg,uint32_t v) {
  int lo=0,hi=dg->entryc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    uint32_t q=dg->entryv[ck].v;
         if (v<q) hi=ck;
    else if (v>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static struct db_detailgram_entry *db_detailgram_insert(struct db_detailgram *dg,int p,uint32_t v) {
  if ((p<0)||(p>dg->entryc)) return 0;
  if (dg->entryc>=dg->entrya) {
    int na=dg->entrya+256;
    if (na>INT_MAX/sizeof(struct db_detailgram_entry)) return 0;
    void *nv=realloc(dg->entryv,sizeof(struct db_detailgram_entry)*na);
    if (!nv) return 0;
    dg->entryv=nv;
    dg->entrya=na;
  }
  struct db_detailgram_entry *entry=dg->entryv+p;
  memmove(entry+1,entry,sizeof(struct db_detailgram_entry)*(dg->entryc-p));
  dg->entryc++;
  memset(entry,0,sizeof(struct db_detailgram_entry));
  entry->v=v;
  return entry;
}

/* Gather.
 */

int db_detailgram_gather(struct db_detailgram *dg,const char *field) {
  
  if (!dg||!dg->db||!field) return -1;
  if (!strcmp(field,"author")) dg->fldp=(uintptr_t)(&((struct db_game*)0)->author)/4;
  else if (!strcmp(field,"platform")) dg->fldp=(uintptr_t)(&((struct db_game*)0)->platform)/4;
  else if (!strcmp(field,"genre")) dg->fldp=(uintptr_t)(&((struct db_game*)0)->genre)/4;
  else return -1;
  dg->entryc=0;
  
  const struct db_game *game=dg->db->games.v;
  int i=dg->db->games.c;
  for (;i-->0;game++) {
    uint32_t v=((uint32_t*)game)[dg->fldp];
    if (!v) continue;
    struct db_detailgram_entry *entry;
    int p=db_detailgram_search(dg,v);
    if (p<0) {
      if (!(entry=db_detailgram_insert(dg,-p-1,v))) return -1;
    } else {
      entry=dg->entryv+p;
    }
    entry->rating_sum+=game->rating;
    if (!entry->gamec) entry->rating_lo=entry->rating_hi=game->rating;
    else if (game->rating>entry->rating_hi) entry->rating_hi=game->rating;
    else if (game->rating<entry->rating_lo) entry->rating_lo=game->rating;
    if (game->pubtime) {
      if (!entry->pubtime_hi) entry->pubtime_lo=entry->pubtime_hi=game->pubtime;
      else if (game->pubtime>entry->pubtime_hi) entry->pubtime_hi=game->pubtime;
      else if (game->pubtime<entry->pubtime_lo) entry->pubtime_lo=game->pubtime;
    }
    uint32_t flag=1,flagp=0;
    for (;flag&&(flag<=game->flags);flag<<=1,flagp++) {
      if (game->flags&flag) entry->flags[flagp]++;
    }
    entry->gamec++;
  }
  
  struct db_detailgram_entry *entry=dg->entryv;
  for (i=dg->entryc;i-->0;entry++) {
    entry->rating_avg=entry->rating_sum/entry->gamec;
  }
  
  return 0;
}

/* Filter by game count.
 */
 
void db_detailgram_filter_gamec(struct db_detailgram *dg,int min,int max) {
  int p=dg->entryc;
  while (p-->0) {
    struct db_detailgram_entry *entry=dg->entryv+p;
    if ((entry->gamec<min)||(entry->gamec>max)) {
      dg->entryc--;
      memmove(entry,entry+1,sizeof(struct db_detailgram_entry)*(dg->entryc-p));
    }
  }
}

/* Sort.
 */
 
static int db_detailgram_cmp_count_desc(const void *a,const void *b) {
  const struct db_detailgram_entry *A=a,*B=b;
  return B->gamec-A->gamec;
}
 
static int db_detailgram_cmp_count_asc(const void *a,const void *b) {
  const struct db_detailgram_entry *A=a,*B=b;
  return A->gamec-B->gamec;
}
 
static int db_detailgram_cmp_rating_avg_desc(const void *a,const void *b) {
  const struct db_detailgram_entry *A=a,*B=b;
  return B->rating_avg-A->rating_avg;
}
 
static int db_detailgram_cmp_rating_avg_asc(const void *a,const void *b) {
  const struct db_detailgram_entry *A=a,*B=b;
  return A->rating_avg-B->rating_avg;
}
 
static int db_detailgram_cmp_pubtime_hi_desc(const void *a,const void *b) {
  const struct db_detailgram_entry *A=a,*B=b;
  return (int)B->pubtime_hi-(int)A->pubtime_hi;
}
 
static int db_detailgram_cmp_pubtime_lo_asc(const void *a,const void *b) {
  const struct db_detailgram_entry *A=a,*B=b;
  if (A->pubtime_lo==B->pubtime_lo) return 0;
  if (!A->pubtime_lo) return 1;
  if (!B->pubtime_lo) return -1;
  return (int)A->pubtime_lo-(int)B->pubtime_lo;
}
 
void db_detailgram_sort_count_desc(struct db_detailgram *dg) {
  qsort(dg->entryv,dg->entryc,sizeof(struct db_detailgram_entry),db_detailgram_cmp_count_desc);
}

void db_detailgram_sort_count_asc(struct db_detailgram *dg) {
  qsort(dg->entryv,dg->entryc,sizeof(struct db_detailgram_entry),db_detailgram_cmp_count_asc);
}

void db_detailgram_sort_rating_avg_desc(struct db_detailgram *dg) {
  qsort(dg->entryv,dg->entryc,sizeof(struct db_detailgram_entry),db_detailgram_cmp_rating_avg_desc);
}

void db_detailgram_sort_rating_avg_asc(struct db_detailgram *dg) {
  qsort(dg->entryv,dg->entryc,sizeof(struct db_detailgram_entry),db_detailgram_cmp_rating_avg_asc);
}

void db_detailgram_sort_pubtime_hi_desc(struct db_detailgram *dg) {
  qsort(dg->entryv,dg->entryc,sizeof(struct db_detailgram_entry),db_detailgram_cmp_pubtime_hi_desc);
}

void db_detailgram_sort_pubtime_lo_asc(struct db_detailgram *dg) {
  qsort(dg->entryv,dg->entryc,sizeof(struct db_detailgram_entry),db_detailgram_cmp_pubtime_lo_asc);
}
