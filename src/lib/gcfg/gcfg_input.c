#include "gcfg.h"
#include <string.h>
#include <limits.h>
#include <stdlib.h>

/* Skip buttons and return next device header.
 */
 
int gcfg_input_next_device(struct gcfg_input_device_header *dst,struct gcfg_input_reader *reader) {
  for (;;) {
    if (reader->p>=reader->c) return 0;
    const char *line=reader->v+reader->p;
    int linec=0;
    while ((reader->p<reader->c)&&(reader->v[reader->p++]!=0x0a)) linec++;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec||(line[0]=='#')) continue;
    
    // Not a device block? Skip it.
    if ((linec<3)||memcmp(line,">>>",3)) continue;
    
    if (gcfg_input_device_header_eval(dst,line,linec)<0) return -1;
    return 1;
  }
}

/* Return next button definition.
 */
 
int gcfg_input_next_button(struct gcfg_input_button *dst,struct gcfg_input_reader *reader) {
  for (;;) {
    if (reader->p>=reader->c) return 0;
    const char *line=reader->v+reader->p;
    int linec=0;
    int p0=reader->p;
    while ((reader->p<reader->c)&&(reader->v[reader->p++]!=0x0a)) linec++;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec||(line[0]=='#')) continue;
    
    // Device block? Unread it and call it EOF.
    if ((linec>=3)&&!memcmp(line,">>>",3)) {
      reader->p=p0;
      return 0;
    }
    
    if (gcfg_input_button_eval(dst,line,linec)<0) return -1;
    return 1;
  }
}

/* Produce device header.
 */

int gcfg_input_device_header_repr(char *dst,int dsta,const struct gcfg_input_device_header *src) {
  if (!src) return -1;
  int dstc=0,err,v;
  if (dstc<=dsta-3) memcpy(dst+dstc,">>>",3); dstc+=3;
  if (src->vid||src->pid||src->version) {
    if (dstc<dsta) dst[dstc]=' '; dstc++;
    dstc+=gcfg_hexuint_repr(dst+dstc,dsta-dstc,src->vid);
  }
  if (src->pid||src->version) {
    if (dstc<dsta) dst[dstc]=' '; dstc++;
    dstc+=gcfg_hexuint_repr(dst+dstc,dsta-dstc,src->pid);
  }
  if (src->version) {
    if (dstc<dsta) dst[dstc]=' '; dstc++;
    dstc+=gcfg_hexuint_repr(dst+dstc,dsta-dstc,src->version);
  }
  if (src->namec>0) {
    if (dstc<dsta) dst[dstc]=' '; dstc++;
    if (dstc<dsta) dst[dstc]='"'; dstc++;
    if (dstc<=dsta-src->namec) memcpy(dst+dstc,src->name,src->namec); dstc+=src->namec;
    if (dstc<dsta) dst[dstc]='"'; dstc++;
  }
  if (dstc<dsta) dst[dstc]=0x0a; dstc++;
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Read device header.
 */
 
int gcfg_input_device_header_eval(struct gcfg_input_device_header *dst,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0,tokenc;
  const char *token;
  memset(dst,0,sizeof(struct gcfg_input_device_header));
  
  // Introducer.
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if ((srcp>srcc-3)||memcmp(src+srcp,">>>",3)) return -1;
  srcp+=3;
  
  // vid, pid, version.
  dst->vid=dst->pid=dst->version=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&(src[srcp]!='"')&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  if (tokenc&&(gcfg_hexuint_eval(&dst->vid,token,tokenc)<0)) return -1;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&(src[srcp]!='"')&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  if (tokenc&&(gcfg_hexuint_eval(&dst->pid,token,tokenc)<0)) return -1;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&(src[srcp]!='"')&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  if (tokenc&&(gcfg_hexuint_eval(&dst->version,token,tokenc)<0)) return -1;
  
  // name
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if ((srcp<srcc)&&(src[srcp]=='"')) {
    srcp++;
    dst->name=src+srcp;
    dst->namec=0;
    for (;;) {
      if (srcp>=srcc) return -1;
      if (src[srcp++]=='"') break;
      dst->namec++;
    }
  } else {
    dst->name=0;
    dst->namec=0;
  }
  
  // Assert completion.
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp<srcc) return -1;
  return 0;
}

/* Produce button definition.
 */
 
int gcfg_input_button_repr(char *dst,int dsta,const struct gcfg_input_button *src) {
  if (!src) return -1;
  int dstc=0;
  dstc+=gcfg_hexuint_repr(dst+dstc,dsta-dstc,src->srcbtnid);
  if (dstc<dsta) dst[dstc]=' '; dstc++;
  if (src->btnid>=0) {
    dstc+=gcfg_btnid_repr(dst+dstc,dsta-dstc,src->btnid);
  } else if (src->btnnamec>0) {
    if (dstc<=dsta-src->btnnamec) memcpy(dst+dstc,src->btnname,src->btnnamec);
    dstc+=src->btnnamec;
  } else {
    return -1;
  }
  if (src->namec>0) {
    if (dstc<dsta) dst[dstc]=' '; dstc++;
    if (dstc<dsta) dst[dstc]='"'; dstc++;
    if (dstc<=dsta-src->namec) memcpy(dst+dstc,src->name,src->namec); dstc+=src->namec;
    if (dstc<dsta) dst[dstc]='"'; dstc++;
  }
  if (src->commentc>0) {
    if (dstc<dsta) dst[dstc]=' '; dstc++;
    if (dstc<=dsta-src->commentc) memcpy(dst+dstc,src->comment,src->commentc); dstc+=src->commentc;
  }
  if (dstc<dsta) dst[dstc]=0x0a; dstc++;
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Read button definition.
 */

int gcfg_input_button_eval(struct gcfg_input_button *dst,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0,tokenc;
  const char *token;
  memset(dst,0,sizeof(struct gcfg_input_button));
  
  // srcbtnid
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  if (gcfg_hexuint_eval(&dst->srcbtnid,token,tokenc)<0) return -1;
  
  // btnid
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  if (gcfg_btnid_eval(&dst->btnid,token,tokenc)<0) dst->btnid=-1; // This is allowed to be unknown.
  dst->btnname=token;
  dst->btnnamec=tokenc;
  
  // name
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if ((srcp<srcc)&&(src[srcp]=='"')) {
    srcp++;
    dst->name=src+srcp;
    dst->namec=0;
    for (;;) {
      if (srcp>=srcc) return -1;
      if (src[srcp++]=='"') break;
      dst->namec++;
    }
  } else {
    dst->name=0;
    dst->namec=0;
  }
  
  // comment
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  dst->comment=src+srcp;
  dst->commentc=srcc-srcp;
  while (dst->commentc&&((unsigned char)dst->comment[dst->commentc-1]<=0x20)) dst->commentc--;
  
  return 0;
}

/* Primitive tokens.
 */
 
int gcfg_hexuint_repr(char *dst,int dsta,int src) {
  unsigned int mask=~0xf;
  int dstc=1;
  while (src&mask) { dstc++; mask<<=4; }
  if (dstc>dsta) return dstc;
  int i=dstc;
  for (;i-->0;src>>=4) dst[i]="0123456789abcdef"[src&15];
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

int gcfg_btnid_repr(char *dst,int dsta,int src) {
  const char *sym=0;
  int symc=0;
  switch (src) {
    #define _(tag) case GCFG_BTNID_##tag: sym=#tag; symc=sizeof(#tag)-1; break;
    GCFG_FOR_EACH_BTNID
    #undef _
  }
  if (sym) {
    if (symc<=dsta) {
      memcpy(dst,sym,symc);
      if (symc<dsta) dst[symc]=0;
    }
    return symc;
  }
  return gcfg_hexuint_repr(dst,dsta,src);
}

int gcfg_hexuint_eval(int *dst,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if ((srcc>2)&&(src[0]=='0')&&((src[1]=='x')||(src[1]=='X'))) { src+=2; srcc-=2; }
  *dst=0;
  for (;srcc-->0;src++) {
    int digit;
         if ((*src>='0')&&(*src<='9')) digit=*src-'0';
    else if ((*src>='a')&&(*src<='f')) digit=*src-'a'+10;
    else if ((*src>='A')&&(*src<='F')) digit=*src-'A'+10;
    else return -1;
    if ((*dst)&~(UINT_MAX>>4)) return -1;
    (*dst)<<=4;
    (*dst)|=digit;
  }
  return 0;
}

int gcfg_btnid_eval(int *dst,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(src,#tag,srcc)) { *dst=GCFG_BTNID_##tag; return 0; }
  GCFG_FOR_EACH_BTNID
  #undef _
  return gcfg_hexuint_eval(dst,src,srcc);
}

/* Input manifold cleanup.
 */
 
void gcfg_input_del(struct gcfg_input *input) {
  if (!input) return;
  if (input->src) free(input->src);
  if (input->devicev) {
    while (input->devicec-->0) gcfg_input_device_cleanup(input->devicev+input->devicec);
    free(input->devicev);
  }
  free(input);
}

/* Input device cleanup.
 */
 
void gcfg_input_device_cleanup(struct gcfg_input_device *device) {
  if (!device) return;
  if (device->buttonv) free(device->buttonv);
}

/* Add device to manifold.
 */
 
struct gcfg_input_device *gcfg_input_add_device(struct gcfg_input *input) {
  if (!input) return 0;
  if (input->devicec>=input->devicea) {
    int na=input->devicea+16;
    if (na>INT_MAX/sizeof(struct gcfg_input_device)) return 0;
    void *nv=realloc(input->devicev,sizeof(struct gcfg_input_device)*na);
    if (!nv) return 0;
    input->devicev=nv;
    input->devicea=na;
  }
  struct gcfg_input_device *device=input->devicev+input->devicec++;
  memset(device,0,sizeof(struct gcfg_input_device));
  return device;
}

/* Search devices in manifold.
 */
 
struct gcfg_input_device *gcfg_input_get_device(const struct gcfg_input *input,int vid,int pid,int version,const char *name,int namec) {
  if (!input) return 0;
  if (!name) namec=0; else if (namec<0) { namec=0; while (name[namec]) namec++; }
  struct gcfg_input_device *device=input->devicev;
  int i=input->devicec;
  for (;i-->0;device++) {
    if (device->header.vid&&(vid!=device->header.vid)) continue;
    if (device->header.pid&&(pid!=device->header.pid)) continue;
    if (device->header.version&&(version!=device->header.version)) continue;
    if (device->header.namec&&((device->header.namec!=namec)||memcmp(device->header.name,name,namec))) continue;
    return device;
  }
  return 0;
}

/* Get or add button to device.
 */
 
struct gcfg_input_button *gcfg_input_device_get_button(struct gcfg_input_device *device,int srcbtnid,int add_if_missing) {
  if (!device) return 0;
  int lo=0,hi=device->buttonc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    struct gcfg_input_button *q=device->buttonv+ck;
         if (srcbtnid<q->srcbtnid) hi=ck;
    else if (srcbtnid>q->srcbtnid) lo=ck+1;
    else return q;
  }
  if (!add_if_missing) return 0;
  if (device->buttonc>=device->buttona) {
    int na=device->buttona+16;
    if (na>INT_MAX/sizeof(struct gcfg_input_button)) return 0;
    void *nv=realloc(device->buttonv,sizeof(struct gcfg_input_button)*na);
    if (!nv) return 0;
    device->buttonv=nv;
    device->buttona=na;
  }
  struct gcfg_input_button *button=device->buttonv+lo;
  memmove(button+1,button,sizeof(struct gcfg_input_button)*(device->buttonc-lo));
  device->buttonc++;
  memset(button,0,sizeof(struct gcfg_input_button));
  button->srcbtnid=srcbtnid;
  return button;
}

/* New manifold from the global config, or empty.
 */

struct gcfg_input *gcfg_input_get() {
  struct gcfg_input *input=calloc(1,sizeof(struct gcfg_input));
  if (!input) return 0;
  if ((input->srcc=gcfg_read_file(&input->src,0,0,"input",5))<0) input->srcc=0;
  int mapid_next=1;
  struct gcfg_input_reader reader={.v=input->src,.c=input->srcc};
  struct gcfg_input_device_header header;
  while (gcfg_input_next_device(&header,&reader)>0) {
    struct gcfg_input_device *device=gcfg_input_add_device(input);
    if (!device) break;
    device->mapid=mapid_next++;
    device->header=header;
    struct gcfg_input_button button;
    while (gcfg_input_next_button(&button,&reader)>0) {
      struct gcfg_input_button *dst=gcfg_input_device_get_button(device,button.srcbtnid,1);
      if (!dst) continue;
      *dst=button;
    }
  }
  return input;
}
