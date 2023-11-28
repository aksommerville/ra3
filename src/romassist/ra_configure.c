#include "ra_internal.h"
#include "opt/serial/serial.h"

/* --help
 */
 
static void ra_print_usage(const char *topic,int topicc) {
  fprintf(stderr,"\nUsage: %s [OPTIONS]\n",ra.exename);
  fprintf(stderr,
    "\n"
    "OPTIONS:\n"
    "  --help              Print this message and exit.\n"
    "  --dbroot=PATH       Directory containing our database.\n"
    "  --htdocs=PATH       Directory containing static files for HTTP service.\n"
    "  --menu=PATH         Executable for front end.\n"
    "  --port=2600         TCP port for HTTP server.\n"
    "  --terminable=1      Relaunch the menu if it quits, don't let the user quit.\n"
    "  --poweroff=0        Nonzero to call `poweroff` at POST /api/shutdown. Otherwise just quit.\n"
    "  --update=1          Automatically upgrade everything we can.\n"
    "\n"
  );
}

/* Receive key=value option.
 */

static int ra_configure_kv(const char *k,int kc,const char *v,int vc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  
  // Default value.
  if (!vc) {
    if ((kc>3)&&!memcmp(k,"no-",3)) { v="0"; vc=1; k+=3; kc-=3; }
    else { v="1"; vc=1; }
  }
  
  if ((kc==4)&&!memcmp(k,"help",4)) {
    ra_print_usage(v,vc);
    return -2;
  }
  
  #define STROPT(opt,fld) { \
    if ((kc==sizeof(opt)-1)&&!memcmp(k,opt,kc)) { \
      if (ra.fld) { \
        fprintf(stderr,"%s: Multiple values for '"opt"'.\n",ra.exename); \
        return -2; \
      } \
      if (!(ra.fld=malloc(vc+1))) return -1; \
      memcpy(ra.fld,v,vc); \
      ra.fld[vc]=0; \
      return 0; \
    } \
  }
  #define INTOPT(opt,fld,lo,hi) { \
    if ((kc==sizeof(opt)-1)&&!memcmp(k,opt,kc)) { \
      int n; \
      if ((vc==4)&&!memcmp(v,"true",4)) n=1; \
      else if ((vc==5)&&!memcmp(v,"false",5)) n=0; \
      else if ((sr_int_eval(&n,v,vc)<1)||(n<lo)||(n>hi)) { \
        fprintf(stderr,"%s: Expected integer in %d..%d for '%s', found '%.*s'.\n",ra.exename,lo,hi,opt,vc,v); \
        return -2; \
      } \
      ra.fld=n; \
      return 0; \
    } \
  }
  
  STROPT("dbroot",dbroot)
  STROPT("htdocs",htdocs)
  STROPT("menu",menu)
  INTOPT("port",http_port,1,65535)
  INTOPT("terminable",terminable,0,1)
  INTOPT("poweroff",allow_poweroff,0,1)
  INTOPT("update",update_enable,0,1)
  
  #undef STROPT
  #undef INTOPT
  
  fprintf(stderr,"%s: Unexpected option '%.*s' = '%.*s'.\n",ra.exename,kc,k,vc,v);
  return -2;
}

/* Receive argv.
 */
 
static int ra_configure_argv(int argc,char **argv) {
  int argi=1;
  while (argi<argc) {
    const char *arg=argv[argi++];
    if (!arg||!arg[0]) continue;
    
    // Positional arguments: Not using.
    if (arg[0]!='-') {
      fprintf(stderr,"%s: Unexpected argument '%s'.\n",ra.exename,arg);
      return -2;
    }
    
    // Single dash alone: Not using.
    if (!arg[1]) {
      fprintf(stderr,"%s: Unexpected argument '%s'.\n",ra.exename,arg);
      return -2;
    }
    
    // Short options. No combining, and everything can take an argument.
    if (arg[1]!='-') {
      char k=arg[1];
      const char *v=0;
      if (arg[2]) v=arg+2;
      else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
      int err=ra_configure_kv(&k,1,v,-1);
      if (err<0) {
        if (err!=-2) fprintf(stderr,"%s: Unspecified error processing option '%c' = '%s'.\n",ra.exename,k,v);
        return -2;
      }
      continue;
    }
    
    // Double dash slone: Not using.
    if (!arg[2]) {
      fprintf(stderr,"%s: Unexpected argument '%s'.\n",ra.exename,arg);
      return -2;
    }
    
    // Long options.
    const char *k=arg+2;
    int kc=0;
    while (k[kc]&&(k[kc]!='=')) kc++;
    const char *v=0;
    if (k[kc]=='=') v=k+kc+1;
    else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
    int err=ra_configure_kv(k,kc,v,-1);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error processing option '%.*s' = '%s'.\n",ra.exename,kc,k,v);
      return -2;
    }
  }
  return 0;
}

/* Configure, main entry point.
 */
 
int ra_configure(int argc,char **argv) {
  int err;

  ra.exename="romassist";
  if ((argc>=1)&&argv[0]&&argv[0][0]) {
    ra.exename=argv[0];
  }
  ra.http_port=2600;
  ra.terminable=1;
  ra.update_enable=1;
  
  //TODO config file?
  
  if ((err=ra_configure_argv(argc,argv))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error processing command line.\n",ra.exename);
    return -2;
  }
  
  return 0;
}
