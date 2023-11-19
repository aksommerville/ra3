#include "mn_internal.h"

struct mn mn={0};

/* Update.
 */
 
static int mn_update() {

  gui_update(mn.gui,eh_input_get(0));
          
  int winw=0,winh=0;
  eh_video_get_size(&winw,&winh);
  eh_video_begin();
  gui_render(mn.gui,winw,winh);
  eh_video_end();

  return 0;
}

/* Generate PCM.
 */
 
static void mn_generate_pcm(void *v,int c) {
  int rate=eh_audio_get_rate();
  int chanc=eh_audio_get_chanc();
  memset(v,0,c<<1);//TODO
}

/* Initialize.
 */
 
static int mn_load_none() {

  struct gui_delegate delegate={
    .userdata=0,
    .cb_sound_effect=mn_cb_sound_effect,
  };
  if (!(mn.gui=gui_new(&delegate))) return -1;
  struct gui_widget *home=gui_replace_page(mn.gui,&mn_widget_type_home);
  if (!home) return -1;
  
  //TODO fetch persisted dbs state.
  dbs_refresh_search(&mn.dbs);

  return 0;
}

/* Receive HTTP response over the fake WebSocket.
 */
 
static void mn_http_response(const char *src,int srcc) {
  dbs_http_response(&mn.dbs,src,srcc);
}

/* Begin a sound effect.
 */
 
void mn_cb_sound_effect(int sfxid,void *userdata) {
  //fprintf(stderr,"%s %d\n",__func__,sfxid);
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
  return eh_main(argc,argv,&delegate);
}
