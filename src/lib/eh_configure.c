#include "eh_internal.h"
#include "opt/serial/serial.h"

/* Set string.
 */
 
static int eh_config_set_string(char **dst,const char *src,int srcc) {
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
    "\n"
  );
}

/* Key=value arguments.
 */
 
static int eh_argv_kv(const char *k,int kc,const char *v,int vc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  
  // When value is omitted, check for a key prefix "no-". Replace value with "1" or "0".
  if (!vc) {
    if ((kc>3)&&!memcmp(k,"no-",3)) {
      k+=3;
      kc-=3;
      v="0";
      vc=1;
    } else {
      v="1";
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
  if ((kc==10)&&!memcmp(k,"fullscreen",10)) { eh.fullscreen=vn; return 0; }
  if ((kc==10)&&!memcmp(k,"audio-rate",10)) { eh.audio_rate=vn; return 0; }
  if ((kc==11)&&!memcmp(k,"audio-chanc",11)) { eh.audio_chanc=vn; return 0; }
  if ((kc==12)&&!memcmp(k,"audio-device",12)) return eh_config_set_string(&eh.audio_device,v,vc);
  if ((kc==12)&&!memcmp(k,"glsl-version",12)) { eh.glsl_version=vn; return 0; }
  
  //TODO other params?
  
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

/* Configure, main entry point.
 */
 
int eh_configure(int argc,char **argv) {

  eh.exename="emulator";
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) {
    eh.exename=argv[0];
  }
  
  eh.glsl_version=120;
  
  int argi=1,err;
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
  
  return 0;
}
