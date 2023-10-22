#include "db_internal.h"
#include "opt/serial/serial.h"

/* Represent/evaluate one flag.
 */
 
const char *db_flag_repr(uint32_t flag_1bit) {
  switch (flag_1bit) {
    #define _(tag) case DB_FLAG_##tag: return #tag;
    DB_FLAG_FOR_EACH
    #undef _
    case 0x00001000: return "flag12";
    case 0x00002000: return "flag13";
    case 0x00004000: return "flag14";
    case 0x00008000: return "flag15";
    case 0x00010000: return "flag16";
    case 0x00020000: return "flag17";
    case 0x00040000: return "flag18";
    case 0x00080000: return "flag19";
    case 0x00100000: return "flag20";
    case 0x00200000: return "flag21";
    case 0x00400000: return "flag22";
    case 0x00800000: return "flag23";
    case 0x01000000: return "flag24";
    case 0x02000000: return "flag25";
    case 0x04000000: return "flag26";
    case 0x08000000: return "flag27";
    case 0x10000000: return "flag28";
    case 0x20000000: return "flag29";
    case 0x40000000: return "flag30";
    case 0x80000000: return "flag31";
  }
  return "";
}

uint32_t db_flag_eval(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char norm[16];
  int normc=0,srcp=0;
  for (;srcp<srcc;srcp++) {
    if ((src[srcp]>='a')&&(src[srcp]<='z')) {
      norm[normc++]=src[srcp];
    } else if ((src[srcp]>='0')&&(src[srcp]<='9')) {
      norm[normc++]=src[srcp];
    } else if (src[srcp]=='_') {
      norm[normc++]=src[srcp];
    } else if ((src[srcp]>='A')&&(src[srcp]<='Z')) {
      norm[normc++]=src[srcp]+0x20;
    }
    if (normc>=sizeof(norm)) break;
  }
  #define _(tag) if ((normc==sizeof(#tag)-1)&&!memcmp(norm,#tag,normc)) return DB_FLAG_##tag;
  DB_FLAG_FOR_EACH
  #undef _
  if ((normc>=5)&&!memcmp(norm,"flag",4)) {
    int p=0;
    if (sr_int_eval(&p,norm+4,normc-4)>=2) {
      if ((p>=0)&&(p<32)) return 1<<p;
    }
  }
  // One last thing: It can be a raw integer, as long as it is exactly one bit.
  int n;
  if (sr_int_eval(&n,norm,normc)>=1) {
    if (n) {
      uint32_t bittest=n;
      while (!(bittest&1)) bittest>>=1;
      if (bittest==1) return n;
    }
  }
  return 0;
}

/* Represent multiple flags: space-delimited list.
 */

int db_flags_repr(char *dst,int dsta,uint32_t flags) {
  int dstc=0;
  if (flags) {
    uint32_t bit=1;
    for (;bit;bit<<=1) {
      if (!(flags&bit)) continue;
      const char *src=db_flag_repr(bit);
      if (src&&src[0]) {
        if (dstc) {
          if (dstc<dsta) dst[dstc]=' ';
          dstc++;
        }
        int srcc=0; while (src[srcc]) srcc++;
        if (dstc<=dsta-srcc) memcpy(dst+dstc,src,srcc);
        dstc+=srcc;
      }
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Evaluate multiple flags.
 */
 
int db_flags_eval(uint32_t *dst,const char *src,int srcc) {
  if (!src) { *dst=0; return 0; }
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { src++; srcc--; }
  if (!srcc) { *dst=0; return 0; }
  
  // Plain integers are allowed, and don't need to be just one bit.
  int n;
  if (sr_int_eval(&n,src,srcc)>=1) { *dst=n; return 0; }
  
  // If it's wrapped in "[]", assume it's a list of JSON strings.
  if ((srcc>=2)&&(src[0]=='[')&&(src[srcc-1]==']')) {
    struct sr_decoder decoder={.v=src,.c=srcc};
    if (sr_decode_json_array_start(&decoder)<0) return -1;
    *dst=0;
    while (sr_decode_json_next(0,&decoder)>0) {
      char token[32];
      int tokenc=sr_decode_json_string(token,sizeof(token),&decoder);
      if ((tokenc<0)||(tokenc>sizeof(token))) return -1;
      uint32_t bit=db_flag_eval(token,tokenc);
      if (!bit) return -1;
      (*dst)|=bit;
    }
    if (sr_decode_json_end(&decoder,0)<0) return -1;
    return 0;
  }
  
  // Otherwise, the typical case, space-delimited flag names. Or comma.
  *dst=0;
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if (src[srcp]==',') { srcp++; continue; }
    const char *token=src+srcp;
    int tokenc=0;
    while (srcp<srcc) {
      if ((unsigned char)src[srcp]<=0x20) break;
      if (src[srcp]==',') break;
      tokenc++;
      srcp++;
    }
    uint32_t bit=db_flag_eval(token,tokenc);
    if (!bit) return -1;
    (*dst)|=bit;
  }
  return 0;
}
