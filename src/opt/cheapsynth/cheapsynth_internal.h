#ifndef CHEAPSYNTH_INTERNAL_H
#define CHEAPSYNTH_INTERNAL_H

#include "cheapsynth.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#define CHEAPSYNTH_VOICE_LIMIT 16 /* arbitrary */
#define CHEAPSYNTH_QBUF_SIZE 512
#define CHEAPSYNTH_DURATION_SANITY_LIMIT 3.0f
#define CHEAPSYNTH_DURATION_SAMPLES_LIMIT 1000000 /* Hard stop at INT_MAX/sizeof(int), but realistically much lower. */

struct cheapsynth {
  int rate;
  int chanc;
  
  /* (voicev) can be sparse. Always check (voice->sound) before using.
   */
  struct cheapsynth_voice {
    struct cheapsynth_sound *sound;
    int p;
  } voicev[CHEAPSYNTH_VOICE_LIMIT];
  int voicec;
  
  struct cheapsynth_sound **soundv;
  int soundc,sounda;
  
  float qbuf[CHEAPSYNTH_QBUF_SIZE];
};

struct cheapsynth_sound {
  int id;
  int c;
  float v[];
};

struct cheapsynth_sound *cheapsynth_sound_print(
  struct cheapsynth *cs,
  const struct cheapsynth_sound_config *config
);

#endif
