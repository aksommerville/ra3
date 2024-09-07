#ifndef SYNC_INTERNAL_H
#define SYNC_INTERNAL_H

#include "menu/mn_internal.h"
#include "opt/serial/serial.h"
#include "opt/fakews/fakews.h"

#define SYNC_STAGE_HOST  1 /* Collecting remote host address. */
#define SYNC_STAGE_SETUP 2 /* Collecting pre-run parameters. */
#define SYNC_STAGE_RUN   3 /* In progress. */
#define SYNC_STAGE_DONE  4 /* Reporting summary (success). */
#define SYNC_STAGE_FAIL  5 /* Reporting summary (failure). */

#define SYNC_ACCESS_READ 0
#define SYNC_ACCESS_WRITE 1
#define SYNC_ACCESS_RW 2

const char *sync_access_repr(int access);
const char *sync_no_ask_yes_repr(int v);

/* ssdb: View of one of the databases.
 ************************************************************************/
 
struct ssdb {
  int TODO;
};

void ssdb_cleanup(struct ssdb *ssdb);

/* syncsrv: Service model.
 *************************************************************************/
 
struct syncsrv {
  
  char *remote_host;
  int remote_port;
  
  int access; // SYNC_ACCESS_*
  int add_games; // -1,0,1=no,ask,yes
  int delete_games; // ''
  int add_screencaps; // ''
  int add_plays; // ''
  int add_launchers; // ''
  
  struct fakews *fakews; // To remote. The local one is already open, by libemuhost.
  struct ssdb local;
  struct ssdb remote;
};

void syncsrv_cleanup(struct syncsrv *ss);
int syncsrv_init(struct syncsrv *ss);

int syncsrv_set_remote_host(struct syncsrv *ss,const char *src,int srcc);
int syncsrv_set_remote_port(struct syncsrv *ss,int port);
const char *syncsrv_get_remote_host(const struct syncsrv *ss);
int syncsrv_get_remote_port(const struct syncsrv *ss);

int syncsrv_connect_remote(struct syncsrv *ss);
int syncsrv_collect_remote(struct syncsrv *ss);
int syncsrv_collect_local(struct syncsrv *ss);

int syncsrv_update(struct syncsrv *ss);

#endif
