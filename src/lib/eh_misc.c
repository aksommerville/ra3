#include "eh_internal.h"
#include "opt/fs/fs.h"

/* Scratch directory.
 */
 
int eh_get_scratch_directory(char **dstpp) {
  
  /* "~/.romassist/EMUNAME"
   */
  const char *HOME=getenv("HOME");
  if (!HOME) return -1;
  
  int tmpa=1024,tmpc=0;
  char *tmp=malloc(tmpa);
  if (!tmp) return -1;
  while (1) {
    tmpc=snprintf(tmp,tmpa,"%s/.romassist/%s",HOME,eh.delegate.name);
    if (tmpc<1) {
      free(tmp);
      return -1;
    }
    if (tmpc<tmpa) break;
    char *nv=realloc(tmp,tmpa=tmpc+1);
    if (!nv) {
      free(tmp);
      return -1;
    }
  }
  if (dir_mkdirp(tmp)<0) {
    free(tmp);
    return -1;
  }
  *dstpp=tmp;
  return tmpc;
}
