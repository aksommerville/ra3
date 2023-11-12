#include "ra_internal.h"
#include "opt/serial/serial.h"
#include "opt/png/png.h"
#include "opt/fs/fs.h"
#include <sys/socket.h>
#include "opt/http/http_dict.h"
#include "opt/http/http_xfer.h"
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
  
  // If any GAME clients exist, send to all of them.
  int gamec=0,i=RA_WEBSOCKET_LIMIT;
  const struct ra_websocket_extra *peer=ra.websocket_extrav;
  for (;i-->0;peer++) {
    if (!peer->socket) continue;
    if (peer->role==RA_WEBSOCKET_ROLE_GAME) {
      http_websocket_send(peer->socket,1,v,c);
      gamec++;
    }
  }
  if (gamec) return 0;
  
  // Send to all clients other than the sender.
  for (peer=ra.websocket_extrav,i=RA_WEBSOCKET_LIMIT;i-->0;peer++) {
    if (peer==extra) continue;
    if (!peer->socket) continue;
    http_websocket_send(peer->socket,1,v,c);
  }
  
  return 0;
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
  
  /* If any menu client requested a cap within the last second, send it to them and we're done.
   */
  const int64_t max_age=1000000;
  int64_t now=ra_now();
  int sent_to_menu=0,menuc=0;
  struct ra_websocket_extra *menu=ra.websocket_extrav;
  int i=RA_WEBSOCKET_LIMIT;
  for (;i-->0;menu++) {
    if (menu->role!=RA_WEBSOCKET_ROLE_MENU) continue;
    menuc++;
    if (now-menu->screencap_request_time<=max_age) {
      http_websocket_send(menu->socket,2,v,c);
      menu->screencap_request_time=0;
      sent_to_menu=1;
    }
  }
  if (sent_to_menu) return 0;
  
  /* If any menu is running, send it to all of them.
   * This is for screencaps initiated in game, we do have an assignable button for it.
   */
  if (menuc) {
    for (i=RA_WEBSOCKET_LIMIT,menu=ra.websocket_extrav;i-->0;menu++) {
      if (menu->role!=RA_WEBSOCKET_ROLE_MENU) continue;
      http_websocket_send(menu->socket,2,v,c);
      menu->screencap_request_time=0;
    }
    return 0;
  }
  
  /* If a game is running, and this came from a game, save this cap as a blob.
   */
  if (extra->role==RA_WEBSOCKET_ROLE_GAME) {
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
        return 0;
      }
    }
  }
  
  fprintf(stderr,"%s: Ignoring %d-byte screencap from WebSocket, I don't know where to send it.\n",ra.exename,c);
  return 0;
}

/* id="http"
 */

// Escapes, and produces the leading '?' if needed.
static int ra_ws_http_decode_query(char *dst,int dsta,struct sr_decoder *decoder) {
  int dstc=0;
  int jsonctx=sr_decode_json_object_start(decoder);
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,decoder))>0) {
    char v[256];
    int vc=sr_decode_json_string(v,sizeof(v),decoder);
    if ((vc<0)||(vc>sizeof(v))) return -1;
    
    if (dstc) {
      if (dstc<dsta) dst[dstc]='&';
    } else {
      if (dstc<dsta) dst[dstc]='?';
    }
    dstc++;
    
    int err=sr_url_encode(dst+dstc,dsta-dstc,k,kc);
    if (err<0) return -1;
    dstc+=err;
    
    if (dstc<dsta) dst[dstc]='=';
    dstc++;
    
    err=sr_url_encode(dst+dstc,dsta-dstc,v,vc);
    if (err<0) return -1;
    dstc+=err;
  }
  if (sr_decode_json_end(decoder,jsonctx)<0) return -1;
  return dstc;
}

static int ra_ws_http_decode_headers(struct http_xfer *req,struct sr_decoder *decoder) {
  int jsonctx=sr_decode_json_object_start(decoder);
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,decoder))>0) {
    char v[256];
    int vc=sr_decode_json_string(v,sizeof(v),decoder);
    if ((vc<0)||(vc>sizeof(v))) return -1;
    if (http_xfer_set_header(req,k,kc,v,vc)<0) return -1;
  }
  if (sr_decode_json_end(decoder,jsonctx)<0) return -1;
  return 0;
}

static int ra_ws_http_decode_request(struct http_xfer *req,const void *v,int c) {
  
  char method[32]="GET";
  int methodc=3;
  char path[256]="";
  int pathc=0;
  char query[256]="";
  int queryc=0;
  
  struct sr_decoder decoder={.v=v,.c=c};
  if (sr_decode_json_object_start(&decoder)<0) return -1;
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    
    if ((kc==6)&&!memcmp(k,"method",6)) {
      methodc=sr_decode_json_string(method,sizeof(method),&decoder);
      if ((methodc<0)||(methodc>sizeof(method))) return -1;
      continue;
    }
    
    if ((kc==4)&&!memcmp(k,"path",4)) {
      pathc=sr_decode_json_string(path,sizeof(path),&decoder);
      if ((pathc<0)||(pathc>sizeof(path))) return -1;
      continue;
    }
    
    if ((kc==5)&&!memcmp(k,"query",5)) {
      queryc=ra_ws_http_decode_query(query,sizeof(query),&decoder);
      if ((queryc<0)||(queryc>sizeof(query))) return -1;
      continue;
    }
    
    if ((kc==7)&&!memcmp(k,"headers",7)) {
      if (ra_ws_http_decode_headers(req,&decoder)<0) return -1;
      continue;
    }
    
    if ((kc==4)&&!memcmp(k,"body",4)) {
      char body[1024];
      int bodyc=sr_decode_json_string(body,sizeof(body),&decoder);
      if ((bodyc<0)||(bodyc>sizeof(body))) return -1;
      if (http_xfer_append_body(req,body,bodyc)<0) return -1;
      continue;
    }
    
    if (sr_decode_json_skip(&decoder)<0) return -1;
  }
  if (!methodc||!pathc) return -1;
  
  char line[600];
  int linec=snprintf(line,sizeof(line),"%.*s %.*s%.*s HTTP/1.1",methodc,method,pathc,path,queryc,query);
  if ((linec<0)||(linec>=sizeof(line))) return -1;
  if (http_xfer_set_line(req,line,linec)<0) return -1;
  
  return 0;
}

static int ra_ws_http_encode_response_text(struct sr_encoder *dst,struct http_xfer *rsp) {
  if (sr_encode_json_object_start(dst,0,0)<0) return -1;
  if (sr_encode_json_string(dst,"id",2,"httpresponse",12)<0) return -1;
  if (sr_encode_json_int(dst,"status",6,http_xfer_get_status(rsp))<0) return -1;
  
  const char *message=0;
  int messagec=http_xfer_get_status_message(&message,rsp);
  if (messagec<0) messagec=0;
  if (sr_encode_json_string(dst,"message",7,message,messagec)<0) return -1;
  
  /* Body: If it's already JSON (which should always be the case), don't encode as a JSON string, just dump the raw text.
   */
  const void *body=0;
  int bodyc=http_xfer_get_body(&body,rsp);
  if (bodyc<0) bodyc=0;
  const char *content_type=0;
  int content_typec=http_xfer_get_header(&content_type,rsp,"Content-Type",12);
  if ((content_typec==16)&&!memcmp(content_type,"application/json",16)) {
    if (sr_encode_fmt(dst,"\"body\":%.*s",bodyc,body)<0) return -1;
  } else {
    if (sr_encode_json_string(dst,"body",4,body,bodyc)<0) return -1;
  }
  
  int jsonctx=sr_encode_json_object_start(dst,"headers",7);
  if (jsonctx<0) return -1;
  const struct http_dict_entry *header=rsp->headers.v;
  int i=rsp->headers.c;
  for (;i-->0;header++) {
    if (sr_encode_json_string(dst,header->k,header->kc,header->v,header->vc)<0) return -1;
  }
  if (sr_encode_json_object_end(dst,jsonctx)<0) return -1;
  
  if (sr_encode_json_object_end(dst,0)<0) return -1;
  return 0;
}

static int ra_ws_http_encode_response(struct http_socket *socket,struct http_xfer *rsp) {
  struct sr_encoder dst={0};
  int err=ra_ws_http_encode_response_text(&dst,rsp);
  if (err>=0) err=http_websocket_send(socket,1,dst.v,dst.c);
  sr_encoder_cleanup(&dst);
  return err;
}
 
static int ra_ws_fake_http(
  struct http_xfer *req,
  struct http_xfer *rsp,
  const void *v,int c,
  struct ra_websocket_extra *extra
) {
  if (!req||!rsp) return -1;
  if (ra_ws_http_decode_request(req,v,c)<0) return -1;
  if (ra_http_api(req,rsp,0)<0) return -1;
  if (ra_ws_http_encode_response(extra->socket,rsp)<0) return -1;
  return 0;
}
 
static int ra_ws_rcv_http(struct ra_websocket_extra *extra,const void *v,int c) {
  struct http_xfer *req=http_xfer_new(ra.http);
  struct http_xfer *rsp=http_xfer_new(ra.http);
  int err=ra_ws_fake_http(req,rsp,v,c,extra);
  http_xfer_del(req);
  http_xfer_del(rsp);
  return err;
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
    _(http)
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
