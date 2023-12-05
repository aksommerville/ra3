#define _GNU_SOURCE 1
#include "ra_internal.h"
#include "opt/serial/serial.h"
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Cleanup.
 */
 
void ra_process_cleanup(struct ra_process *process) {
  if (process->next_launch) free(process->next_launch);
}

/* Helper for splitting up the text of a command.
 */
 
struct ra_cmd {
  char **envv;
  int envc,enva;
  char **argv;
  int argc,arga;
};

static void ra_cmd_cleanup(struct ra_cmd *cmd) {
  if (cmd->envv) {
    while (cmd->envc-->0) free(cmd->envv[cmd->envc]);
    free(cmd->envv);
  }
  if (cmd->argv) {
    while (cmd->argc-->0) free(cmd->argv[cmd->argc]);
    free(cmd->argv);
  }
}

static int ra_cmd_require(char ***v,int *c,int *a) {
  if (*c<*a) return 0;
  int na=(*a)+8;
  if (na>INT_MAX/sizeof(void*)) return -1;
  void *nv=realloc(*v,sizeof(void*)*na);
  if (!nv) return -1;
  *v=nv;
  *a=na;
  return 0;
}

static int ra_cmd_envv_append(struct ra_cmd *cmd,const char *src,int srcc) {
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (ra_cmd_require(&cmd->envv,&cmd->envc,&cmd->enva)<0) return -1;
  if (!(cmd->envv[cmd->envc]=malloc(srcc+1))) return -1;
  memcpy(cmd->envv[cmd->envc],src,srcc);
  cmd->envv[cmd->envc][srcc]=0;
  cmd->envc++;
  return 0;
}

static int ra_cmd_argv_append(struct ra_cmd *cmd,const char *src,int srcc) {
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (ra_cmd_require(&cmd->argv,&cmd->argc,&cmd->arga)<0) return -1;
  if (!(cmd->argv[cmd->argc]=malloc(srcc+1))) return -1;
  memcpy(cmd->argv[cmd->argc],src,srcc);
  cmd->argv[cmd->argc][srcc]=0;
  cmd->argc++;
  return 0;
}

/* (argv) gets a null at the end, no surprises there.
 * If (envc) is zero we don't touch it -- use execvp and let the current environment pass through.
 * Nonzero (envc), we copy the current environment and then terminate it.
 */
static int ra_cmd_terminate(struct ra_cmd *cmd) {
  if (cmd->envc) {
    char **localp=environ;
    for (;*localp;localp++) if (ra_cmd_envv_append(cmd,*localp,-1)<0) return -1;
    if (ra_cmd_require(&cmd->envv,&cmd->envc,&cmd->enva)<0) return -1;
    cmd->envv[cmd->envc]=0;
  }
  if (ra_cmd_require(&cmd->argv,&cmd->argc,&cmd->arga)<0) return -1;
  cmd->argv[cmd->argc]=0;
  return 0;
}

/* Split command.
 */
 
static int ra_process_split_command(struct ra_cmd *cmd,const char *src) {
  if (!src) return -1;
  // Tokens before the executable that contain '=' are presumed to be environment variables.
  // We probably need to handle quoting, and this would be the place for it. Trying to avoid that.
  int readingenv=1;
  while (*src) {
    if ((unsigned char)(*src)<=0x20) { src++; continue; }
    const char *token=src;
    int tokenc=0,isenv=0;
    while ((unsigned char)token[tokenc]>0x20) {
      if (token[tokenc]=='=') isenv=1;
      tokenc++;
    }
    if (readingenv&&isenv) {
      if (ra_cmd_envv_append(cmd,token,tokenc)<0) return -1;
    } else {
      readingenv=0;
      if (ra_cmd_argv_append(cmd,token,tokenc)<0) return -1;
    }
    src+=tokenc;
  }
  // Assert that there is at least one arg -- argv[0] is required -- and terminate both lists.
  if (cmd->argc<1) return -1;
  if (ra_cmd_terminate(cmd)<0) return -1;
  return 0;
}

/* Launch command.
 */
 
static int ra_process_launch_command(struct ra_process *process,const char *cmdstr) {
  
  struct ra_cmd cmd={0};
  if (ra_process_split_command(&cmd,cmdstr)<0) {
    ra_cmd_cleanup(&cmd);
    return -1;
  }
  
  /* Fork the new process.
   */
  int pid=fork();
  if (pid<0) {
    fprintf(stderr,"%s: fork: %m\n",ra.exename);
    ra_cmd_cleanup(&cmd);
    return -1;
  }
  
  /* Parent? Record the pid and we're done.
   */
  if (pid) {
    process->pid=pid;
    ra_cmd_cleanup(&cmd);
    return 0;
  }
  
  /* We are the child.
   * TODO: Working directory? Capture stdout/stderr? Other housekeeping?
   */
  if (cmd.envc) execvpe(cmd.argv[0],cmd.argv,cmd.envv);
  else execvp(cmd.argv[0],cmd.argv);
  fprintf(stderr,"%s: execvp: %m\n",ra.exename);
  exit(1);
  return -1;
}

/* Update.
 */
 
int ra_process_update(struct ra_process *process) {

  /* If a process is running, check for termination.
   */
  if (process->pid) {
    int err,wstatus=0;
    err=waitpid(process->pid,&wstatus,WNOHANG);
    if (err>0) {
      if (WIFEXITED(wstatus)) {
        int status=WEXITSTATUS(wstatus);
        fprintf(stderr,"%s: Child process %d (gameid %d) exitted with status %d.\n",ra.exename,process->pid,process->gameid,status);
      } else {
        int sigid=0;
        if (WIFSIGNALED(wstatus)) sigid=WTERMSIG(wstatus);
        fprintf(stderr,"%s: Child process %d (gameid %d) abnormal exit. Signal %d (%s).\n",ra.exename,process->pid,process->gameid,sigid,strsignal(sigid));
      }
      if (process->gameid) db_play_finish(ra.db,process->gameid);
      if (!process->gameid&&!process->next_launch) process->menu_terminated=1;
      process->pid=0;
      if (!process->next_launch) process->gameid=0;
      ra_report_gameid(0);
    } else if (err<0) {
      fprintf(stderr,"%s:WARNING: waitpid() error. Assuming child process %d was lost somehow.\n",ra.exename,process->pid);
      process->pid=0;
      if (!process->next_launch) process->gameid=0;
      ra_report_gameid(0);
    }
  }
  
  /* If no process running, run in order of preference:
   *  - Game if requested (next_launch).
   *  - Menu if configured (ra.menu).
   *  - Nothing, leave it zero.
   */
  if (!process->pid) {
    if (process->next_launch) {
      if (ra_process_launch_command(process,process->next_launch)<0) {
        return -1;
      }
      free(process->next_launch);
      process->next_launch=0;
      ra_report_gameid(ra.process.gameid);
    } else if (process->menu_terminated) {
      // Wait for main to acknowledge, or quit if it decides to.
    } else if (ra.menu) {
      if (ra_process_launch_command(process,ra.menu)<0) {
        return -2;
      }
    }
  }

  return 0;
}

/* Status.
 */
 
int ra_process_get_status(const struct ra_process *process) {
  if (!process) return RA_PROCESS_STATUS_NONE;
  if (!process->pid) return RA_PROCESS_STATUS_NONE;
  if (process->gameid) return RA_PROCESS_STATUS_GAME;
  return RA_PROCESS_STATUS_MENU;
}

/* Resolve some variable from (cmd).
 * Variables begin with '$' and do not contain whitespace.
 */
 
static int ra_process_insert_command_variable(struct sr_encoder *dst,const char *src,int srcc,const char *path,int pathc,uint32_t gameid) {

  if ((srcc==5)&&!memcmp(src,"$FILE",5)) return sr_encode_raw(dst,path,pathc);
  
  if ((srcc>=9)&&!memcmp(src,"$COMMENT:",9)) {
    int addc=0;
    const char *k=src+9;
    int kc=srcc-9;
    int katom=db_string_lookup(ra.db,k,kc);
    if (katom>0) {
      struct db_comment *comment;
      int i=db_comments_get_by_gameid(&comment,ra.db,gameid);
      for (;i-->0;comment++) {
        if (comment->k==katom) {
          const char *v=0;
          int vc=db_string_get(&v,ra.db,comment->v);
          if (vc>0) {
            if (addc++) {
              if (sr_encode_raw(dst," ",1)<0) return -1;
            }
            if (sr_encode_raw(dst,v,vc)<0) return -1;
          }
        }
      }
    }
    return 0;
  }
      
  
  fprintf(stderr,"%s: Can't launch game %d! Unexpected launcher variable '%.*s'\n",ra.exename,gameid,srcc,src);
  return -1;
}

/* Insert (path) in (cmd), to a newly allocated string.
 */
 
static char *ra_process_combine_command(const char *cmd,int cmdc,const char *path,int pathc,uint32_t gameid) {
  struct sr_encoder dst={0};
  int cmdp=0;
  while (cmdp<cmdc) {
    if ((unsigned char)cmd[cmdp]<=0x20) { cmdp++; continue; }
    const char *token=cmd+cmdp;
    int tokenc=0;
    while ((cmdp<cmdc)&&((unsigned char)cmd[cmdp++]>0x20)) tokenc++;
    if (dst.c) {
      if (sr_encode_u8(&dst,' ')<0) {
        sr_encoder_cleanup(&dst);
        return 0;
      }
    }
    if (token[0]=='$') {
      if (ra_process_insert_command_variable(&dst,token,tokenc,path,pathc,gameid)<0) {
        sr_encoder_cleanup(&dst);
        return 0;
      }
    } else {
      if (sr_encode_raw(&dst,token,tokenc)<0) {
        sr_encoder_cleanup(&dst);
        return 0;
      }
    }
  }
  if (sr_encode_u8(&dst,0)<0) {
    sr_encoder_cleanup(&dst);
    return 0;
  }
  return dst.v;
}

/* Prepare launch.
 */
 
int ra_process_prepare_launch(
  struct ra_process *process,
  const char *cmd,int cmdc,
  const char *path,int pathc,
  uint32_t gameid
) {
  if (!process||!gameid) return -1;
  if (!cmd) return -1;
  if (cmdc<0) { cmdc=0; while (cmd[cmdc]) cmdc++; }
  if (!cmdc) return -1;
  if (!path) pathc=0; else if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  
  char *nv=ra_process_combine_command(cmd,cmdc,path,pathc,gameid);
  if (!nv) return -1;
  if (ra_process_restart_menu(process)<0) {
    free(nv);
    return -1;
  }
  if (process->next_launch) free(process->next_launch);
  process->next_launch=nv;
  process->gameid=gameid;
  
  return 0;
}

/* Current real time.
 */
 
static int64_t ra_process_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return tv.tv_sec*1000000ll+tv.tv_usec;
}

/* Terminate current process.
 */

int ra_process_terminate_game(struct ra_process *process) {
  if (!process->gameid) return 0;
  return ra_process_restart_menu(process);
}

int ra_process_restart_menu(struct ra_process *process) {
  if (!process->pid) return 0;
  if (kill(process->pid,SIGINT)<0) return -1;
  if (process->next_launch) {
    free(process->next_launch);
    process->next_launch=0;
  }
  return 0;
}

void ra_process_terminate_and_wait(struct ra_process *process,int toms) {
  if (!process->pid) return;
  if (kill(process->pid,SIGINT)<0) return;
  if (toms>0) {
    int64_t stoptime=ra_process_now()+toms*1000ll;
    while (1) {
      int err,wstatus=0;
      err=waitpid(process->pid,&wstatus,WNOHANG);
      if (err<0) break;
      if (err) {
        int status=WEXITSTATUS(wstatus);
        fprintf(stderr,"%s: Child process %d (gameid %d) exitted with status %d.\n",ra.exename,process->pid,process->gameid,status);
        process->pid=0;
        if (!process->next_launch) process->gameid=0;
        break;
      }
      int64_t now=ra_process_now();
      if (now>=stoptime) break;
      usleep(5000);
    }
  }
}
