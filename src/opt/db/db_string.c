#include "db_internal.h"
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"
#include <errno.h>

/* Cleanup store.
 */
 
void db_stringstore_cleanup(struct db_stringstore *store) {
  if (store->toc) free(store->toc);
  if (store->text) free(store->text);
}

/* Intern a chunk of raw text to (store->text).
 */
 
static int db_stringstore_text_intern(struct db_stringstore *store,const char *src,int srcc) {
  if (srcc<1) return 0;
  
  /* Exact matches of a full existing string have already been checked for (and that's a firm requirement).
   * We can go a little further and look for matches against portions of existing text.
   * ie if (src) exists anywhere in (store->text), we can point to that.
   * If an existing comment says "Double Dragon rules!" and a new comment is "Dragon rules!", we don't need to append any text.
   * This amounts to a huge amount of searching.
   * TODO Can I get some numbers around cost and benefit of this?
   */
  if (1) {
    int i=store->textc-srcc+1;
    const char *p=store->text;
    for (;i-->0;p++) {
      if (!memcmp(p,src,srcc)) return p-store->text;
    }
  }
  
  int p=store->textc;
  if (p>INT_MAX-srcc) return -1;
  int na=p+srcc;
  if (na>store->texta) {
    if (na<INT_MAX-1024) na=(na+1024)&~1023;
    void *nv=realloc(store->text,na);
    if (!nv) return -1;
    store->text=nv;
    store->texta=na;
  }
  memcpy(store->text+p,src,srcc);
  store->textc+=srcc;
  store->dirty=1;
  return 0;
}

/* Search TOC for exact text.
 * (-p-1) if not found, and this is either (tocc) or a vacant slot.
 */
 
static int db_stringstore_search(const struct db_stringstore *store,const char *src,int srcc) {
  const struct db_string_toc_entry *entry=store->toc;
  int p=0,i=store->tocc;
  int vacantp=store->tocc;
  for (;i-->0;p++,entry++) {
    if (!entry->c) {
      if (vacantp>p) vacantp=p;
      continue;
    }
    if (entry->c!=srcc) continue;
    if (entry->p>UINT32_MAX-entry->c) continue; //TODO aggressive validation, is this too much?
    if (entry->p+entry->c>store->textc) continue;
    if (memcmp(src,store->text+entry->p,srcc)) continue;
    return p;
  }
  return -vacantp-1;
}

/* Intern.
 */
 
uint32_t db_string_intern(struct db *db,const char *src,int srcc) {
  if (!db) return 0;
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) return 0;
  int p=db_stringstore_search(&db->strings,src,srcc);
  if (p>=0) return 1+p;
  p=-p-1;
  if (p>db->strings.tocc) return 0;
  int textp=db_stringstore_text_intern(&db->strings,src,srcc);
  if (textp<0) return 0;
  if (p==db->strings.tocc) {
    int na=db->strings.tocc+1;
    if (na>db->strings.toca) {
      if (na<INT_MAX-256) na=(na+256)&~255;
      if (na>INT_MAX/sizeof(struct db_string_toc_entry)) return 0;
      void *nv=realloc(db->strings.toc,sizeof(struct db_string_toc_entry)*na);
      if (!nv) return 0;
      db->strings.toc=nv;
      db->strings.toca=na;
    }
    db->strings.tocc++;
  }
  struct db_string_toc_entry *entry=db->strings.toc+p;
  entry->p=textp;
  entry->c=srcc;
  db->strings.dirty=1;
  db->dirty=1;
  return 1+p;
}

/* Lookup.
 */

uint32_t db_string_lookup(const struct db *db,const char *src,int srcc) {
  if (!db||!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) return 0;
  int p=db_stringstore_search(&db->strings,src,srcc);
  if (p<0) return 0;
  return 1+p;
}

/* Get text for stringid.
 */
 
int db_string_get(void *dstpp,const struct db *db,uint32_t stringid) {
  if (!db||!stringid) {
    return 0;
  }
  stringid--;
  if (stringid>=db->strings.tocc) return 0;
  const struct db_string_toc_entry *entry=db->strings.toc+stringid;
  if (entry->p>UINT32_MAX-entry->c) return 0;
  if (entry->p+entry->c>db->strings.textc) return 0;
  if (dstpp) *(void**)dstpp=db->strings.text+entry->p;
  return entry->c;
}

/* Clear.
 */
 
void db_stringstore_clear(struct db_stringstore *store) {
  if (!store) return;
  if (!store->tocc&&!store->textc) return;
  store->tocc=0;
  store->textc=0;
  store->dirty=1;
}

/* Convenience: Encode a string as JSON, from stringid.
 */
 
int db_encode_json_string(struct sr_encoder *dst,const struct db *db,const char *k,int kc,uint32_t stringid) {
  const char *src=0;
  int srcc=db_string_get(&src,db,stringid);
  return sr_encode_json_string(dst,k,kc,src,srcc);
}

int db_decode_json_string(uint32_t *stringid,struct db *db,struct sr_decoder *src) {
  char tmp[256];
  char *text=tmp;
  int tmpc=sr_decode_json_string(tmp,sizeof(tmp),src);
  if (tmpc<0) return -1;
  if (tmpc>sizeof(tmp)) {
    if (!(text=malloc(tmpc+8))) return -1;
    int tmpc2=sr_decode_json_string(text,tmpc+8,src);
    if (tmpc2>tmpc+8) {
      free(text);
      return -1;
    }
    tmpc=tmpc2;
  }
  if (tmpc) {
    if (!(*stringid=db_string_intern(db,text,tmpc))) {
      if (text!=tmp) free(text);
      return -1;
    }
  } else {
    *stringid=0;
  }
  if (text!=tmp) free(text);
  return 0;
}

/* Load.
 */
 
int db_stringstore_load(struct db_stringstore *store,const char *root,int rootc) {
  
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s%cstoc",rootc,root,path_separator());
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  void *nv=0;
  int nc=file_read(&nv,path);
  if (nc<0) {
    if (errno==ENOENT) {
      store->tocc=0;
      store->textc=0;
      store->dirty=0;
      return 0;
    }
    return -1;
  }
  if (store->toc) free(store->toc);
  store->toc=nv;
  store->tocc=nc/sizeof(struct db_string_toc_entry);
  store->toca=store->tocc;
  
  memcpy(path+pathc-4,"text",4);
  if ((nc=file_read(&nv,path))<0) {
    store->tocc=0;
    if (errno==ENOENT) {
      store->textc=0;
      store->dirty=0;
      return 0;
    }
    return -1;
  }
  if (store->text) free(store->text);
  store->text=nv;
  store->textc=nc;
  store->texta=nc;
  
  store->dirty=0;
  return 0;
}

/* Save.
 */
 
int db_stringstore_save(struct db_stringstore *store,const char *root,int rootc) {
  if (!store->dirty) return 0;
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s%cstoc",rootc,root,path_separator());
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  if (file_write(path,store->toc,store->tocc*sizeof(struct db_string_toc_entry))<0) return -1;
  memcpy(path+pathc-4,"text",4);
  if (file_write(path,store->text,store->textc)<0) return -1;
  store->dirty=0;
  return 1;
}
