#include "eh_internal.h"

/* Quit.
 */
 
void eh_drivers_quit() {
  //TODO
}

/* Init.
 */
 
int eh_drivers_init() {
  //TODO
  return 0;
}

/* Accessors to audio.
 */
 
int eh_audio_get_rate() {
  return 0;//TODO
}

int eh_audio_get_chanc() {
  return 0;//TODO
}

int eh_audio_get_format() {
  return 0;//TODO
}

void eh_audio_write(const void *v,int framec) {
  //TODO
}

int eh_audio_guess_framec() {
  return 123;//TODO
}

int eh_audio_lock() {
  return 0;//TODO
}

void eh_audio_unlock() {
  //TODO
}

/* Accessors to video.
 */
 
void eh_video_write(const void *fb) {
  //TODO
}

void eh_video_begin() {
  //TODO
}

void eh_video_end() {
  //TODO
}

void eh_video_get_size(int *w,int *h) {
  *w=640;//TODO
  *h=480;
}

void eh_ctab_write(int p,int c,const void *rgb) {
  //TODO
}

void eh_ctab_read(void *rgb,int p,int c) {
  //TODO
}

/* Accessors to input.
 */
 
uint16_t eh_input_get(uint8_t plrid) {
  return 0;//TODO
}
