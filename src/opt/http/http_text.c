#include "http_internal.h"

/* Hex digit.
 */
 
static int http_hexdigit_eval(char src) {
  if ((src>='0')&&(src<='9')) return src-'0';
  if ((src>='a')&&(src<='f')) return src-'a'+10;
  if ((src>='A')&&(src<='F')) return src-'A'+10;
  return -1;
}

/* Method.
 */
 
const char *http_method_repr(int method) {
  switch (method) {
    #define _(tag) case HTTP_METHOD_##tag: return #tag;
    _(GET)
    _(POST)
    _(PUT)
    _(PATCH)
    _(DELETE)
    _(HEAD)
    _(UNKNOWN)
    _(WEBSOCKET)
    #undef _
  }
  return "?";
}

int http_method_eval(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) return 0;
  char norm[16];
  if (srcc>sizeof(norm)) return HTTP_METHOD_UNKNOWN;
  int i=srcc; while (i-->0) {
    if ((src[i]>='a')&&(src[i]<='z')) norm[i]=src[i]-0x20;
    else norm[i]=src[i];
  }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(norm,#tag,srcc)) return HTTP_METHOD_##tag;
  _(GET)
  _(POST)
  _(PUT)
  _(PATCH)
  _(DELETE)
  _(HEAD)
  _(UNKNOWN)
  _(WEBSOCKET)
  #undef _
  if ((srcc==13)&&!memcmp(norm,"FAKEWEBSOCKET",13)) return HTTP_METHOD_WEBSOCKET;
  return HTTP_METHOD_UNKNOWN;
}

/* URL encoding.
 */
 
int http_url_encode(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc=0,srcp=0;
  for (;srcp<srcc;srcp++) {
    uint8_t ch=src[srcp];
    if (
      (ch<=0x20)||
      (ch>0x7e)||
      (ch=='/')||
      (ch=='?')||
      (ch=='=')||
      (ch=='&')||
      (ch=='#')
    ) {
      if (dstc<=dsta-3) {
        dst[dstc++]='%';
        dst[dstc++]="0123456789abcdef"[ch>>4];
        dst[dstc++]="0123456789abcdef"[ch&15];
      } else dstc+=3;
    } else {
      if (dstc<dsta) dst[dsta]=ch;
      dstc++;
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

int http_url_decode(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc=0,srcp=0;
  while (srcp<srcc) {
    if ((src[srcp]=='%')&&(srcp<=srcc-3)) {
      int hi=http_hexdigit_eval(src[srcp+1]);
      int lo=http_hexdigit_eval(src[srcp+2]);
      if ((hi>=0)&&(lo>=0)) {
        if (dstc<dsta) dst[dstc]=(hi<<4)|lo;
        dstc++;
        srcp+=3;
      } else {
        if (dstc<dsta) dst[dstc]='%';
        dstc++;
        srcp++;
      }
    } else {
      if (dstc<dsta) dst[dstc]=src[srcp];
      dstc++;
      srcp++;
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Integers.
 */
 
int http_int_eval(int *dst,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return -1;
  int srcp=0,positive=1;
  if (src[srcp]=='-') {
    positive=0;
    if (++srcp>=srcc) return -1;
  } else if (src[srcp]=='+') {
    if (++srcp>=srcc) return -1;
  }
  *dst=0;
  for (;srcp<srcc;srcp++) {
    int digit=src[srcp]-'0';
    if ((digit<0)||(digit>9)) return -1;
    if (positive) {
      if (*dst>INT_MAX/10) return -1;
      (*dst)*=10;
      if (*dst>INT_MAX-digit) return -1;
      (*dst)+=digit;
    } else {
      if (*dst<INT_MIN/10) return -1;
      (*dst)*=10;
      if (*dst<INT_MIN+digit) return -1;
      (*dst)-=digit;
    }
  }
  return 0;
}

/* Case-insensitive memcmp.
 */
 
int http_memcasecmp(const void *a,const void *b,int c) {
  if (c<1) return 0;
  if (a==b) return 0;
  if (!a) return -1;
  if (!b) return 1;
  const uint8_t *A=a,*B=b;
  for (;c-->0;A++,B++) {
    uint8_t cha=*A; if ((cha>=0x41)&&(cha<=0x5a)) cha+=0x20;
    uint8_t chb=*B; if ((chb>=0x41)&&(chb<=0x5a)) chb+=0x20;
    if (cha<chb) return -1;
    if (cha>chb) return 1;
  }
  return 0;
}

/* Measure text.
 */
 
int http_measure_space(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  return srcp;
}

int http_measure_line(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  srcc--; // so we can always read one beyond it
  int srcp=0;
  while (srcp<srcc) {
    if ((src[srcp]==0x0d)&&(src[srcp+1]==0x0a)) return srcp+2;
    srcp++;
  }
  return 0;
}

/* Wildcard match.
 */
 
int http_wildcard_match(const char *pat,int patc,const char *src,int srcc) {
  if (!pat) patc=0; else if (patc<0) { patc=0; while (pat[patc]) patc++; }
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int patp=0,srcp=0;
  while (1) {
    
    if (patp>=patc) {
      if (srcp>=srcc) return 1;
      return 0;
    }
    if (pat[patp]=='*') {
      patp++;
      if ((patp<patc)&&(pat[patp]=='*')) { // double star: any amount of anything.
        patp++;
        if (patp>=patc) return 1;
        while (srcp<srcc) {
          if (http_wildcard_match(pat+patp,patc-patp,src+srcp,srcc-srcp)) return 1;
          srcp++;
        }
        return 0;
      } else { // single star: any amount of anything except slash
        while (srcp<srcc) {
          if (http_wildcard_match(pat+patp,patc-patp,src+srcp,srcc-srcp)) return 1;
          if (src[srcp]=='/') return 0;
          srcp++;
        }
        return 0;
      }
    }
    if (srcp>=srcc) return 0;
    
    if (pat[patp]!=src[srcp]) return 0;
    patp++;
    srcp++;
  }
}
