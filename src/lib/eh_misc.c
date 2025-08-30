#include "eh_internal.h"
#include "inmgr/inmgr.h"
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"
#include <stdarg.h>

/* gcfg directories.
 */
 
int eh_get_scratch_directory(char **dstpp) {
  char tmp[1024];
  int tmpc=inmgr_compose_path(tmp,sizeof(tmp),"romassist",9,eh.delegate.name,-1);
  if ((tmpc<1)||(tmpc>=sizeof(tmp))) return -1;
  if (dir_mkdirp(tmp)<0) return -1;
  char *dst=malloc(tmpc+1);
  if (!dst) return -1;
  memcpy(dst,tmp,tmpc);
  dst[tmpc]=0;
  *dstpp=dst;
  return tmpc;
}
 
int eh_get_romassist_directory(char *dst,int dsta) {
  return inmgr_compose_path(dst,dsta,"romassist",9,0,0);
}

/* Encode HTTP request.
 */
 
static int eh_encode_http_request(struct sr_encoder *encoder,const char *method,int methodc,const char *path,int pathc,va_list vargs) {
  if (sr_encode_json_object_start(encoder,0,0)<0) return -1;
  if (sr_encode_json_string(encoder,"id",2,"http",4)<0) return -1;
  if (sr_encode_json_string(encoder,"method",6,method,methodc)<0) return -1;
  if (sr_encode_json_string(encoder,"path",4,path,pathc)<0) return -1;
  int jsonctx=sr_encode_json_object_start(encoder,"query",5);
  if (jsonctx<0) return -1;
  while (1) {
    const char *k=va_arg(vargs,const char*);
    if (!k) break;
    const char *v=va_arg(vargs,const char*);
    if (sr_encode_json_string(encoder,k,-1,v,-1)<0) return -1;
  }
  if (sr_encode_json_object_end(encoder,jsonctx)<0) return -1;
  if (sr_encode_json_object_end(encoder,0)<0) return -1;
  return 0;
}

/* HTTP request.
 */
 
int eh_request_http_(
  const char *method,int methodc,
  const char *path,int pathc,
  ...
) {
  if (!method) return -1;
  if (methodc<0) { methodc=0; while (method[methodc]) methodc++; }
  if (!path) return -1;
  if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  if (!eh.fakews) return -1;
  if (!fakews_is_connected(eh.fakews)) {
    if (fakews_connect_now(eh.fakews)<0) return -1;
  }
  struct sr_encoder encoder={0};
  va_list vargs;
  va_start(vargs,pathc);
  int err=eh_encode_http_request(&encoder,method,methodc,path,pathc,vargs);
  if (err<0) {
    sr_encoder_cleanup(&encoder);
    return err;
  }
  err=fakews_send(eh.fakews,1,encoder.v,encoder.c);
  sr_encoder_cleanup(&encoder);
  return err;
}

/* Find numeric "X-Correlation-Id" in HTTP response headers.
 */
 
static int eh_read_x_correlation_id(const char *src,int srcc) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_object_start(&decoder)<0) return 0;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    if ((kc==16)&&!memcmp(k,"X-Correlation-Id",16)) { // TODO I guess should be case-insensitive, HTTP is.
      int v=0;
      sr_decode_json_int(&v,&decoder);
      return v;
    }
    if (sr_decode_json_skip(&decoder)<0) return 0;
  }
  return 0;
}

/* Split HTTP response.
 */
 
int eh_http_response_split(struct eh_http_response *response,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  memset(response,0,sizeof(struct eh_http_response));
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_object_start(&decoder)<0) return -1;
  const char *k,*v;
  int kc,vc,idok=0;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    
    if ((kc==2)&&!memcmp(k,"id",2)) {
      char id[32];
      int idc=sr_decode_json_string(id,sizeof(id),&decoder);
      if ((idc!=12)||memcmp(id,"httpresponse",12)) return -1;
      idok=1;
      continue;
    }
    
    if ((kc==6)&&!memcmp(k,"status",6)) {
      if (sr_decode_json_int(&response->status,&decoder)<0) return -1;
      continue;
    }
    
    if ((kc==7)&&!memcmp(k,"message",7)) {
      if ((response->messagec=sr_decode_json_expression(&response->message,&decoder))<0) return -1;
      continue;
    }
    
    if ((kc==7)&&!memcmp(k,"headers",7)) {
      if ((response->headersc=sr_decode_json_expression(&response->headers,&decoder))<0) return -1;
      response->x_correlation_id=eh_read_x_correlation_id(response->headers,response->headersc);
      continue;
    }
    
    if ((kc==4)&&!memcmp(k,"body",4)) {
      if ((response->bodyc=sr_decode_json_expression(&response->body,&decoder))<0) return -1;
      continue;
    }
    
    if (sr_decode_json_skip(&decoder)<0) return -1;
  }
  if (!idok) return -1;
  return sr_decode_json_end(&decoder,0);
}
