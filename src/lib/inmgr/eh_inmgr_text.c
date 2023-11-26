#include "eh_inmgr_internal.h"
#include "opt/serial/serial.h"

#define EH_FOR_EACH_NAMED_BUTTON \
  _(NONE) \
  _(LEFT) \
  _(RIGHT) \
  _(UP) \
  _(DOWN) \
  _(SOUTH) \
  _(WEST) \
  _(EAST) \
  _(NORTH) \
  _(AUX1) \
  _(AUX2) \
  _(AUX3) \
  _(L1) \
  _(R1) \
  _(L2) \
  _(R2) \
  _(HORZ) \
  _(VERT) \
  _(DPAD) \
  _(QUIT) \
  _(SCREENCAP) \
  _(FULLSCREEN) \
  _(PAUSE) \
  _(DEBUG) \
  _(STEP) \
  _(FASTFWD) \
  _(SAVESTATE) \
  _(LOADSTATE)

/* Btnid eval.
 */
 
int eh_btnid_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) return -1;
  
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!sr_memcasecmp(src,#tag,srcc)) return EH_BTN_##tag;
  EH_FOR_EACH_NAMED_BUTTON
  #undef _
  
  /* A few button names changed from v2 to v3. We accept both (but beware, we produce our own v3 names).
   */
  if ((srcc==1)&&!sr_memcasecmp(src,"A",1)) return EH_BTN_SOUTH;
  if ((srcc==1)&&!sr_memcasecmp(src,"B",1)) return EH_BTN_WEST;
  if ((srcc==1)&&!sr_memcasecmp(src,"C",1)) return EH_BTN_EAST;
  if ((srcc==1)&&!sr_memcasecmp(src,"D",1)) return EH_BTN_NORTH;
  if ((srcc==1)&&!sr_memcasecmp(src,"L",1)) return EH_BTN_L1;
  if ((srcc==1)&&!sr_memcasecmp(src,"R",1)) return EH_BTN_R1;
  
  int n;
  if (sr_int_eval(&n,src,srcc)>=2) return n;
  
  return -1;
}

/* Btnid repr.
 */
 
int eh_btnid_repr(char *dst,int dsta,int btnid) {
  if (!dst||(dsta<0)) dsta=0;
  switch (btnid) {
    #define _(tag) case EH_BTN_##tag: { \
      int dstc=sizeof(#tag)-1; \
      if (dstc<dsta) memcpy(dst,#tag,dstc+1); \
      else if (dstc<=dsta) memcpy(dst,#tag,dstc); \
      return dstc; \
    }
    EH_FOR_EACH_NAMED_BUTTON
    #undef _
  }
  return sr_hexuint_repr_prefixed(dst,dsta,btnid,0);
}
 
const char *eh_btnid_repr_canned(int btnid) {
  switch (btnid) {
    #define _(tag) case EH_BTN_##tag: return #tag;
    EH_FOR_EACH_NAMED_BUTTON
    #undef _
  }
  return 0;
}
 
const char *eh_btnid_repr_by_index(int p) {
  if (p<0) return 0;
  #define _(tag) if (!p--) return #tag;
  EH_FOR_EACH_NAMED_BUTTON
  #undef _
  return 0;
}

int eh_btnid_by_index(int p) {
  if (p<0) return 0;
  #define _(tag) if (!p--) return EH_BTN_##tag;
  EH_FOR_EACH_NAMED_BUTTON
  #undef _
  return 0;
}

int eh_index_by_btnid(int btnid) {
  // I wish this could be done as a switch, maybe using __LINE__ or something? Doesn't appear to be possible.
  int p=0;
  #define _(tag) if (btnid==EH_BTN_##tag) return p; p++;
  EH_FOR_EACH_NAMED_BUTTON
  #undef _
  return 0;
}
