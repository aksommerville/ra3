#include "mn_internal.h"
#include "opt/serial/serial.h"

struct mn mn={0};

/* Update.
 */
 
static int mn_update() {

  if (mn.rebuild_gui) {
    mn.rebuild_gui=0;
    if (!gui_replace_page(mn.gui,&mn_widget_type_home)) return -1;
    gui_dirty_pack(mn.gui);
    gui_dirty_video(mn.gui);
  }

  gui_update(mn.gui,eh_input_get(0));
          
  int winw=0,winh=0;
  eh_video_get_size(&winw,&winh);
  eh_video_begin();
  gui_render(mn.gui,winw,winh);
  eh_video_end();

  return 0;
}

/* Audio.
 */

static struct cheapsynth_sound_config mn_sound_ACTIVATE={
  .id=GUI_SFXID_ACTIVATE,
  .master=0.250f,
  .modrate=2.0f,
  .modabsolute=0,
  .level={{25,1.0f},{40,0.20f},{300,0.0f}},
  .pitch={{0,500.0f},{370,500.0f}},
  .mod={{0,0.0f},{35,2.0f},{300,1.0f}},
};

static struct cheapsynth_sound_config mn_sound_MINOR_OK={
  .id=GUI_SFXID_MINOR_OK,
  .master=0.200f,
  .modrate=2.0f,
  .modabsolute=0,
  .level={{20,1.0f},{30,0.15f},{200,0.0f}},
  .pitch={{0,800.0f},{250,1500.0f}},
  .mod={{0,1.0f},{250,5.0f}},
};

static struct cheapsynth_sound_config mn_sound_MOTION={
  .id=GUI_SFXID_MOTION,
  .master=0.100f,
  .modrate=3.0f,
  .modabsolute=0,
  .level={{20,1.0f},{30,0.10f},{150,0.0f}},
  .pitch={{0,440.0f},{100,700.0f},{100,400.0f}},
  .mod={{0,0.5f},{40,2.0f},{160,1.0f}},
};

static struct cheapsynth_sound_config mn_sound_CANCEL={
  .id=GUI_SFXID_CANCEL,
  .master=0.200f,
  .modrate=0.500f,
  .modabsolute=0,
  .level={{20,1.0f},{40,0.30f},{200,0.0f}},
  .pitch={{0,180.0f},{260,400.0f}},
  .mod={{0,3.0f},{260,0.0f}},
};

static struct cheapsynth_sound_config mn_sound_REJECT={
  .id=GUI_SFXID_REJECT,
  .master=0.150f,
  .modrate=1.3f,
  .modabsolute=0,
  .level={{30,1.0f},{40,0.40f},{200,0.0f}},
  .pitch={{0,300.0f},{260,200.0f}},
  .mod={{0,3.0f},{30,5.0f},{260,2.0f}},
};

static void mn_generate_pcm(void *v,int c) {
  cheapsynth_updatei(v,c,mn.cheapsynth);
}

void mn_cb_sound_effect(int sfxid,void *userdata) {
  struct cheapsynth_sound *sound=0;
  switch (sfxid) {
    #define _(tag) case GUI_SFXID_##tag: sound=cheapsynth_sound_intern(mn.cheapsynth,&mn_sound_##tag); break;
    _(ACTIVATE)
    _(MINOR_OK)
    _(MOTION)
    _(CANCEL)
    _(REJECT)
    #undef _
  }
  if (!sound) return;
  if (eh_audio_lock()<0) return;
  cheapsynth_sound_play(mn.cheapsynth,sound);
  eh_audio_unlock();
}

/* Get data path.
 */
 
static char mn_data_path_tmp[1024];

const char *mn_data_path(const char *basename) {
  if (!mn.data_path||!basename) return 0;
  int bc=0;
  while (basename[bc]) bc++;
  if (mn.data_pathc+1+bc>=sizeof(mn_data_path_tmp)) return 0;
  memcpy(mn_data_path_tmp,mn.data_path,mn.data_pathc);
  mn_data_path_tmp[mn.data_pathc]='/';
  memcpy(mn_data_path_tmp+mn.data_pathc+1,basename,bc+1);
  return mn_data_path_tmp;
}

/* Set (mn.data_path).
 */
 
static int mn_choose_data_path() {
  
  /* (mn.exename) should contain "/ra3/" somewhere, and if so, we want thru that.
   * If not, roll the dice on the working directory.
   */
  int okc=0;
  const char *p=mn.exename;
  for (;*p;p++) {
    if (!memcmp(p,"/ra3/",5)) {
      okc=p-mn.exename+5;
      break;
    }
  }
  if (okc) {
    mn.data_pathc=okc+13;
    if (!(mn.data_path=malloc(mn.data_pathc+1))) return -1;
    memcpy(mn.data_path,mn.exename,okc);
    memcpy(mn.data_path+okc,"src/menu/data",14);
  } else {
    mn.data_pathc=13;
    if (!(mn.data_path=strdup("src/menu/data"))) return -1;
  }
  
  return 0;
}

/* Initialize.
 */
 
static int mn_load_none() {
  
  if (mn_choose_data_path()<0) return -1;
  
  dbs_init(&mn.dbs);
  dbs_refresh_search(&mn.dbs);
  dbs_refresh_all_metadata(&mn.dbs);

  struct gui_delegate delegate={
    .userdata=0,
    .cb_sound_effect=mn_cb_sound_effect,
  };
  if (!(mn.gui=gui_new(&delegate,mn.data_path,mn.data_pathc))) return -1;
  struct gui_widget *home=gui_replace_page(mn.gui,&mn_widget_type_home);
  if (!home) return -1;
  
  if (eh_audio_get_format()!=EH_AUDIO_FORMAT_S16N) return -1;
  if (!(mn.cheapsynth=cheapsynth_new(eh_audio_get_rate(),eh_audio_get_chanc()))) return -1;

  return 0;
}

/* Receive HTTP response over the fake WebSocket.
 */
 
static void mn_http_response(const char *src,int srcc) {
  dbs_http_response(&mn.dbs,src,srcc);
}

static void mn_websocket_incoming(const char *id,int idc,const char *src,int srcc) {
  //fprintf(stderr,"%s: %.*s\n",__func__,srcc,src);
  
  if ((idc==7)&&!memcmp(id,"upgrade",7)) {
    char text[256];
    int textc=0;
    char running[32];
    int runningc=0;
    int pendingc=0;
    int status=INT_MIN;
    int finished=1;
    struct sr_decoder decoder={.v=src,.c=srcc};
    const char *k;
    int kc;
    sr_decode_json_object_start(&decoder);
    while ((kc=sr_decode_json_next(&k,&decoder))>0) {
      if ((kc!=2)||memcmp(k,"id",2)) finished=0;
    
      if ((kc==7)&&!memcmp(k,"pending",7)) {
        int pendingctx=sr_decode_json_array_start(&decoder);
        if (pendingctx<0) return;
        pendingc=0;
        while (sr_decode_json_next(0,&decoder)>0) {
          if (sr_decode_json_skip(&decoder)<0) break;
          pendingc++;
        }
        if (sr_decode_json_end(&decoder,pendingctx)<0) return;
        
      } else if ((kc==7)&&!memcmp(k,"running",7)) {
        running[0]='?';
        runningc=1;
        int runctx=sr_decode_json_object_start(&decoder);
        if (runctx<0) return;
        const char *runk;
        int runkc;
        while ((runkc=sr_decode_json_next(&runk,&decoder))>0) {
          if ((runkc==11)&&!memcmp(runk,"displayName",11)) {
            runningc=sr_decode_json_string(running,sizeof(running),&decoder);
            if ((runningc<0)||(runningc>sizeof(running))) {
              if (sr_decode_json_skip(&decoder)<0) return;
              running[0]='?';
              runningc=1;
            }
          } else {
            if (sr_decode_json_skip(&decoder)<0) return;
          }
        }
        if (sr_decode_json_end(&decoder,runctx)<0) return;
        
      } else if ((kc==4)&&!memcmp(k,"text",4)) {
        textc=sr_decode_json_string(text,sizeof(text),&decoder);
        if ((textc<0)||(textc>sizeof(text))) {
          if (sr_decode_json_skip(&decoder)<0) return;
          text[0]='?';
          textc=1;
        } else if (!textc) {
          text[0]='?';
          textc=1;
        }
        
      } else if ((kc==6)&&!memcmp(k,"status",6)) {
        if (sr_decode_json_int(&status,&decoder)<0) return;
        
      } else {
        if (sr_decode_json_skip(&decoder)<0) return;
      }
    }
    //fprintf(stderr,"websocket:upgrade: finished=%d pendingc=%d running='%.*s' status=%d text='%.*s'\n",finished,pendingc,runningc,running,status,textc,text);
    //TODO How much of this data do we want to display, and how?
    mn.upgrade_in_progress=finished?0:1;
    if ((status>INT_MIN)&&(status!=0)) mn.upgrade_status=status;
  }
}

/* Configure.
 */

static int mn_configure(const char *k,int kc,const char *v,int vc,int vn) {
  if ((kc==5)&&!memcmp(k,"kiosk",5)) {
    mn.kiosk=1;
    return 1;
  }
  return 0;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  struct eh_delegate delegate={
    .name="Romassist",
    .video_width=0, // 0,0 to request an OpenGL context at variable resolution.
    .video_height=0,
    .video_format=0,
    .video_rate=0, // 0=don't care
    .audio_rate=0, // 0=don't care
    .audio_chanc=1,
    .audio_format=EH_AUDIO_FORMAT_S16N,
    .playerc=1,
    .configure=mn_configure,
    .load_file=0, // no need
    .load_serial=0, // no need
    .load_none=mn_load_none,
    .update=mn_update,
    .generate_pcm=mn_generate_pcm,
    .reset=0, // no need
    .use_menu_role=1,
    .http_response=mn_http_response,
    .websocket_incoming=mn_websocket_incoming,
  };
  mn.exename=(argc>=0)?argv[0]:0;
  if (!mn.exename||!mn.exename[0]) mn.exename=delegate.name;
  return eh_main(argc,argv,&delegate);
}
