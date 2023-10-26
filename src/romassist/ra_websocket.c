#include "ra_internal.h"
#include "opt/serial/serial.h"
#include "opt/png/png.h"
#include "opt/fs/fs.h"
#include <sys/time.h>

/* Current time.
 */
 
static int64_t ra_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (int64_t)tv.tv_sec*1000000ll+tv.tv_usec;
}

/* Role.
 */
 
static const char *ra_ws_role_repr(int role) {
  switch (role) {
    case RA_WEBSOCKET_ROLE_MENU: return "MENU";
    case RA_WEBSOCKET_ROLE_GAME: return "GAME";
  }
  return "?";
}

/* Send a packet to all clients of a given role.
 */
 
static int ra_websocket_send_to_role(int role,int packet_type,const void *v,int c) {
  if (role==RA_WEBSOCKET_ROLE_NONE) return 0;
  const struct ra_websocket_extra *extra=ra.websocket_extrav;
  int i=RA_WEBSOCKET_LIMIT;
  for (;i-->0;extra++) {
    if (extra->role!=role) continue;
    if (!extra->socket) continue;
    http_websocket_send(extra->socket,packet_type,v,c);
  }
  return 0;
}

/* id="hello"
 */
 
static int ra_ws_rcv_hello(struct ra_websocket_extra *extra,const void *v,int c) {
  return 0;
}
    
/* id="game"
 */
 
static int ra_ws_rcv_game(struct ra_websocket_extra *extra,const void *v,int c) {
  if (extra->role!=RA_WEBSOCKET_ROLE_GAME) return 0;
  return ra_websocket_send_to_role(RA_WEBSOCKET_ROLE_MENU,1,v,c);
}
    
/* id="requestScreencap"
 */
 
static int ra_ws_rcv_requestScreencap(struct ra_websocket_extra *extra,const void *v,int c) {
  if (extra->role!=RA_WEBSOCKET_ROLE_MENU) return 0;
  extra->screencap_request_time=ra_now();
  return ra_websocket_send_to_role(RA_WEBSOCKET_ROLE_GAME,1,v,c);
}
    
/* id="pause"
 */
 
static int ra_ws_rcv_pause(struct ra_websocket_extra *extra,const void *v,int c) {
  if (extra->role!=RA_WEBSOCKET_ROLE_MENU) return 0;
  return ra_websocket_send_to_role(RA_WEBSOCKET_ROLE_GAME,1,v,c);
}
    
/* id="resume"
 */
 
static int ra_ws_rcv_resume(struct ra_websocket_extra *extra,const void *v,int c) {
  if (extra->role!=RA_WEBSOCKET_ROLE_MENU) return 0;
  return ra_websocket_send_to_role(RA_WEBSOCKET_ROLE_GAME,1,v,c);
}
    
/* id="step"
 */
 
static int ra_ws_rcv_step(struct ra_websocket_extra *extra,const void *v,int c) {
  if (extra->role!=RA_WEBSOCKET_ROLE_MENU) return 0;
  return ra_websocket_send_to_role(RA_WEBSOCKET_ROLE_GAME,1,v,c);
}
    
/* id="comment"
 */
 
static int ra_ws_rcv_comment(struct ra_websocket_extra *extra,const void *v,int c) {
  if (extra->role!=RA_WEBSOCKET_ROLE_GAME) return 0;
  //TODO Consider saving comment to db.
  return ra_websocket_send_to_role(RA_WEBSOCKET_ROLE_MENU,1,v,c);
}

/* Binary: PNG
 */
 
static int ra_ws_rcv_png(struct ra_websocket_extra *extra,const void *v,int c) {
  if (extra->role!=RA_WEBSOCKET_ROLE_GAME) return 0;
  
  /* If any menu client requested a cap within the last second, send it to them and we're done.
   */
  const int64_t max_age=1000000;
  int64_t now=ra_now();
  int sent_to_menu=0;
  struct ra_websocket_extra *menu=ra.websocket_extrav;
  int i=RA_WEBSOCKET_LIMIT;
  for (;i-->0;menu++) {
    if (menu->role!=RA_WEBSOCKET_ROLE_MENU) continue;
    if (now-menu->screencap_request_time<=max_age) {
      http_websocket_send(menu->socket,2,v,c);
      menu->screencap_request_time=0;
      sent_to_menu=1;
    }
  }
  if (sent_to_menu) return 0;
  
  /* If a game is running, save this cap as a blob.
   */
  if ((ra_process_get_status(&ra.process)==RA_PROCESS_STATUS_GAME)&&ra.process.gameid) {
    char *blobpath=db_blob_compose_path(ra.db,ra.process.gameid,"scap",4,".png",4);
    if (blobpath) {
      if (file_write(blobpath,v,c)>=0) {
        db_invalidate_blobs_for_gameid(ra.db,ra.process.gameid);
        fprintf(stderr,"%s: Saved screencap.\n",blobpath);
      } else {
        fprintf(stderr,"%s: Failed to write file, %d bytes.\n",blobpath,c);
      }
      free(blobpath);
    }
  }
  
  fprintf(stderr,"%s: Ignoring %d-byte screencap from WebSocket, I don't know where to send it.\n",ra.exename,c);
  return 0;
}

/* Send any initial content to a WebSocket we just accepted.
 */
 
static int ra_ws_compose_handshake(struct ra_websocket_extra *extra) {
  switch (extra->role) {
  
    case RA_WEBSOCKET_ROLE_MENU: {
        if (ra.gameid_reported) {
          struct sr_encoder encoder={0};
          sr_encode_json_object_start(&encoder,0,0);
          sr_encode_json_string(&encoder,"id",2,"game",4);
          sr_encode_json_int(&encoder,"gameid",6,ra.gameid_reported);
          if (sr_encode_json_object_end(&encoder,0)>=0) {
            http_websocket_send(extra->socket,1,encoder.v,encoder.c);
          }
          sr_encoder_cleanup(&encoder);
        }
      } break;
      
  }
  return 0;
}

/* New WebSocket connection.
 */
 
static int ra_ws_connect(struct http_socket *sock,int role) {
  struct ra_websocket_extra *extra=0;
  struct ra_websocket_extra *q=ra.websocket_extrav;
  int i=RA_WEBSOCKET_LIMIT;
  for (;i-->0;q++) {
    if (q->role==RA_WEBSOCKET_ROLE_NONE) {
      extra=q;
      break;
    }
  }
  if (!extra) {
    fprintf(stderr,"%s:WARNING: Rejecting WebSocket connection because too many are open.\n",ra.exename);
    http_websocket_close(sock);
    return 0;
  }
  memset(extra,0,sizeof(struct ra_websocket_extra));
  extra->role=role;
  extra->socket=sock;
  http_socket_set_userdata(sock,extra);
  fprintf(stderr,"%s: Accepted %s WebSocket connection.\n",ra.exename,ra_ws_role_repr(role));
  return ra_ws_compose_handshake(extra);
}
 
int ra_ws_connect_menu(struct http_socket *sock,void *userdata) {
  return ra_ws_connect(sock,RA_WEBSOCKET_ROLE_MENU);
}
 
int ra_ws_connect_game(struct http_socket *sock,void *userdata) {
  return ra_ws_connect(sock,RA_WEBSOCKET_ROLE_GAME);
}

/* Lost WebSocket connection.
 */

int ra_ws_disconnect(struct http_socket *sock,void *userdata) {
  struct ra_websocket_extra *extra=http_socket_get_userdata(sock);
  if (extra&&(extra->role!=RA_WEBSOCKET_ROLE_NONE)) {
    fprintf(stderr,"%s: Lost %s WebSocket connection.\n",ra.exename,ra_ws_role_repr(extra->role));
    extra->role=RA_WEBSOCKET_ROLE_NONE;
    extra->socket=0;
  }
  return 0;
}

/* Extract "id" from JSON as a string.
 */
 
static int ra_ws_extra_json_id(char *dst,int dsta,const void *v,int c) {
  struct sr_decoder decoder={.v=v,.c=c};
  if (sr_decode_json_object_start(&decoder)<0) return -1;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    if ((kc==2)&&!memcmp(k,"id",2)) {
      return sr_decode_json_string(dst,dsta,&decoder);
    } else {
      if (sr_decode_json_skip(&decoder)<0) return -1;
    }
  }
  return -1;
}

/* Content received via WebSocket.
 * General policy is to quietly ignore malformed packets.
 */

int ra_ws_message(struct http_socket *sock,int type,const void *v,int c,void *userdata) {
  struct ra_websocket_extra *extra=http_socket_get_userdata(sock);
  if (!extra||(extra->socket!=sock)) return 0;
  
  /* Most packets are text (type 1), which text is JSON with a member "id".
   */
  if (type==1) {
    char id[32];
    int idc=ra_ws_extra_json_id(id,sizeof(id),v,c);
    if ((idc<1)||(idc>sizeof(id))) {
      fprintf(stderr,"%s: Discarding %d bytes text via WebSocket from %s: Malformed JSON or invalid id.\n",ra.exename,c,ra_ws_role_repr(extra->role));
      return 0;
    }
    
    #define _(tag) if ((idc==sizeof(#tag)-1)&&!memcmp(id,#tag,idc)) return ra_ws_rcv_##tag(extra,v,c);
    _(hello)
    _(game)
    _(requestScreencap)
    _(pause)
    _(resume)
    _(step)
    _(comment)
    #undef _
    
    fprintf(stderr,"%s: Unknown WebSocket packet ID '%.*s' from %s client.\n",ra.exename,idc,id,ra_ws_role_repr(extra->role));
    return 0;
  }
  
  /* Binary packets (type 2) can be screencaps.
   * Maybe other uses in the future. Will distinguish by data type, which must be detectable.
   */
  if (type==2) {
    if ((c>=8)&&!memcmp(v,"\x89PNG\r\n\x1a\n",8)) return ra_ws_rcv_png(extra,v,c);
    fprintf(stderr,"%s: Ignoring %d-byte binary WebSocket message from %s client, unknown format.\n",ra.exename,c,ra_ws_role_repr(extra->role));
    return 0;
  }
  
  return 0;
}

/* Notify of changed gameid.
 */
 
int ra_report_gameid(uint32_t gameid) {
  if (gameid==ra.gameid_reported) return 0;
  ra.gameid_reported=gameid;
  struct sr_encoder encoder={0};
  sr_encode_json_object_start(&encoder,0,0);
  sr_encode_json_string(&encoder,"id",2,"game",4);
  sr_encode_json_int(&encoder,"gameid",6,gameid);
  if (sr_encode_json_object_end(&encoder,0)>=0) {
    ra_websocket_send_to_role(RA_WEBSOCKET_ROLE_MENU,1,encoder.v,encoder.c);
  }
  sr_encoder_cleanup(&encoder);
  return 0;
}
