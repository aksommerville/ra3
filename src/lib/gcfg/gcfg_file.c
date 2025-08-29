#include "gcfg.h"
#include "opt/fs/fs.h"
#include <stdlib.h>
#include <string.h>

/* Compose path.
 */
 
int gcfg_compose_path(char *dst,int dsta,const char *app,int appc,const char *base,int basec) {
  if (!dst||(dsta<0)) dsta=0;
  if (!app) appc=0; else if (appc<0) { appc=0; while (app[appc]) appc++; }
  if (!base) basec=0; else if (basec<0) { basec=0; while (base[basec]) basec++; }
  while (appc&&(app[0]=='/')) { app++; appc--; }
  while (appc&&(app[appc-1]=='/')) appc--;
  while (basec&&(base[0]=='/')) { base++; basec--; }
  while (basec&&(base[basec-1]=='/')) basec--;
  
  /* Path begins "~/.config/aksomm/".
   * The ".config" is used by other Linux apps, maybe other OSes too.
   * If nothing else on the host uses it, whatever, no harm done.
   * All my games should get lumped together, that's what "aksomm" is for.
   */
  int dstc=0;
  const char *src;
  if ((src=getenv("HOME"))&&src[0]) {
    int srcc=0;
    while (src[srcc]) srcc++;
    if (dstc>=dsta-srcc) return -1;
    memcpy(dst+dstc,src,srcc);
    dstc+=srcc;
  } else if ((src=getenv("USER"))&&src[0]) {
    int srcc=0;
    while (src[srcc]) srcc++;
    if (dstc>=dsta-6) return -1;
    memcpy(dst+dstc,"/home/",6);
    dstc+=6;
    if (dstc>=dsta-srcc) return -1;
    memcpy(dst+dstc,src,srcc);
    dstc+=srcc;
  } else {
    return -1;
  }
  if (dstc&&(dst[dstc-1]!='/')) dst[dstc++]='/';
  if (dstc>=dsta-15) return -1;
  memcpy(dst+dstc,".config/aksomm/",15);
  dstc+=15;
  
  if (appc) {
    if (dstc>=dsta-appc) return -1;
    memcpy(dst+dstc,app,appc);
    dstc+=appc;
    if (dstc&&(dst[dstc-1]!='/')) dst[dstc++]='/';
  }
  
  if (basec) {
    if (dstc>=dsta-basec) return -1;
    memcpy(dst+dstc,base,basec);
    dstc+=basec;
  }
  
  if (dstc>=dsta) return -1;
  dst[dstc]=0;
  return dstc;
}

/* Read file.
 */

int gcfg_read_file(void *dstpp,const char *app,int appc,const char *base,int basec) {
  char path[1024];
  int pathc=gcfg_compose_path(path,sizeof(path),app,appc,base,basec);
  if ((pathc<1)||(pathc>=sizeof(path))) return 0;
  void *serial=0;
  int serialc=file_read(&serial,path);
  if (serialc<0) return 0;
  if (!serialc) {
    if (serial) free(serial);
    return 0;
  }
  *(void**)dstpp=serial;
  return serialc;
}

/* Write file.
 */

int gcfg_write_file(const char *app,int appc,const char *base,int basec,const void *src,int srcc) {
  char path[1024];
  int pathc=gcfg_compose_path(path,sizeof(path),app,appc,base,basec);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  if (dir_mkdirp_parent(path)<0) return -1;
  return file_write(path,src,srcc);
}
