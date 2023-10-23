/* ra_process.h
 * Manages the foreground process. There is never more than one at a time.
 */

#ifndef RA_PROCESS_H
#define RA_PROCESS_H

struct ra_process {

  /* If nonzero, we will try to launch this the next time our child quits.
   * Once launched, we null it out again.
   */
  char *next_launch;
  
  /* If nonzero, the pid of our child process.
   * Can be a game or the menu, and can also linger on zero. (eg if no menu is requested).
   */
  int pid;
  
  /* If nonzero, the requested gameid.
   * This is set during the request stage (next_launch not null), and also while running.
   */
  uint32_t gameid;
};

void ra_process_cleanup(struct ra_process *process);

/* OK to call irregularly, say every second or so.
 * The only possible error is failure to launch something pending.
 * -1 if a game fails to launch -- you probably want to ra_process_restart_menu() in this case.
 * -2 if the menu fails to launch -- you probably want to abort in this case.
 * If you take no action after an error, we'll try the same thing again next time.
 */
int ra_process_update(struct ra_process *process);

/* What is currently running?
 * We don't care about pending requests, only the reality right now.
 */
int ra_process_get_status(const struct ra_process *process);
#define RA_PROCESS_STATUS_NONE 0
#define RA_PROCESS_STATUS_MENU 1
#define RA_PROCESS_STATUS_GAME 2

/* Given (cmd) from the launcher and (path) from the game, prepare to launch this game.
 * On success, we signal the current process to quit, set (next_launch), and return >=0.
 * The new process is not actually launched at this time; it will happen during some subsequent update.
 * On failures, our internal state does not change.
 * (gameid) is for bookkeeping only.
 */
int ra_process_prepare_launch(
  struct ra_process *process,
  const char *cmd,int cmdc,
  const char *path,int pathc,
  uint32_t gameid
);

/* Begin the work of terminating the child process.
 * These are essentially the same thing, but ra_process_terminate_game() checks that it actually is a game,
 * and does nothing if the menu is already running.
 * Quietly noops if no child process is running.
 */
int ra_process_terminate_game(struct ra_process *process);
int ra_process_restart_menu(struct ra_process *process);

#endif
