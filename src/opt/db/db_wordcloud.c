#include "db_internal.h"
#include "opt/serial/serial.h"

/* Cleanup.
 */
 
void db_wordcloud_cleanup(struct db_wordcloud *wc) {
  if (wc->entryv) free(wc->entryv);
  memset(wc,0,sizeof(struct db_wordcloud));
}

/* Entry list.
 * Sorting is by (vc,content), the way we sort during gather.
 */
 
static int db_wordcloud_search(const struct db_wordcloud *wc,const char *src,int srcc) {
  int lo=0,hi=wc->entryc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct db_wordcloud_entry *q=wc->entryv+ck;
         if (srcc<q->vc) hi=ck;
    else if (srcc>q->vc) lo=ck+1;
    else {
      int cmp=sr_memcasecmp(src,q->v,srcc);
           if (cmp<0) hi=ck;
      else if (cmp>0) lo=ck+1;
      else return ck;
    }
  }
  return -lo-1;
}

static int db_wordcloud_insert(struct db_wordcloud *wc,int p,const char *src,int srcc) {
  if (srcc<1) return -1;
  if ((p<0)||(p>wc->entryc)) return -1;
  if (wc->entryc>=wc->entrya) {
    int na=wc->entrya+1024;
    if (na>INT_MAX/sizeof(struct db_wordcloud_entry)) return -1;
    void *nv=realloc(wc->entryv,sizeof(struct db_wordcloud_entry)*na);
    if (!nv) return -1;
    wc->entryv=nv;
    wc->entrya=na;
  }
  struct db_wordcloud_entry *entry=wc->entryv+p;
  memmove(entry+1,entry,sizeof(struct db_wordcloud_entry)*(wc->entryc-p));
  wc->entryc++;
  memset(entry,0,sizeof(struct db_wordcloud_entry));
  entry->v=src;
  entry->vc=srcc;
  entry->instc=1;
  return 0;
}

/* Pre-filter some words out.
 * Articles, prepositions, conjunctions, anything shorter than 3 characters.
 * Mind that "s" and "t" are crazy common, because our tokenizer breaks on apostrophe.
 * Also "home", "andy", and "kiddo", since those appear in every file name on my various machines.
 * And beyond that, I'm just picking off words that come up experimentally.
 */
 
static int db_wordcloud_token_worth_keeping(struct db_wordcloud *wc,const char *src,int srcc) {
  if (srcc<3) return 0;
  char norm[8];
  if (srcc>(int)sizeof(norm)) return 1; // Anything longer than our normalization buffer must be ok.
  int i=srcc; while (i-->0) {
    if ((src[i]>='A')&&(src[i]<='Z')) norm[i]=src[i]+0x20;
    else norm[i]=src[i];
  }
  switch (srcc) {
    case 3: {
        if (!memcmp(norm,"and",3)) return 0;
        if (!memcmp(norm,"but",3)) return 0;
        if (!memcmp(norm,"for",3)) return 0;
        if (!memcmp(norm,"the",3)) return 0;
        if (!memcmp(norm,"you",3)) return 0;
        if (!memcmp(norm,"not",3)) return 0;
        if (!memcmp(norm,"rom",3)) return 0;
        if (!memcmp(norm,"one",3)) return 0;
        if (!memcmp(norm,"can",3)) return 0;
        if (!memcmp(norm,"don",3)) return 0; // "don't"
        if (!memcmp(norm,"won",3)) return 0;
      } break;
    case 4: {
        if (!memcmp(norm,"home",4)) return 0;
        if (!memcmp(norm,"andy",4)) return 0;
        if (!memcmp(norm,"with",4)) return 0;
        if (!memcmp(norm,"game",4)) return 0;
        if (!memcmp(norm,"this",4)) return 0;
        if (!memcmp(norm,"that",4)) return 0;
        if (!memcmp(norm,"than",4)) return 0;
        if (!memcmp(norm,"just",4)) return 0;
      } break;
    case 5: {
        if (!memcmp(norm,"kiddo",5)) return 0;
        if (!memcmp(norm,"games",5)) return 0;
      } break;
  }
  return 1;
}

/* Add entries from one string.
 * Tokenization is dead simple: [a-zA-Z0-9] are words, everything else is space.
 * In particular, underscore does not count as a letter. I tend to use it as a word break in file names.
 */
 
static int db_wordcloud_gather_from_string(struct db_wordcloud *wc,const char *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    const char *token=src+srcp;
    int tokenc=0;
    while (srcp<srcc) {
           if ((src[srcp]>='a')&&(src[srcp]<='z')) ;
      else if ((src[srcp]>='A')&&(src[srcp]<='Z')) ;
      else if ((src[srcp]>='0')&&(src[srcp]<='9')) ;
      else break;
      tokenc++;
      srcp++;
    }
    if (tokenc) {
      if (db_wordcloud_token_worth_keeping(wc,token,tokenc)) {
        int p=db_wordcloud_search(wc,token,tokenc);
        if (p>=0) {
          wc->entryv[p].instc++;
        } else {
          if (db_wordcloud_insert(wc,-p-1,token,tokenc)<0) return -1;
        }
      }
    } else {
      srcp++;
    }
  }
  return 0;
}

/* Gather strings.
 */

int db_wordcloud_gather(struct db_wordcloud *wc) {
  wc->entryc=0;
  if (!wc->db) return -1;
  
  /* We're not going to read the text dump straight.
   * We wouldn't know where the string boundaries are, and wouldn't know if a chunk of text has been orphaned.
   */
  const struct db_string_toc_entry *toc=wc->db->strings.toc;
  int i=wc->db->strings.tocc;
  for (;i-->0;toc++) {
    if (toc->p>wc->db->strings.textc-toc->c) continue; // Would be invalid and really not our problem, but hey let's be safe.
    int err=db_wordcloud_gather_from_string(wc,wc->db->strings.text+toc->p,toc->c);
    if (err<0) return err;
  }

  return 0;
}

/* Filter by count descending.
 */
 
static int db_wordcloud_ccmp(const void *a,const void *b) {
  const struct db_wordcloud_entry *A=a,*B=b;
  if (A->instc<B->instc) return 1;
  if (A->instc>B->instc) return -1;
  // Sort ties lexically.
  int cmpc=(A->vc<B->vc)?A->vc:B->vc;
  int cmp=sr_memcasecmp(A->v,B->v,cmpc);
  if (cmp) return cmp;
  if (A->vc>cmpc) return 1;
  if (B->vc>cmpc) return -1;
  return 0;
}

void db_wordcloud_filter_by_count(struct db_wordcloud *wc,int instc_min,int instc_max) {
  qsort(wc->entryv,wc->entryc,sizeof(struct db_wordcloud_entry),db_wordcloud_ccmp);
  // Once sorted it's simple: We're removing only from the head and tail.
  // This could be done even more efficiently with a binary search, but I'm sure it doesn't matter.
  while (wc->entryc&&(wc->entryv[wc->entryc-1].instc<instc_min)) wc->entryc--;
  int rmc=0;
  while ((rmc<wc->entryc)&&(wc->entryv[rmc].instc>instc_max)) rmc++;
  wc->entryc-=rmc;
  memmove(wc->entryv,wc->entryv+rmc,sizeof(struct db_wordcloud_entry)*wc->entryc);
}
