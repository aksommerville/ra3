#include "eh_internal.h"
#include "opt/fs/fs.h"

/* Scratch directory.
 */
 
int eh_get_scratch_directory(char **dstpp) {
  char tmp[1024];
  int tmpc=eh_get_romassist_directory(tmp,sizeof(tmp));
  if ((tmpc<1)||(tmpc>=sizeof(tmp))) return -1;
  if (dir_mkdirp(tmp)<0) return -1;
  char *dst=malloc(tmpc+1);
  memcpy(dst,tmp,tmpc+1);
  *dstpp=dst;
  return tmpc;
}

/* Romassist root directory.
 */
 
int eh_get_romassist_directory(char *dst,int dsta) {
  if (!dst||(dsta<0)) dsta=0;
  const char *src;
  int dstc=0;
  
  // First, env "HOME".
  if (src=getenv("HOME")) {
    int srcc=0; while (src[srcc]) srcc++;
    if (srcc>0) {
      if (srcc<=dsta) memcpy(dst,src,srcc);
      dstc=srcc;
    }
  }
  
  // If that didn't work, but env "USER" is present, try guessing the home path.
  if (!dstc&&(src=getenv("USER"))) {
    int srcc=0; while (src[srcc]) srcc++;
    if (srcc>0) {
      const char *pfx=0;
      #if USE_linux
        pfx="/home/";
      #elif USE_macos
        pfx="/Users/";
      #elif USE_mswin
        pfx="C:/Users/";
      #endif
      if (pfx) {
        int pfxc=0; while (pfx[pfxc]) pfxc++;
        dstc=pfxc+srcc;
        if (dstc<=dsta) {
          memcpy(dst,pfx,pfxc);
          memcpy(dst+pfxc,src,srcc);
        }
      }
    }
  }
  
  // Still no? OK forget it.
  if (dstc<1) return -1;
  
  // Append separator and our own name.
  if ((dstc<dsta)&&(dst[dstc-1]!='/')) dst[dstc++]='/';
  if (dstc<=dsta-10) memcpy(dst+dstc,".romassist",10);
  dstc+=10;
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}
