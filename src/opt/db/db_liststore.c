#include "db_internal.h"
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"
#include <errno.h>

/* Cleanup.
 */

void db_liststore_cleanup(struct db_liststore *store) {
  if (store->v) {
    while (store->c-->0) db_list_del(store->v[store->c]);
    free(store->v);
  }
}

/* Clear.
 */

void db_liststore_clear(struct db_liststore *store) {
  if (!store->c) return;
  while (store->c>0) {
    store->c--;
    db_list_del(store->v[store->c]);
  }
  store->contigc=0;
  store->dirty=1;
}

/* Remove one.
 */
 
void db_liststore_remove(struct db_liststore *store,int p) {
  if ((p<0)||(p>=store->c)) return;
  struct db_list *list=store->v[p];
  store->c--;
  memmove(store->v+p,store->v+p+1,sizeof(void*)*(store->c-p));
  db_list_del(list);
  store->dirty=1;
  if (p<store->contigc) store->contigc=p;
}

/* Decode.
 */

int db_liststore_decode(struct db_liststore *store,const void *src,int srcc) {
  if ((srcc<0)||(srcc&&!src)) return -1;
  uint32_t pvid=0;
  struct sr_decoder decoder={.v=src,.c=srcc};
  while (sr_decoder_remaining(&decoder)) {
  
    if (store->c>=store->a) {
      int na=store->a+32;
      if (na>INT_MAX/sizeof(void*)) return -1;
      void *nv=realloc(store->v,sizeof(void*)*na);
      if (!nv) return -1;
      store->v=nv;
      store->a=na;
    }
    struct db_list *list=calloc(1,sizeof(struct db_list));
    if (!list) return -1;
    store->v[store->c++]=list;
    
    uint32_t *idloc=0;
    if (sr_decode_raw(&idloc,&decoder,4)<0) return -1;
    list->listid=*idloc;
    if (list->listid<=pvid) {
      store->c--;
      free(list);
      return -1;
    }
    pvid=list->listid;
    
    uint32_t *hdr=0;
    if (sr_decode_raw(&hdr,&decoder,12)<0) return -1;
    list->name=hdr[0];
    list->desc=hdr[1];
    list->sorted=hdr[2]&0xff;
    int gamec=hdr[2]>>8;
    
    if (gamec) {
      if (sr_decoder_remaining(&decoder)<4*gamec) return -1;
      if (!(list->gameidv=malloc(4*gamec))) return -1;
      memcpy(list->gameidv,(char*)decoder.v+decoder.p,4*gamec);
      decoder.p+=4*gamec;
      list->gameidc=list->gameida=gamec;
    }
  }
  while ((store->contigc<store->c)&&(store->v[store->contigc]->listid==store->contigc+1)) store->contigc++;
  return 0;
}

/* Encode.
 */
 
int db_liststore_encode(struct sr_encoder *dst,const struct db_liststore *store) {
  struct db_list **p=store->v;
  int i=store->c;
  for (;i-->0;p++) {
    if (db_list_encode_binary(dst,*p)<0) return -1;
  }
  return 0;
}

int db_list_encode_binary(struct sr_encoder *dst,const struct db_list *list) {
  if (sr_encode_raw(dst,&list->listid,4)<0) return -1;
  if (sr_encode_raw(dst,&list->name,4)<0) return -1;
  if (sr_encode_raw(dst,&list->desc,4)<0) return -1;
  if (sr_encode_u8(dst,list->sorted)<0) return -1;
  if (sr_encode_intle(dst,list->gameidc,3)<0) return -1;
  if (sr_encode_raw(dst,list->gameidv,list->gameidc<<2)<0) return -1;
  return 0;
}

/* Load.
 */
 
int db_liststore_load(struct db_liststore *store,const char *root,int rootc) {
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s%clist",rootc,root,path_separator());
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  void *serial=0;
  int serialc=file_read(&serial,path);
  if (serialc<0) {
    if (errno==ENOENT) {
      store->c=0;
      store->contigc=0;
      store->dirty=0;
      return 0;
    }
    return -1;
  }
  store->c=0;
  store->contigc=0;
  int err=db_liststore_decode(store,serial,serialc);
  free(serial);
  if (err<0) return -1;
  store->dirty=0;
  return 0;
}

/* Save.
 */
 
int db_liststore_save(struct db_liststore *store,const char *root,int rootc) {
  if (!store->dirty) return 0;
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s%clist",rootc,root,path_separator());
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  struct sr_encoder encoder={0};
  if (db_liststore_encode(&encoder,store)<0) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  int err=file_write(path,encoder.v,encoder.c);
  sr_encoder_cleanup(&encoder);
  if (err<0) return -1;
  store->dirty=0;
  return 1;
}

/* Search.
 */
 
int db_liststore_search(const struct db_liststore *store,uint32_t listid) {
  if (!listid) return -1;
  if (listid<=store->contigc) return listid-1;
  int lo=store->contigc,hi=store->c;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    uint32_t q=store->v[ck]->listid;
         if (listid<q) hi=ck;
    else if (listid>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Insert.
 */
 
struct db_list *db_liststore_insert(struct db_liststore *store,int p,uint32_t listid) {
  if (!listid||(p<0)||(p>store->c)) return 0;
  if (p&&(listid<=store->v[p-1]->listid)) return 0;
  if ((p<store->c)&&(listid>=store->v[p]->listid)) return 0;
  if (store->c>=store->a) {
    int na=store->a+32;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(store->v,sizeof(void*)*na);
    if (!nv) return 0;
    store->v=nv;
    store->a=na;
  }
  struct db_list *list=calloc(1,sizeof(struct db_list));
  if (!list) return 0;
  list->listid=listid;
  memmove(store->v+p+1,store->v+p,sizeof(void*)*(store->c-p));
  store->c++;
  store->v[p]=list;
  while ((store->contigc<store->c)&&(store->v[store->contigc]->listid==store->contigc+1)) store->contigc++;
  store->dirty=1;
  return list;
}

/* Lowest unused ID.
 */
 
uint32_t db_liststore_next_id(const struct db_liststore *store) {
  return store->contigc+1;
}
