#include "eh_internal.h"
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"

/* Set string.
 */
 
int eh_config_set_string(char **dst,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (*dst) free(*dst);
  *dst=nv;
  return 0;
}

/* --help
 */
 
static void eh_print_help(const char *topic,int topicc) {
  fprintf(stderr,"Usage: %s [OPTIONS] ROMFILE\n",eh.exename);
  fprintf(stderr,
    "\n"
    "OPTIONS:\n"
    "  --help                   Print this message.\n"
    "  --video=LIST             Video drivers, in order of preference.\n"
    "  --audio=LIST             Audio drivers, in order of preference.\n"
    "  --input=LIST             Input drivers -- we will instantiate all.\n"
    "  --fullscreen=0|1\n"
    "  --audio-rate=HZ\n"
    "  --audio-chanc=1|2\n"
    "  --audio-device=STRING\n"
    "  --glsl-version=INT\n"
    "  --input-config=PATH\n"
    "  --screen=any             (left,right,top,bottom) Try to land window on the given monitor.\n"
    "  --romassist=HOST:PORT\n"
    "\n"
  );
}

/* --screen
 */
 
static int eh_config_screen_eval(const char *src,int srcc) {
  char norm[16];
  if ((srcc<1)||(srcc>sizeof(norm))) return 0;
  int i=srcc; while (i-->0) {
    if ((src[i]>='a')&&(src[i]<='z')) norm[i]=src[i]-0x20;
    else norm[i]=src[i];
  }
  if ((srcc==4)&&!memcmp(norm,"LEFT",4)) return EH_SCREEN_LEFT;
  if ((srcc==5)&&!memcmp(norm,"RIGHT",5)) return EH_SCREEN_RIGHT;
  if ((srcc==3)&&!memcmp(norm,"TOP",3)) return EH_SCREEN_TOP;
  if ((srcc==6)&&!memcmp(norm,"BOTTOM",6)) return EH_SCREEN_BOTTOM;
  return 0;
}

static const char *eh_config_screen_repr(int v) {
  switch (v) {
    case EH_SCREEN_LEFT: return "LEFT";
    case EH_SCREEN_RIGHT: return "RIGHT";
    case EH_SCREEN_TOP: return "TOP";
    case EH_SCREEN_BOTTOM: return "BOTTOM";
  }
  return "";
}

/* --romassist=HOST:PORT
 */
 
static int eh_config_set_romassist(const char *src,int srcc) {
  const char *host=src;
  int hostc=0;
  while ((hostc<srcc)&&(host[hostc]!=':')) hostc++;
  if ((hostc>=srcc)||(host[hostc]!=':')) return -1;
  int port=0;
  if ((sr_int_eval(&port,host+hostc+1,srcc-hostc-1)<2)||(port<0)||(port>0xffff)) return -1;
  if (eh_config_set_string(&eh.romassist_host,host,hostc)<0) return -1;
  eh.romassist_port=port;
  return 0;
}

/* Key=value arguments.
 */
 
static int eh_argv_kv(const char *k,int kc,const char *v,int vc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  
  // When value is omitted, check for a key prefix "no-".
  if (!vc) {
    if ((kc>3)&&!memcmp(k,"no-",3)) {
      k+=3;
      kc-=3;
      v="0";
      vc=1;
    }
  }
  
  // Evaluate (v) as a signed decimal integer.
  int vn=0,vnok=0,err;
  if (sr_int_eval(&vn,v,vc)>=0) vnok=1;
  else vn=0;
  
  // Check built-in parameters.
  
  if ((kc==4)&&!memcmp(k,"help",4)) {
    eh_print_help(v,vc);
    return 0;
  }
  
  if ((kc==5)&&!memcmp(k,"video",5)) return eh_config_set_string(&eh.video_drivers,v,vc);
  if ((kc==5)&&!memcmp(k,"audio",5)) return eh_config_set_string(&eh.audio_drivers,v,vc);
  if ((kc==5)&&!memcmp(k,"input",5)) return eh_config_set_string(&eh.input_drivers,v,vc);
  if ((kc==10)&&!memcmp(k,"fullscreen",10)) { eh.fullscreen=vc?vn:1; return 0; }
  if ((kc==10)&&!memcmp(k,"audio-rate",10)) { eh.audio_rate=vn; return 0; }
  if ((kc==11)&&!memcmp(k,"audio-chanc",11)) { eh.audio_chanc=vn; return 0; }
  if ((kc==12)&&!memcmp(k,"audio-device",12)) return eh_config_set_string(&eh.audio_device,v,vc);
  if ((kc==12)&&!memcmp(k,"glsl-version",12)) { eh.glsl_version=vn; return 0; }
  if ((kc==12)&&!memcmp(k,"input-config",12)) return eh_config_set_string(&eh.input_config_path,v,vc);
  if ((kc==6)&&!memcmp(k,"screen",6)) { eh.prefer_screen=eh_config_screen_eval(v,vc); return 0; }
  
  /* "--romassist" splits into two fields.
   */
  if ((kc==9)&&!memcmp(k,"romassist",9)) return eh_config_set_romassist(v,vc);

  /* A few parameters left over from v2 that we're not using anymore.
   * Ignore them if present, otherwise our shared config file would throw some errors.
   */
  if ((kc==12)&&!memcmp(k,"audio-format",12)) return 0;
  if ((kc==6)&&!memcmp(k,"shader",6)) return 0;
  if ((kc==7)&&!memcmp(k,"fastfwd",7)) return 0;
  
  // Give the client a whack at it.
  if (eh.delegate.configure) {
    if ((err=eh.delegate.configure(k,kc,v,vc,vn))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error processing option '%.*s' = '%.*s'.\n",eh.exename,kc,k,vc,v);
      return -2;
    }
  }
  
  fprintf(stderr,"%s: Unexpected option '%.*s' = '%.*s'.\n",eh.exename,kc,k,vc,v);
  return -2;
}

/* Positional arguments.
 */
 
static int eh_argv_positional(const char *arg) {
  if (!eh.rompath) return eh_config_set_string(&eh.rompath,arg,-1);
  fprintf(stderr,"%s: Unexpected argument '%s'.\n",eh.exename,arg);
  return -2;
}

/* Default path for config file.
 * Never returns 0 or >=dsta.
 */
 
static int eh_config_get_path(char *dst,int dsta) {
  if (!dst||(dsta<2)) return -1;
  int dstc=eh_get_romassist_directory(dst,dsta);
  if ((dstc<1)||(dstc>=dsta-12)) return -1;
  memcpy(dst+dstc,"/emuhost.cfg",13);
  return dstc+12;
}

/* Encode live configuration.
 */
 
static int eh_config_encode(struct sr_encoder *dst) {
  if (sr_encode_raw(dst,"# This file is rewritten blindly by emuhost. Comments and formatting will be lost.\n",-1)<0) return -1;
  
  if (sr_encode_fmt(dst,"video=%s\n",eh.video_drivers?eh.video_drivers:"")<0) return -1;
  if (sr_encode_fmt(dst,"fullscreen=%d\n",eh.fullscreen)<0) return -1;
  if (sr_encode_fmt(dst,"screen=%s\n",eh_config_screen_repr(eh.prefer_screen))<0) return -1;
  if (sr_encode_fmt(dst,"glsl-version=%d\n",eh.glsl_version)<0) return -1;
  
  if (sr_encode_fmt(dst,"audio=%s\n",eh.audio_drivers?eh.audio_drivers:"")<0) return -1;
  if (sr_encode_fmt(dst,"audio-rate=%d\n",eh.audio_rate)<0) return -1;
  if (sr_encode_fmt(dst,"audio-chanc=%d\n",eh.audio_chanc)<0) return -1;
  if (sr_encode_fmt(dst,"audio-device=%s\n",eh.audio_device?eh.audio_device:"")<0) return -1;
  
  if (sr_encode_fmt(dst,"input=%s\n",eh.input_drivers?eh.input_drivers:"")<0) return -1;
  if (sr_encode_fmt(dst,"input-config=%s\n",eh.input_config_path?eh.input_config_path:"")<0) return -1;
  
  if (sr_encode_fmt(dst,"romassist=%s:%d\n",eh.romassist_host?eh.romassist_host:"",eh.romassist_port)<0) return -1;
  
  return 0;
}

/* Save live configuration to file.
 */
 
int eh_config_save() {
  fprintf(stderr,"%s...\n",__func__);
  char path[1024];
  if (eh_config_get_path(path,sizeof(path))<0) return -1;
  struct sr_encoder encoder={0};
  if (eh_config_encode(&encoder)<0) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  int err=file_write(path,encoder.v,encoder.c);
  sr_encoder_cleanup(&encoder);
  if (err<0) return err;
  fprintf(stderr,"%s: Saved emuhost config.\n",path);
  return 0;
}

/* Parse and apply config file.
 */
 
static int eh_config_apply_file(const char *src,int srcc,const char *path) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  int lineno=0,err,linec;
  const char *line;
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    lineno++;
    int i=0; for (;i<linec;i++) if (line[i]=='#') linec=i;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    if (!linec) continue;
    
    const char *k=line;
    int kc=0,linep=0;
    while ((linep<linec)&&(line[linep++]!='=')) kc++;
    while (kc&&((unsigned char)k[kc-1]<=0x20)) kc--;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    const char *v=line+linep;
    int vc=linec-linep;
    
    if ((err=eh_argv_kv(k,kc,v,vc))<0) {
      fprintf(stderr,"%s:%d: Error applying config '%.*s' = '%.*s'\n",path,lineno,kc,k,vc,v);
      return -2;
    }
  }
  return 0;
}

/* Find a config file and if it exists, apply configuration from there.
 */
 
static int eh_config_from_file() {
  char path[1024];
  int pathc=eh_config_get_path(path,sizeof(path));
  if (pathc<0) return -1;
  char *serial=0;
  int serialc=file_read(&serial,path);
  if (serialc<0) {
    fprintf(stderr,"%s: Config file not found or failed to read. (this is ok)\n",path);
    return 0;
  }
  int err=eh_config_apply_file(serial,serialc,path);
  free(serial);
  return err;
}

/* Start configuration.
 * Set any defaults if we can cheaply and passively.
 */
 
static void eh_configure_start() {
  eh.glsl_version=120;
  eh.romassist_port=2600;
}

/* Finish configuration.
 * Last chance for defaults or validation.
 */
 
static int eh_configure_finish() {

  // input-config, default to "~/.romassist/input.cfg"
  if (!eh.input_config_path) {
    char tmp[1024];
    int tmpc=eh_get_romassist_directory(tmp,sizeof(tmp));
    if ((tmpc>0)&&(tmpc<sizeof(tmp)-10)) {
      memcpy(tmp+tmpc,"/input.cfg",11);
      tmpc+=10;
      if (eh_config_set_string(&eh.input_config_path,tmp,tmpc)<0) return -1;
    }
  }
  
  // romassist_host, default to "localhost"
  if (!eh.romassist_host&&eh.romassist_port) {
    if (eh_config_set_string(&eh.romassist_host,"localhost",9)<0) return -1;
  }

  return 0;
}

/* Configure, main entry point.
 */
 
int eh_configure(int argc,char **argv) {

  eh.exename="emulator";
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) {
    eh.exename=argv[0];
  }
  
  eh_configure_start();
  int err=eh_config_from_file();
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error loading general config file.\n",eh.exename);
    return -2;
  }
  
  int argi=1;
  while (argi<argc) {
    const char *arg=argv[argi++];
    if (!arg||!arg[0]) continue;
    
    // Positional args (no dash).
    if (arg[0]!='-') {
      if ((err=eh_argv_positional(arg))<0) {
        if (err!=-2) fprintf(stderr,"%s: Unspecified error processing argument '%s'.\n",eh.exename,arg);
        return -2;
      }
      continue;
    }
    
    // Single dash alone. Reserved.
    if (!arg[1]) {
      fprintf(stderr,"%s: Unexpected argument '%s'.\n",eh.exename,arg);
      return -2;
    }
    
    // Short options: "-k" "-kVVV" "-k VVV". No combining.
    if (arg[1]!='-') {
      char k=arg[1];
      const char *v=arg+2;
      if (!*v&&(argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
      if ((err=eh_argv_kv(&k,1,v,-1))<0) {
        if (err!=-2) fprintf(stderr,"%s: Unspecified error processing argument '%s'.\n",eh.exename,arg);
        return -2;
      }
      continue;
    }
    
    // More than one dash alone. Reserved.
    const char *stripped=arg;
    while (stripped[0]=='-') stripped++;
    if (!stripped[0]) {
      fprintf(stderr,"%s: Unexpected argument '%s'.\n",eh.exename,arg);
      return -2;
    }
    
    // Long options: "--k=v" "--k v" "--k"
    const char *k=stripped;
    int kc=0;
    while (k[kc]&&(k[kc]!='=')) kc++;
    const char *v=0;
    if (k[kc]=='=') v=k+kc+1;
    else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
    if ((err=eh_argv_kv(k,kc,v,-1))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error processing argument '%s'.\n",eh.exename,arg);
      return -2;
    }
  }
  
  return eh_configure_finish();
}
