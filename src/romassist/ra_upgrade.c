#include "ra_internal.h"
#include "opt/serial/serial.h"
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <unistd.h>

/* Startup.
 * Probably nothing for us to do?
 */
 
int ra_upgrade_startup() {
  ra.upgrade.fd=-1;
  return 0;
}

/* Helper for the child argv.
 */
 
struct ra_upgrade_argv {
  char **v;
  int c,a;
};

static void ra_upgrade_argv_cleanup(struct ra_upgrade_argv *argv) {
  if (argv->v) {
    while (argv->c-->0) free(argv->v[argv->c]);
    free(argv->v);
  }
}

static int ra_upgrade_argv_require(struct ra_upgrade_argv *argv,int addc) {
  if (addc<1) return 0;
  if (argv->c>INT_MAX-addc) return -1;
  int na=argv->c+addc;
  if (na>argv->a) {
    if (na>INT_MAX-8) na=(na+8)&~7;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(argv->v,sizeof(void*)*na);
    if (!nv) return -1;
    argv->v=nv;
    argv->a=na;
  }
  return 0;
}

static int ra_upgrade_argv_terminate(struct ra_upgrade_argv *argv) {
  if (ra_upgrade_argv_require(argv,1)<0) return -1;
  argv->v[argv->c]=0;
  return 0;
}

static int ra_upgrade_argv_append(struct ra_upgrade_argv *argv,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (ra_upgrade_argv_require(argv,1)<0) return -1;
  char *nv=malloc(srcc+1);
  if (!nv) return -1;
  memcpy(nv,src,srcc);
  nv[srcc]=0;
  argv->v[argv->c++]=nv;
  return 0;
}

static int ra_upgrade_argv_appendf(struct ra_upgrade_argv *argv,const char *fmt,...) {
  if (ra_upgrade_argv_require(argv,1)<0) return -1;
  int tmpa=64;
  char *tmp=malloc(tmpa);
  if (!tmp) return -1;
  while (1) {
    va_list vargs;
    va_start(vargs,fmt);
    int tmpc=vsnprintf(tmp,tmpa,fmt,vargs);
    if ((tmpc<0)||(tmpc==INT_MAX)) {
      free(tmp);
      return -1;
    }
    if (tmpc<tmpa) {
      argv->v[argv->c++]=tmp;
      return 0;
    }
    tmpa=tmpc+1;
    void *nv=realloc(tmp,tmpa);
    if (!nv) {
      free(tmp);
      return -1;
    }
    tmp=nv;
  }
}

static void ra_upgrade_argv_log(const struct ra_upgrade_argv *argv,const char *msg) {
  fprintf(stderr,"%s",msg);
  int i=0; for (;i<argv->c;i++) fprintf(stderr," '%s'",argv->v[i]);
  fprintf(stderr,"\n");
}

/* Compose argv for "git+make"
 */
 
static int ra_upgrade_compose_argv_gitmake(struct ra_upgrade_argv *argv,struct db *db,const struct db_upgrade *upgrade,const char *param,int paramc) {
  
  if (ra_upgrade_argv_append(argv,"/bin/sh",-1)<0) return -1;
  if (ra_upgrade_argv_append(argv,"-c",2)<0) return -1;
  
  struct sr_encoder shcmd={0};
  sr_encode_fmt(&shcmd,"cd '%.*s' && git pull && make",paramc,param);
  ra_upgrade_argv_append(argv,shcmd.v,shcmd.c);
  sr_encoder_cleanup(&shcmd);
  
  if (ra_upgrade_argv_terminate(argv)<0) return -1;
  return 0;
}

/* Compose argv for an upgrade. No globals.
 */
 
static int ra_upgrade_compose_argv(struct ra_upgrade_argv *argv,struct db *db,const struct db_upgrade *upgrade) {

  const char *method=0,*param=0;
  int methodc=db_string_get(&method,ra.db,ra.upgrade.inflight->method); if (methodc<0) methodc=0;
  int paramc=db_string_get(&param,ra.db,ra.upgrade.inflight->param); if (paramc<0) paramc=0;
  
  if ((methodc==8)&&!memcmp(method,"git+make",8)) return ra_upgrade_compose_argv_gitmake(argv,db,upgrade,param,paramc);
  
  fprintf(stderr,"%s:%d: Unknown upgrade method '%.*s'\n",__FILE__,__LINE__,methodc,method);
  return -1;
}

/* Spawn child process with the given arguments. No globals, aside from logging.
 */
 
static int ra_upgrade_spawn_child(int *fd,struct ra_upgrade_argv *argv) {
  if (argv->c<1) return -1;
  
  /* Make a pipe to read child's output from.
   */
  int chpipe[2]={-1,-1};
  if (pipe(chpipe)<0) return -1;
  
  /* Fork the new process.
   */
  int pid=fork();
  if (pid<0) {
    fprintf(stderr,"%s: fork: %m\n",ra.exename);
    close(chpipe[0]);
    close(chpipe[1]);
    return -1;
  }
  
  /* Parent? We're done.
   */
  if (pid) {
    close(chpipe[1]);
    *fd=chpipe[0];
    return pid;
  }
  
  /* Combine and capture the child's stdout and stderr.
   */
  close(chpipe[0]);
  dup2(chpipe[1],STDOUT_FILENO);
  dup2(chpipe[1],STDERR_FILENO);
  
  /* We are the child.
   * TODO: Working directory? Environment? Other housekeeping?
   */
  execvp(argv->v[0],argv->v);
  fprintf(stderr,"%s: execvp: %m\n",ra.exename);
  exit(1);
  return -1;
}

/* Launch what's recorded in (ra.upgrade.inflight).
 */
 
static int ra_upgrade_launch() {
  if (ra.upgrade.pid) return -1;
  ra.upgrade.noop=1; // until the child's output suggests otherwise
  
  struct ra_upgrade_argv argv={0};
  if (ra_upgrade_compose_argv(&argv,ra.db,ra.upgrade.inflight)<0) {
    ra_upgrade_argv_cleanup(&argv);
    return -1;
  }
  
  //ra_upgrade_argv_log(&argv,"Ready to spawn child.");
  int pid=ra_upgrade_spawn_child(&ra.upgrade.fd,&argv);
  //fprintf(stderr,"...pid=%d\n",pid);
  if (pid<0) return -1;
  ra.upgrade.pid=pid;
  
  ra_upgrade_argv_cleanup(&argv);
  return 0;
}

/* Outgoing notifications.
 */
 
static void ra_upgrade_notify_common(struct sr_encoder *packet) {
  sr_encode_json_object_start(packet,0,0);
  sr_encode_json_string(packet,"id",2,"upgrade",7);
  
  if (ra.upgrade.waitingc) {
    int arrayctx=sr_encode_json_array_start(packet,"pending",7);
    const struct db_upgrade *upgrade=ra.upgrade.waitingv;
    int i=ra.upgrade.waitingc;
    for (;i-->0;upgrade++) {
      // Is it overkill to encode the whole upgrade for everything pending?
      sr_encode_json_setup(packet,0,0);
      db_upgrade_encode(packet,ra.db,upgrade,DB_FORMAT_json,DB_DETAIL_record);
    }
    sr_encode_json_array_end(packet,arrayctx);
  }
  
  if (ra.upgrade.inflight) {
    sr_encode_json_setup(packet,"running",7);
    db_upgrade_encode(packet,ra.db,ra.upgrade.inflight,DB_FORMAT_json,DB_DETAIL_record);
  }
}
 
static void ra_upgrade_notify() {
  struct sr_encoder packet={0};
  ra_upgrade_notify_common(&packet);
  if (sr_encode_json_object_end(&packet,0)>=0) {
    ra_websocket_send_to_role(RA_WEBSOCKET_ROLE_MENU,1,packet.v,packet.c);
  }
  sr_encoder_cleanup(&packet);
}

static void ra_upgrade_notify_text(const char *src,int srcc) {
  struct sr_encoder packet={0};
  ra_upgrade_notify_common(&packet);
  sr_encode_json_string(&packet,"text",4,src,srcc);
  if (sr_encode_json_object_end(&packet,0)>=0) {
    ra_websocket_send_to_role(RA_WEBSOCKET_ROLE_MENU,1,packet.v,packet.c);
  }
  sr_encoder_cleanup(&packet);
}

static void ra_upgrade_notify_status(int status) {
  struct sr_encoder packet={0};
  ra_upgrade_notify_common(&packet);
  sr_encode_json_int(&packet,"status",6,status);
  if (sr_encode_json_object_end(&packet,0)>=0) {
    ra_websocket_send_to_role(RA_WEBSOCKET_ROLE_MENU,1,packet.v,packet.c);
  }
  sr_encoder_cleanup(&packet);
}

/* Begin one upgrade.
 */
 
static int ra_upgrade_begin_1(struct db_upgrade *req) {
  if (ra.upgrade.inflight) return -1;
  if (ra.upgrade.pid) return -1;
  if (!(ra.upgrade.inflight=malloc(sizeof(struct db_upgrade)))) return -1;
  memcpy(ra.upgrade.inflight,req,sizeof(struct db_upgrade));
  if (ra_upgrade_launch()<0) {
    free(ra.upgrade.inflight);
    ra.upgrade.inflight=0;
    return -1;
  }
  ra_upgrade_notify();
  return 0;
}

/* Drop inflight state and start next upgrade if there is one.
 */
 
static int ra_upgrade_advance() {
  ra.upgrade.pid=0;
  if (ra.upgrade.fd>=0) {
    close(ra.upgrade.fd);
    ra.upgrade.fd=-1;
  }
  if (ra.upgrade.inflight) {
    free(ra.upgrade.inflight);
    ra.upgrade.inflight=0;
  }
  if (ra.upgrade.waitingc<1) {
    ra_upgrade_notify();
    return 0;
  }
  if (ra_upgrade_begin_1(ra.upgrade.waitingv)<0) return -1;
  ra.upgrade.waitingc--;
  memmove(ra.upgrade.waitingv,ra.upgrade.waitingv+1,sizeof(struct db_upgrade)*ra.upgrade.waitingc);
  return 0;
}

/* Report child's exit status, update db records, etc.
 * (ra.upgrade.inflight) should still be populated at this point.
 */
 
static void ra_upgrade_report_completion(int status) {
  if (!ra.upgrade.inflight) {
    // I think this isn't possible but not certain. If it happens, we might have left clients waiting for the outcome of an upgrade.
    return;
  }
  if (status) {
    db_upgrade_update_for_build(ra.db,ra.upgrade.inflight->upgradeid,"error",5);
  } else if (ra.upgrade.noop) {
    db_upgrade_update_for_build(ra.db,ra.upgrade.inflight->upgradeid,"noop",4);
  } else {
    db_upgrade_update_for_build(ra.db,ra.upgrade.inflight->upgradeid,0,0);
  }
  ra_upgrade_notify_status(status);
}

/* Report output from child's stdout and stderr (combined).
 */
 
static int hasstr(const char *haystack,int haystackc,const char *needle,int needlec) {
  if (needlec>haystackc) return 0;
  haystackc-=needlec;
  for (;haystackc-->=0;haystack++) if (!memcmp(haystack,needle,needlec)) return 1;
  return 0;
}
 
static void ra_upgrade_report_output(const char *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    const char *line=src+srcp;
    int linec=0;
    while ((srcp<srcc)&&(src[srcp++]!=0x0a)) linec++;
    ra_upgrade_notify_text(line,linec);
    
    /* Does this look like real activity? If so, we should clear the global (noop) flag.
     * TODO: These markers are specific to git+make. Other methods will likely need different logic.
     */
    if (ra.upgrade.noop) {
      if ((srcc==19)&&!memcmp(src,"Already up to date.",19)) { // git-pull says this as it noops
      } else if (hasstr(src,srcc,"Entering directory",18)) { // make says this a lot, not important
      } else if (hasstr(src,srcc,"Leaving directory",17)) { // ''
      } else if (hasstr(src,srcc,"Nothing to be done for",22)) { // make says this sometimes, depends how things are arranged
      } else { // anything else, let's figure some action was taken.
        ra.upgrade.noop=0;
      }
    }
  }
}

/* Routine update.
 */
 
int ra_upgrade_update() {

  /* When nothing is happening, check current time against our recorded timestamp.
   * Greater than 24 hours, trigger a full check.
   * Since the recorded timestamp is initially zero, this is the startup check too.
   */
  if (ra.update_enable&&!ra.upgrade.inflight&&!ra.upgrade.waitingc) {
    uint32_t now=db_time_now();
    const int MINUTES_PER_DAY=60*24;
    if (db_time_diff_m(ra.upgrade.last_update_time,now)>=MINUTES_PER_DAY) {
      ra.upgrade.last_update_time=now;
      struct db_upgrade *list=0;
      int listc=ra_upgrade_list(&list,0,0xffffffff);
      if (listc>0) {
        ra_upgrade_begin(list,listc);
        free(list);
      }
    }
  }

  if (ra.upgrade.fd>=0) {
    struct pollfd pollfd={.fd=ra.upgrade.fd,.events=POLLIN|POLLERR|POLLHUP};
    if (poll(&pollfd,1,0)>0) {
      char tmp[1024];
      int tmpc=read(ra.upgrade.fd,tmp,sizeof(tmp));
      if (tmpc>0) {
        ra_upgrade_report_output(tmp,tmpc);
      } else if (tmpc<0) {
        close(ra.upgrade.fd);
        ra.upgrade.fd=-1;
      }
    }
  }
  if (ra.upgrade.pid) {
    int err,wstatus;
    err=waitpid(ra.upgrade.pid,&wstatus,WNOHANG);
    if (err<0) {
      ra_upgrade_report_completion(1);
      ra_upgrade_advance();
    } else if (err>0) {
      if (WIFEXITED(wstatus)) {
        int status=WEXITSTATUS(wstatus);
        ra_upgrade_report_completion(status);
      } else if (WIFSIGNALED(wstatus)) {
        //int sigid=WTERMSIG(wstatus);
        ra_upgrade_report_completion(1);
      } else {
        ra_upgrade_report_completion(1);
      }
      ra_upgrade_advance();
    }
  }
  return 0;
}

/* Begin a batch of updates.
 */
 
int ra_upgrade_begin(struct db_upgrade *upgradev,int upgradec) {
  if (!upgradev||(upgradec<1)) return 0;
  if (!ra_upgrade_is_idle(&ra.upgrade)) return -1;
  if (ra_upgrade_begin_1(upgradev)<0) return -1;
  if (upgradec>1) {
    int nc=upgradec-1;
    if (nc>ra.upgrade.waitinga) {
      void *nv=realloc(ra.upgrade.waitingv,sizeof(struct db_upgrade)*nc);
      if (!nv) return -1;
      ra.upgrade.waitingv=nv;
      ra.upgrade.waitinga=nc;
    }
    memcpy(ra.upgrade.waitingv,upgradev+1,sizeof(struct db_upgrade)*nc);
    ra.upgrade.waitingc=nc;
  }
  return 0;
}

/* Determine which upgrades ought to happen.
 */
 
struct ra_upgrade_list_context {
  struct db_upgrade *v;
  int c,a;
};

static void ra_upgrade_list_context_cleanup(struct ra_upgrade_list_context *ctx) {
  if (ctx->v) free(ctx->v);
}

static int ra_upgrade_list_append(struct ra_upgrade_list_context *ctx,const struct db_upgrade *upgrade) {
  if (ctx->c>=ctx->a) {
    int na=ctx->a+16;
    if (na>INT_MAX/sizeof(struct db_upgrade)) return -1;
    void *nv=realloc(ctx->v,sizeof(struct db_upgrade)*na);
    if (!nv) return -1;
    ctx->v=nv;
    ctx->a=na;
  }
  struct db_upgrade *dst=ctx->v+ctx->c++;
  memcpy(dst,upgrade,sizeof(struct db_upgrade));
  return 0;
}

static int ra_upgrade_list_find(const struct ra_upgrade_list_context *ctx,uint32_t upgradeid) {
  const struct db_upgrade *upgrade=ctx->v;
  int i=0;
  for (;i<ctx->c;i++,upgrade++) {
    if (upgrade->upgradeid==upgradeid) return i;
  }
  return -1;
}

// Fill list with all upgrades matching the criteria. 
static int ra_upgrade_list_initial(struct ra_upgrade_list_context *ctx,uint32_t upgradeid,uint32_t before) {
  const struct db_upgrade *upgrade=db_upgrade_get_by_index(ra.db,0);
  int i=db_upgrade_count(ra.db);
  for (;i-->0;upgrade++) {
  
    if (upgradeid&&(upgradeid!=upgrade->upgradeid)) continue;
    if (upgrade->checktime>before) continue;
    
    if (ra_upgrade_list_append(ctx,upgrade)<0) return -1;
  }
  return 0;
}

// Add anything dependant on anything in the current list, recursively.
static int ra_upgrade_list_cascade(struct ra_upgrade_list_context *ctx) {
  while (1) {
    int dirty=0;
    const struct db_upgrade *upgrade=db_upgrade_get_by_index(ra.db,0);
    int i=db_upgrade_count(ra.db);
    for (;i-->0;upgrade++) {
      if (!upgrade->depend) continue;
      if (ra_upgrade_list_find(ctx,upgrade->upgradeid)>=0) continue;
      if (ra_upgrade_list_find(ctx,upgrade->depend)<0) continue;
      if (ra_upgrade_list_append(ctx,upgrade)<0) return -1;
      dirty=1;
    }
    if (!dirty) return 0;
  }
}

/* Ensure dependencies are in the proper build order.
 * Beware that we don't detect dependency cycles.
 * If a cycle exists, everything still works, but we build in an imperfect order (because there isn't a perfect order).
 * We keep this sort safe with a panic counter.
 */
static int ra_upgrade_list_sort(struct ra_upgrade_list_context *ctx) {
  int panic=ctx->c;
  while (panic-->0) {
    // Find one upgrade with a (depend) that is after it.
    // If we can't find one, we're done. If we can, move it to the back.
    // This might not be smartest way to go about things, but there's a definite upper limit
    // to the count of operations, for our panic counter.
    int fromp=-1;
    int i=0;
    struct db_upgrade *upgrade=ctx->v;
    for (;i<ctx->c;i++,upgrade++) {
      if (!upgrade->depend) continue;
      if (ra_upgrade_list_find(ctx,upgrade->depend)<=i) continue;
      fromp=i;
      break;
    }
    if (fromp<0) break;
    struct db_upgrade tmp=ctx->v[fromp];
    memmove(ctx->v+fromp,ctx->v+fromp+1,sizeof(struct db_upgrade)*(ctx->c-fromp-1));
    ctx->v[ctx->c-1]=tmp;
  }
  return 0;
}
 
int ra_upgrade_list(struct db_upgrade **dstpp,uint32_t upgradeid,uint32_t before) {
  struct ra_upgrade_list_context ctx={0};
  if (ra_upgrade_list_initial(&ctx,upgradeid,before)<0) {
    ra_upgrade_list_context_cleanup(&ctx);
    return -1;
  }
  if (ra_upgrade_list_cascade(&ctx)<0) {
    ra_upgrade_list_context_cleanup(&ctx);
    return -1;
  }
  if (ra_upgrade_list_sort(&ctx)<0) {
    ra_upgrade_list_context_cleanup(&ctx);
    return -1;
  }
  *dstpp=ctx.v;
  ctx.v=0;
  ra_upgrade_list_context_cleanup(&ctx);
  return ctx.c;
}
