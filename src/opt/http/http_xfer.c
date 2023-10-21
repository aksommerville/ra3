#include "http_internal.h"
#include <stdarg.h>

/* Delete.
 */
 
void http_xfer_del(struct http_xfer *xfer) {
  if (!xfer) return;
  if (xfer->refc-->1) return;
  
  if (xfer->line) free(xfer->line);
  http_dict_cleanup(&xfer->headers);
  sr_encoder_cleanup(&xfer->body);
  
  free(xfer);
}

/* Retain.
 */
 
int http_xfer_ref(struct http_xfer *xfer) {
  if (!xfer) return -1;
  if (xfer->refc<1) return -1;
  if (xfer->refc>=INT_MAX) return -1;
  xfer->refc++;
  return 0;
}

/* New.
 */
 
struct http_xfer *http_xfer_new(struct http_context *context) {
  if (!context) return 0;
  struct http_xfer *xfer=calloc(1,sizeof(struct http_xfer));
  if (!xfer) return 0;
  
  xfer->refc=1;
  xfer->context=context;
  
  return xfer;
}

/* Trivial accessors.
 */

int http_xfer_hold(struct http_xfer *xfer) {
  if (!xfer||(xfer->state!=HTTP_XFER_STATE_UNSET)) return -1;
  xfer->state=HTTP_XFER_STATE_DEFERRED;
  return 0;
}

int http_xfer_ready(struct http_xfer *xfer) {
  if (!xfer||(xfer->state!=HTTP_XFER_STATE_DEFERRED)) return -1;
  xfer->state=HTTP_XFER_STATE_DEFERRAL_COMPLETE;
  return 0;
}

/* Access to Request-Line or Status-Line.
 */
 
int http_xfer_get_status(const struct http_xfer *xfer) {
  if (!xfer) return 0;
  int srcp=0;
  while ((srcp<xfer->linec)&&((unsigned char)xfer->line[srcp]>0x20)) srcp++;
  while ((srcp<xfer->linec)&&((unsigned char)xfer->line[srcp]<=0x20)) srcp++;
  const char *src=xfer->line+srcp;
  int srcc=0;
  while ((srcp<xfer->linec)&&((unsigned char)xfer->line[srcp++]>0x20)) srcc++;
  int status=0;
  if (http_int_eval(&status,src,srcc)<0) return 0;
  return status;
}

int http_xfer_get_status_message(void *dstpp,const struct http_xfer *xfer) {
  if (!xfer) return 0;
  int srcp=0;
  while ((srcp<xfer->linec)&&((unsigned char)xfer->line[srcp]>0x20)) srcp++;
  while ((srcp<xfer->linec)&&((unsigned char)xfer->line[srcp]<=0x20)) srcp++;
  while ((srcp<xfer->linec)&&((unsigned char)xfer->line[srcp]>0x20)) srcp++;
  while ((srcp<xfer->linec)&&((unsigned char)xfer->line[srcp]<=0x20)) srcp++;
  *(void**)dstpp=xfer->line+srcp;
  return xfer->linec-srcp;
}

int http_xfer_get_method(const struct http_xfer *xfer) {
  if (!xfer) return 0;
  const char *src=xfer->line;
  int srcc=0;
  while ((srcc<xfer->linec)&&((unsigned char)src[srcc]>0x20)) srcc++;
  return http_method_eval(src,srcc);
}

int http_xfer_get_path(void *dstpp,const struct http_xfer *xfer) {
  if (!xfer) return 0;
  int srcp=0;
  while ((srcp<xfer->linec)&&((unsigned char)xfer->line[srcp]>0x20)) srcp++;
  while ((srcp<xfer->linec)&&((unsigned char)xfer->line[srcp]<=0x20)) srcp++;
  *(void**)dstpp=xfer->line+srcp;
  int pathc=0;
  while ((srcp<xfer->linec)&&((unsigned char)xfer->line[srcp]>0x20)&&(xfer->line[srcp]!='?')) { srcp++; pathc++; }
  return pathc;
}

int http_xfer_set_line(struct http_xfer *xfer,const char *src,int srcc) {
  if (!xfer) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { src++; srcc--; }
  char *nv=malloc(srcc+1);
  if (!nv) return -1;
  memcpy(nv,src,srcc);
  nv[srcc]=0;
  if (xfer->line) free(xfer->line);
  xfer->line=nv;
  xfer->linec=srcc;
  return 0;
}

int http_xfer_set_status(struct http_xfer *xfer,int code,const char *fmt,...) {
  if ((code<100)||(code>999)) code=500;
  char tmp[256];
  int tmpc=snprintf(tmp,sizeof(tmp),"HTTP/1.1 %3d ",code);
  if ((tmpc<1)||(tmpc>=sizeof(tmp))) return -1;
  va_list vargs;
  va_start(vargs,fmt);
  int err=vsnprintf(tmp+tmpc,sizeof(tmp)-tmpc,fmt,vargs);
  if ((err<0)||(tmpc+err>sizeof(tmp))) err=0;
  tmpc+=err;
  return http_xfer_set_line(xfer,tmp,tmpc);
}

/* Query string.
 */
 
static int http_for_query_text(const char *src,int srcc,int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),void *userdata) {
  int srcp=0,err;
  while (srcp<srcc) {
    if (src[srcp]=='&') { srcp++; continue; }
    const char *ksrc=src+srcp,*vsrc=0;
    int ksrcc=0,vsrcc=0;
    while ((srcp<srcc)&&(src[srcp]!='=')&&(src[srcp]!='&')) { srcp++; ksrcc++; }
    if ((srcp<srcc)&&(src[srcp]=='=')) {
      srcp++;
      vsrc=src+srcp;
      while ((srcp<srcc)&&(src[srcp++]!='&')) vsrcc++;
    }
    if (err=cb(ksrc,ksrcc,vsrc,vsrcc,userdata)) return err;
  }
  return 0;
}
 
int http_xfer_for_query(const struct http_xfer *xfer,int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),void *userdata) {
  if (!xfer||!cb) return -1;
  int linep=0,err;
  while ((linep<xfer->linec)&&((unsigned char)xfer->line[linep]>0x20)) linep++;
  while ((linep<xfer->linec)&&((unsigned char)xfer->line[linep]<=0x20)) linep++;
  while ((linep<xfer->linec)&&((unsigned char)xfer->line[linep]>0x20)) {
    if (xfer->line[linep++]=='?') {
      const char *src=xfer->line+linep;
      int srcc=0;
      while ((linep<xfer->linec)&&((unsigned char)xfer->line[linep++]>0x20)) srcc++;
      if (err=http_for_query_text(src,srcc,cb,userdata)) return err;
      break;
    }
  }
  const char *ct=0;
  int ctc=http_xfer_get_header(&ct,xfer,"Content-Type",12);
  if ((ctc==33)&&!memcmp(ct,"application/x-www-form-urlencoded",33)) {
    if (err=http_for_query_text(xfer->body.v,xfer->body.c,cb,userdata)) return err;
  }
  return 0;
}

struct http_xfer_get_query_string_context {
  char *dst;
  int dsta;
  int dstc;
  const char *k;
  int kc;
};

static int http_xfer_get_query_string_cb(const char *k,int kc,const char *v,int vc,void *userdata) {
  struct http_xfer_get_query_string_context *ctx=userdata;
  // To be strictly correct, we should url-decode (k) as well.
  // But I don't like that. A key should be arranged to not need escapes, and if the client escapes it anyway, well fuck him.
  if (kc!=ctx->kc) return 0;
  if (memcmp(k,ctx->k,kc)) return 0;
  ctx->dstc=http_url_decode(ctx->dst,ctx->dsta,v,vc);
  return 1;
}

int http_xfer_get_query_string(char *dst,int dsta,const struct http_xfer *xfer,const char *k,int kc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  struct http_xfer_get_query_string_context ctx={
    .dst=dst,
    .dsta=dsta,
    .k=k,
    .kc=kc,
  };
  http_xfer_for_query(xfer,http_xfer_get_query_string_cb,&ctx);
  if (ctx.dstc<dsta) dst[ctx.dstc]=0;
  return ctx.dstc;
}
  
int http_xfer_get_query_int(int *dst,const struct http_xfer *xfer,const char *k,int kc) {
  char src[32];
  int srcc=http_xfer_get_query_string(src,sizeof(src),xfer,k,kc);
  if ((srcc<0)||(srcc>sizeof(src))) return -1;
  return http_int_eval(dst,src,srcc);
}

/* Access to headers.
 */
      
int http_xfer_get_header(void *dstpp,const struct http_xfer *xfer,const char *k,int kc) {
  return http_dict_get(dstpp,&xfer->headers,k,kc);
}

int http_xfer_get_header_int(int *dst,const struct http_xfer *xfer,const char *k,int kc) {
  const char *src=0;
  int srcc=http_dict_get(&src,&xfer->headers,k,kc);
  if (srcc<=0) return -1;
  return http_int_eval(dst,src,srcc);
}

int http_xfer_set_header(struct http_xfer *xfer,const char *k,int kc,const char *v,int vc) {
  return http_dict_set(&xfer->headers,k,kc,v,vc);
}

/* Access to body.
 */
 
int http_xfer_get_body(void *dstpp,const struct http_xfer *xfer) {
  if (dstpp) *(void**)dstpp=xfer->body.v;
  return xfer->body.c;
}
 
int http_xfer_set_body(struct http_xfer *xfer,const void *src,int srcc) {
  if ((srcc<0)||(srcc&&!src)) return -1;
  xfer->body.c=0;
  return sr_encode_raw(&xfer->body,src,srcc);
}

 
int http_xfer_append_body(struct http_xfer *xfer,const void *src,int srcc) {
  return sr_encode_raw(&xfer->body,src,srcc);
}

int http_xfer_append_bodyf(struct http_xfer *xfer,const char *fmt,...) {
  if (!xfer) return -1;
  if (!fmt||!fmt[0]) return 0;
  while (1) {
    va_list vargs;
    va_start(vargs,fmt);
    int err=vsnprintf(xfer->body.v+xfer->body.c,xfer->body.a-xfer->body.c,fmt,vargs);
    if ((err<0)||(err>=INT_MAX)) return -1;
    if (xfer->body.c<xfer->body.a-err) {
      xfer->body.c+=err;
      return 0;
    }
    if (sr_encoder_require(&xfer->body,err+1)<0) return -1;
  }
}

struct sr_encoder *http_xfer_get_body_encoder(struct http_xfer *xfer) {
  return &xfer->body;
}
