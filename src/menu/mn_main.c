#include "mn_internal.h"

struct mn mn={0};

/* Update.
 */
 
static int XXX_updatec=0;
 
static int mn_update() {

  XXX_updatec++;
  if (XXX_updatec%1000==1) {
    fprintf(stderr,"%s:%d: %d frames\n",__FILE__,__LINE__,XXX_updatec);
  }

  gui_update(mn.gui,eh_input_get(0));
  if (XXX_updatec%1000==1) {
    fprintf(stderr,"%s:%d: %d frames\n",__FILE__,__LINE__,XXX_updatec);
  }
          
  int winw=0,winh=0;
  eh_video_get_size(&winw,&winh);
  eh_video_begin();
  gui_render(mn.gui,winw,winh);
  eh_video_end();
  if (XXX_updatec%1000==1) {
    fprintf(stderr,"%s:%d: %d frames\n",__FILE__,__LINE__,XXX_updatec);
  }

  return 0;
}

/* Generate PCM.
 */
 
//XXX world's cheapest synthesizer, just to ensure signals are passing as expecting.
static int synth_phase=0;
static int synth_halfwavelength=100;
static int synth_level=10000;
static int synth_ttl=0;
 
static void mn_generate_pcm(void *v,int c) {
  int16_t *dst=v;
  int i=c;
  int activec=synth_ttl;
  if (activec>c) activec=c;
  synth_ttl-=activec;
  while (activec>0) {
    synth_phase++;
    if (synth_phase>=synth_halfwavelength) {
      synth_phase=0;
      synth_level=-synth_level;
    }
    *dst=synth_level;
    dst++;
    i--;
    activec--;
  }
  memset(dst,0,i<<1);
}

/* Begin a sound effect.
 */
 
void mn_cb_sound_effect(int sfxid,void *userdata) {
  //fprintf(stderr,"%s %d\n",__func__,sfxid);
  if (eh_audio_lock()<0) return;
  switch (sfxid) {
    case GUI_SFXID_ACTIVATE: synth_halfwavelength= 50; break;
    case GUI_SFXID_MINOR_OK: synth_halfwavelength= 75; break;
    case GUI_SFXID_MOTION:   synth_halfwavelength=100; break;
    case GUI_SFXID_CANCEL:   synth_halfwavelength=150; break;
    case GUI_SFXID_REJECT:   synth_halfwavelength=200; break;
  }
  synth_ttl=8000;
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
  fprintf(stderr,"%s:%d\n",__FILE__,__LINE__);//2023-11-23T16:03 leave this in place for the next startup segfault, it's happening randomly and rarely
  
  //mn.show_invalid=1;
  if (mn_choose_data_path()<0) return -1;
  
  dbs_init(&mn.dbs);
  fprintf(stderr,"%s:%d\n",__FILE__,__LINE__);
  dbs_refresh_search(&mn.dbs);
  fprintf(stderr,"%s:%d\n",__FILE__,__LINE__);
  dbs_refresh_all_metadata(&mn.dbs);
  fprintf(stderr,"%s:%d\n",__FILE__,__LINE__);

  struct gui_delegate delegate={
    .userdata=0,
    .cb_sound_effect=mn_cb_sound_effect,
  };
  if (!(mn.gui=gui_new(&delegate,mn.data_path,mn.data_pathc))) return -1;
  fprintf(stderr,"%s:%d\n",__FILE__,__LINE__);
  struct gui_widget *home=gui_replace_page(mn.gui,&mn_widget_type_home);
  fprintf(stderr,"%s:%d\n",__FILE__,__LINE__);
  if (!home) return -1;

  fprintf(stderr,"%s:%d\n",__FILE__,__LINE__);
  return 0;
}

/* Receive HTTP response over the fake WebSocket.
 */
 
static void mn_http_response(const char *src,int srcc) {
  dbs_http_response(&mn.dbs,src,srcc);
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
    .configure=0, // no need
    .load_file=0, // no need
    .load_serial=0, // no need
    .load_none=mn_load_none,
    .update=mn_update,
    .generate_pcm=mn_generate_pcm,
    .reset=0, // no need
    .use_menu_role=1,
    .http_response=mn_http_response,
  };
  mn.exename=(argc>=0)?argv[0]:0;
  if (!mn.exename||!mn.exename[0]) mn.exename=delegate.name;
  return eh_main(argc,argv,&delegate);
}
