#include "../mn_internal.h"
#include "opt/serial/serial.h"
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>

/* Object definition.
 */
 
struct mn_widget_network {
  struct gui_widget hdr;
};

#define WIDGET ((struct mn_widget_network*)widget)

/* Cleanup.
 */
 
static void _network_del(struct gui_widget *widget) {
}

/* Form callback.
 */
 
static void network_cb_form(struct gui_widget *form,const char *k,int kc,const char *v,int vc,void *userdata) {
  struct gui_widget *widget=userdata;
  fprintf(stderr,"%s '%.*s' = '%.*s'\n",__func__,kc,k,vc,v);
}

/* Get this host's IP address as a string.
 * This will be the wrong answer, if you're running menu on a different host than romassist.
 * Our design is that menu and romassist run on the same host.
 */
 
static int network_get_my_ip_address(char *dst,int dsta) {
  struct ifaddrs *ifaddrs=0;
  if (getifaddrs(&ifaddrs)<0) {
    fprintf(stderr,"ERROR: %m\n");
    return -1;
  }
  uint8_t tmp[4]={0};
  struct ifaddrs *ifa=ifaddrs;
  for (;ifa;ifa=ifa->ifa_next) {
    if (ifa->ifa_addr) {
      struct sockaddr_in *sin=(struct sockaddr_in*)ifa->ifa_addr;
      if (sin->sin_family==AF_INET) {
        const uint8_t *B=(uint8_t*)&sin->sin_addr.s_addr;
        if (B[0]!=127) {
          memcpy(tmp,B,4);
          break;
        }
      }
    }
  }
  freeifaddrs(ifaddrs);
  if (!tmp[0]) return 0;
  int dstc=0,tmpp=0;
  for (;tmpp<4;tmpp++) {
    if (dstc) {
      if (dstc<dsta) dst[dstc]='.';
      dstc++;
    }
    dstc+=sr_decuint_repr(dst+dstc,dsta-dstc,tmp[tmpp],1);
  }
  return dstc;
}

/* Init.
 */
 
static int _network_init(struct gui_widget *widget) {
  struct gui_widget *form=gui_widget_spawn(widget,&gui_widget_type_form);
  if (!form) return -1;
  gui_widget_form_set_callback(form,network_cb_form,widget);
  
  //gui_widget_form_add_string(form,"Show Invalid Games?",19,mn.show_invalid?"Show":"Hide",4,1);
  
  char url[256];
  memcpy(url,"http://",7);
  int urlc=7;
  int addrc=network_get_my_ip_address(url+urlc,sizeof(url)-urlc);
  if ((addrc>0)&&(urlc+addrc<=sizeof(url))) {
    urlc+=addrc;
  } else {
    memcpy(url+urlc,"localhost",9);
    urlc+=9;
  }
  memcpy(url+urlc,":2600",5);//TODO get from emuhost
  urlc+=5;
  gui_widget_form_add_readonly_string(form,"URL",3,url,urlc);
  
  return 0;
}

/* Let form control most ops.
 */
 
static void _network_measure(int *w,int *h,struct gui_widget *widget,int wlimit,int hlimit) {
  if (widget->childc!=1) return;
  gui_widget_measure(w,h,widget->childv[0],wlimit,hlimit);
}
 
static void _network_pack(struct gui_widget *widget) {
  if (widget->childc!=1) return;
  struct gui_widget *form=widget->childv[0];
  form->x=0;
  form->y=0;
  form->w=widget->w;
  form->h=widget->h;
  gui_widget_pack(form);
}
 
static void _network_draw(struct gui_widget *widget,int x,int y) {
  gui_draw_rect(widget->gui,x,y,widget->w,widget->h,0x003000ff);
  int i=0;
  for (;i<widget->childc;i++) {
    struct gui_widget *child=widget->childv[i];
    gui_widget_draw(child,x+child->x,y+child->y);
  }
}
 
static void _network_motion(struct gui_widget *widget,int dx,int dy) {
  if (widget->childc!=1) return;
  struct gui_widget *form=widget->childv[0];
  if (form->type->motion) form->type->motion(form,dx,dy);
}
 
static void _network_signal(struct gui_widget *widget,int sigid) {
  if (sigid==GUI_SIGID_CANCEL) {
    gui_dismiss_modal(widget->gui,widget);
  } else {
    if (widget->childc!=1) return;
    struct gui_widget *form=widget->childv[0];
    if (form->type->signal) form->type->signal(form,sigid);
  }
}

/* Type definition.
 */
 
const struct gui_widget_type mn_widget_type_network={
  .name="network",
  .objlen=sizeof(struct mn_widget_network),
  .del=_network_del,
  .init=_network_init,
  .measure=_network_measure,
  .pack=_network_pack,
  .draw=_network_draw,
  .motion=_network_motion,
  .signal=_network_signal,
};
