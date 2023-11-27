#include "ra_internal.h"
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Cleanup.
 */
 
void ra_process_cleanup(struct ra_process *process) {
  if (process->next_launch) free(process->next_launch);
}

/* Launch command.
 */
 
static int ra_process_launch_command(struct ra_process *process,const char *cmd) {
  
  /* Allocate and populate argv.
   */
  int arga=8,argc=0,cmdp=0;
  char **argv=malloc(sizeof(void*)*arga);
  if (!argv) return -1;
  while (cmd[cmdp]) {
    if ((unsigned char)cmd[cmdp]<=0x20) { cmdp++; continue; }
    //TODO Should we allow quoted args? Might be necessary if the user has files with spaces in the names.
    const char *token=cmd+cmdp;
    int tokenc=0;
    while (cmd[cmdp]&&((unsigned char)cmd[cmdp]>0x20)) { cmdp++; tokenc++; }
    if (argc>=arga-1) {
      arga+=8;
      void *nv=realloc(argv,sizeof(void*)*arga);
      if (!nv) {
        while (argc-->0) free(argv[argc]);
        free(argv);
        return -1;
      }
      argv=nv;
    }
    if (!(argv[argc]=malloc(tokenc+1))) {
      while (argc-->0) free(argv[argc]);
      free(argv);
      return -1;
    }
    memcpy(argv[argc],token,tokenc);
    argv[argc][tokenc]=0;
    argc++;
  }
  if (!argc) {
    free(argv);
    return -1;
  }
  argv[argc]=0;
  
  /* Fork the new process.
   */
  int pid=fork();
  if (pid<0) {
    fprintf(stderr,"%s: fork: %m\n",ra.exename);
    while (argc-->0) free(argv[argc]);
    free(argv);
    return -1;
  }
  
  /* Parent? Record the pid and we're done.
   */
  if (pid) {
    process->pid=pid;
    while (argc-->0) free(argv[argc]);
    free(argv);
    return 0;
  }
  
  /* We are the child.
   * TODO: Working directory? Environment? Capture stdout/stderr? Other housekeeping?
   */
  execvp(argv[0],argv);
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

/* Insert (path) in (cmd), to a newly allocated string.
 */
 
static char *ra_process_combine_command(const char *cmd,int cmdc,const char *path,int pathc) {
  int insp=0,insc=0;
  int cmdp=cmdc-5;
  while (cmdp>=0) {
    if (!memcmp(cmd+cmdp,"$FILE",5)) {
      insp=cmdp;
      insc=5;
      break;
    }
    cmdp--;
  }
  if (insc&&!pathc) return 0;
  if (!insc) pathc=0;
  int nc=cmdc-insc+pathc;
  char *nv=malloc(nc+1);
  if (!nv) return 0;
  memcpy(nv,cmd,insp);
  memcpy(nv+insp,path,pathc);
  memcpy(nv+insp+pathc,cmd+insp+insc,cmdc-insc-insp);
  nv[nc]=0;
  return nv;
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
  
  char *nv=ra_process_combine_command(cmd,cmdc,path,pathc);
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
