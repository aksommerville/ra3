#include "sync_internal.h"

/* Cleanup.
 */
 
void syncsrv_cleanup(struct syncsrv *ss) {
  if (!ss) return;
  if (ss->remote_host) free(ss->remote_host);
  ssdb_cleanup(&ss->remote);
  ssdb_cleanup(&ss->local);
  fakews_del(ss->fakews);
}

/* Init.
 */
 
int syncsrv_init(struct syncsrv *ss) {
  ss->remote_port=43215;//XXX Should be 2600.
  if (syncsrv_set_remote_host(ss,"localhost",9)<0) return -1;//XXX Should be empty. "localhost" now, while I'm building it out.
  return 0;
}

/* Trivial accessors.
 */

int syncsrv_set_remote_host(struct syncsrv *ss,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (ss->remote_host) free(ss->remote_host);
  ss->remote_host=nv;
  return 0;
}

int syncsrv_set_remote_port(struct syncsrv *ss,int port) {
  if ((port<1)||(port>0xffff)) return -1;
  ss->remote_port=port;
  return 0;
}

const char *syncsrv_get_remote_host(const struct syncsrv *ss) {
  return ss->remote_host;
}

int syncsrv_get_remote_port(const struct syncsrv *ss) {
  return ss->remote_port;
}

/* Remote callbacks.
 */
 
static void ss_cb_connect(void *userdata) {
  struct syncsrv *ss=userdata;
  fprintf(stderr,"%s\n",__func__);
}
 
static void ss_cb_disconnect(void *userdata) {
  struct syncsrv *ss=userdata;
  fprintf(stderr,"%s\n",__func__);
}
 
static void ss_cb_message(int opcode,const void *v,int c,void *userdata) {
  struct syncsrv *ss=userdata;
  fprintf(stderr,"%s opcode=0x%02x c=%d\n",__func__,opcode,c);
}

/* Connect to remote.
 */
 
int syncsrv_connect_remote(struct syncsrv *ss) {
  if (ss->fakews) fakews_del(ss->fakews);
  if (!(ss->fakews=fakews_new(ss->remote_host,-1,ss->remote_port,"/ws/menu",-1,ss_cb_connect,ss_cb_disconnect,ss_cb_message,ss))) return -1;
  return 0;
}

/* Update.
 */
 
int syncsrv_update(struct syncsrv *ss) {
  if (ss->fakews) {
    if (fakews_update(ss->fakews,0)<0) return -1;
  }
  return 0;
}

/* Begin reading the remote database.
 */
 
int syncsrv_collect_remote(struct syncsrv *ss) {
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);
  return 0;
}

/* Begin reading the local database.
 */
 
int syncsrv_collect_local(struct syncsrv *ss) {
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);
  return 0;
}
