#include "eh_inmgr_internal.h"
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"

/* Config to/from file.
 */

int eh_inmgr_load_config(struct eh_inmgr *inmgr,const char *path) {
  void *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) return 0; // File not found is not a real error here.
  int err=eh_inmgr_decode_config(inmgr,src,srcc,path);
  free(src);
  return err;
}

int eh_inmgr_save_config(const char *path,const struct eh_inmgr *inmgr) {
  struct sr_encoder encoder={0};
  if (eh_inmgr_encode_config(&encoder,inmgr)<0) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  int err=file_write(path,encoder.v,encoder.c);
  sr_encoder_cleanup(&encoder);
  return err;
}

/* Decode config device introducer and add a new config to the registry.
 * Caller should strip the '>>>'.
 */
 
static struct eh_inmgr_config *eh_inmgr_decode_config_introducer(
  struct eh_inmgr *inmgr,
  const char *src,int srcc,
  const char *refname,int lineno
) {
  int srcp=0,vendor=0,product=0,namec=0,tokenc;
  const char *name=0,*token=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (sr_int_eval(&vendor,token,tokenc)<2) {
    if (refname) fprintf(stderr,"%s:%d: Failed to evaluate '%.*s' as vendor ID.\n",refname,lineno,tokenc,token);
    return 0;
  }
  
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (sr_int_eval(&product,token,tokenc)<2) {
    if (refname) fprintf(stderr,"%s:%d: Failed to evaluate '%.*s' as product ID.\n",refname,lineno,tokenc,token);
    return 0;
  }
  
  name=src+srcp;
  namec=srcc-srcp;
  
  struct eh_inmgr_config *config=eh_inmgr_configv_append(inmgr);
  if (!config) return 0;
  
  if (eh_inmgr_config_set_name(config,name,namec)<0) return 0;
  config->vendor=vendor;
  config->product=product;
  
  return config;
}

/* Decode config.
 */

int eh_inmgr_decode_config(struct eh_inmgr *inmgr,const void *src,int srcc,const char *refname) {
  if (!inmgr) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  struct eh_inmgr_config *config=0;
  struct sr_decoder decoder={.v=src,.c=srcc};
  int lineno=0,linec;
  const char *line;
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    lineno++;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!line) continue;
    
    if ((linec>=3)&&!memcmp(line,">>>",3)) {
      line+=3; linec-=3;
      if (!(config=eh_inmgr_decode_config_introducer(inmgr,line,linec,refname,lineno))) {
        if (refname) fprintf(stderr,"%s:%d: Failed to parse device introducer.\n",refname,lineno);
        return -1;
      }
      continue;
    }
    
    if (!config) {
      if (refname) fprintf(stderr,"%s:%d: '>>>' required to begin device block.\n",refname,lineno);
      return -1;
    }
    
    const char *srctoken=line;
    int srctokenc=0;
    while (linec&&((unsigned char)line[0]>0x20)) { line++; linec--; srctokenc++; }
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    const char *dsttoken=line;
    int dsttokenc=linec;
    
    int srcbtnid,dstbtnid;
    if (sr_int_eval(&srcbtnid,srctoken,srctokenc)<1) {
      if (refname) fprintf(stderr,"%s:%d: Failed to evaluate '%.*s' as source button id.\n",refname,lineno,srctokenc,srctoken);
      return -1;
    }
    if ((dstbtnid=eh_btnid_eval(dsttoken,dsttokenc))<0) {
      if (refname) fprintf(stderr,"%s:%d:WARNING Failed to evaluate '%.*s' as logical button id.\n",refname,lineno,dsttokenc,dsttoken);
      continue; // Not fatal. I'm going to share these config files with other games, which might define their own extra buttons.
    }
    
    struct eh_inmgr_rule *rule=eh_inmgr_config_add_rule(config,srcbtnid);
    if (!rule) {
      if (refname) fprintf(stderr,"%s:%d: Duplicate source button %d\n",refname,lineno,srcbtnid);
      return -1;
    }
    rule->dstbtnid=dstbtnid;
  }
  return 0;
}

/* Encode config.
 */
 
int eh_inmgr_encode_config(struct sr_encoder *dst,const struct eh_inmgr *inmgr) {
  if (!dst||!inmgr) return -1;
  
  const struct eh_inmgr_config *config=inmgr->configv;
  int configi=inmgr->configc;
  for (;configi-->0;config++) {
    
    if (sr_encode_fmt(dst,
      ">>> 0x%04x 0x%04x %.*s\n",
      config->vendor,config->product,
      config->namec,config->name
    )<0) return -1;
    
    const struct eh_inmgr_rule *rule=config->rulev;
    int rulei=config->rulec;
    for (;rulei-->0;rule++) {
    
      if (sr_encode_fmt(dst,"0x%08x ",rule->srcbtnid)<0) return -1;
      while (1) {
        int err=eh_btnid_repr(dst->v+dst->c,dst->a-dst->c,rule->dstbtnid);
        if (err<=0) return -1;
        if (dst->c<=dst->a-err) {
          dst->c+=err;
          break;
        }
        if (sr_encoder_require(dst,err)<0) return -1;
      }
      if (sr_encode_raw(dst,"\n",1)<0) return -1;
    }
  }
  return 0;
}
