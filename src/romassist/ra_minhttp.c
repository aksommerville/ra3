#include "ra_minhttp.h"
#include "opt/serial/serial.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>

/* Cleanup.
 */
 
void ra_minhttp_cleanup(struct ra_minhttp *mh) {
  if (mh->fd>0) close(mh->fd);
}

/* Init.
 */
 
int ra_minhttp_connect(struct ra_minhttp *mh,const char *host_and_port) {
  memset(mh,0,sizeof(struct ra_minhttp));
  mh->fd=-1;
  
  char zhost[256];
  int sepp=-1,i=0;
  for (;host_and_port[i];i++) {
    if (host_and_port[i]==':') {
      sepp=i;
      break;
    }
  }
  if (sepp<0) return -1;
  if (sepp>=sizeof(zhost)) return -1;
  memcpy(zhost,host_and_port,sepp);
  zhost[sepp]=0;
  const char *port=host_and_port+sepp+1;
  
  struct addrinfo hint={
    .ai_family=AF_INET,
    .ai_socktype=SOCK_STREAM,
    .ai_protocol=IPPROTO_TCP,
    .ai_flags=AI_NUMERICSERV,
  };
  struct addrinfo *ai=0;
  if (getaddrinfo(zhost,port,&hint,&ai)<0) return -1;
  if (!ai) return -1;
  
  if (
    ((mh->fd=socket(PF_INET,SOCK_STREAM,IPPROTO_TCP))<0)||
    (connect(mh->fd,ai->ai_addr,ai->ai_addrlen)<0)
  ) {
    freeaddrinfo(ai);
    if (mh->fd>=0) { close(mh->fd); mh->fd=-1; }
    return -1;
  }
  freeaddrinfo(ai);
  
  return 0;
}

/* Synchronous request.
 */
 
int ra_minhttp_request_sync(
  struct sr_encoder *rspheaders,
  struct sr_encoder *rspbody,
  struct ra_minhttp *mh,
  const char *method,
  const char *path
) {
  if (mh->fd<0) return -1;
  struct sr_encoder req={0};
  if (sr_encode_fmt(&req,"%s %s HTTP/1.1\r\n\r\n",method,path)<0) return -1;
  int err=write(mh->fd,req.v,req.c);
  sr_encoder_cleanup(&req);
  if (err<0) return -1;
  return ra_minhttp_await_response(rspheaders,rspbody,mh);
}

/* Read Status-Line.
 */
 
static int ra_minhttp_read_status_line(const char *src,int srcc) {
  if (!src||(srcc<1)) return -1;
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  // skip protocol...
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) srcp++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  // here's the status
  int status=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) {
    int digit=src[srcp++]-'0';
    if ((digit<0)||(digit>9)) return -1;
    status*=10;
    status+=digit;
    if (status>999) return -1;
  }
  return status;
}

/* Synchronous response.
 */
 
int ra_minhttp_await_response(
  struct sr_encoder *rspheaders,
  struct sr_encoder *rspbody,
  struct ra_minhttp *mh
) {
  char buf[8192];
  int bufp=0,bufc=0;
  int status=0;
  int headers_complete=0;
  int bodyc_outstanding=0;
  while (1) {
    if (mh->fd<0) return -1;
    if (bufp) {
      memmove(buf,buf+bufp,bufc);
      bufp=0;
    }
    int err=read(mh->fd,buf+bufp+bufc,sizeof(buf)-bufc-bufp);
    if (err<=0) {
      close(mh->fd);
      return -1;
    }
    bufc+=err;
    mh->rcvtotal+=err;
    
    if (!status) {
      const char *line=buf+bufp;
      int linec=0;
      while ((linec<bufc)&&(line[linec]!=0x0a)) linec++;
      if (linec>=bufc) continue; // Status-Line incomplete.
      if ((status=ra_minhttp_read_status_line(line,linec))<=0) return -1;
      linec++;
      bufp+=linec;
      bufc-=linec;
    }
    
    if (!headers_complete) {
      while (1) {
        const char *line=buf+bufp;
        int linec=0;
        while ((linec<bufc)&&(line[linec]!=0x0a)) linec++;
        if (linec>=bufc) break; // Incomplete header line.
        linec++;
        if (rspheaders) {
          if (sr_encode_raw(rspheaders,line,linec)<0) return -1;
        }
        // The only server we're going to talk to does not use chunked encoding, and always capitalizes "Content-Length".
        if ((linec>=15)&&!memcmp(line,"Content-Length:",15)) {
          const char *token=line+15;
          int tokenc=linec-15;
          while (tokenc&&((unsigned char)token[0]<=0x20)) { token++; tokenc--; }
          while (tokenc&&((unsigned char)token[tokenc-1]<=0x20)) tokenc--;
          if (sr_int_eval(&bodyc_outstanding,token,tokenc)<2) return -1;
        }
        bufp+=linec;
        bufc-=linec;
        if (linec==2) {
          headers_complete=1;
          break;
        }
      }
      if (!headers_complete) continue;
    }
    
    int cpc=bodyc_outstanding;
    if (bufc<cpc) cpc=bufc;
    if (rspbody) {
      if (sr_encode_raw(rspbody,buf+bufp,cpc)<0) return -1;
    }
    bufp+=cpc;
    bufc-=cpc;
    bodyc_outstanding-=cpc;
    if (bodyc_outstanding<=0) break;
  }
  return status;
}
