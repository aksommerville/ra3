#include "cheapsynth_internal.h"

/* Delete.
 */
 
void cheapsynth_del(struct cheapsynth *cs) {
  if (!cs) return;
  if (cs->soundv) {
    while (cs->soundc-->0) free(cs->soundv[cs->soundc]);
    free(cs->soundv);
  }
  free(cs);
}

/* New.
 */
 
struct cheapsynth *cheapsynth_new(int rate,int chanc) {
  if ((rate<200)||(rate>200000)) return 0;
  if ((chanc<1)||(chanc>8)) return 0;
  fprintf(stderr,"%s rate=%d chanc=%d\n",__func__,rate,chanc);
  struct cheapsynth *cs=calloc(1,sizeof(struct cheapsynth));
  if (!cs) return 0;
  cs->rate=rate;
  cs->chanc=chanc;
  return cs;
}

/* Search sound list.
 */
 
static int cheapsynth_sound_search(struct cheapsynth *cs,int id) {
  int lo=0,hi=cs->soundc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=cs->soundv[ck]->id;
         if (id<q) hi=ck;
    else if (id>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Intern sound.
 */

struct cheapsynth_sound *cheapsynth_sound_intern(struct cheapsynth *cs,const struct cheapsynth_sound_config *config) {
  if (!cs||!config) return 0;
  int p=cheapsynth_sound_search(cs,config->id);
  if (p>=0) return cs->soundv[p];
  p=-p-1;
  if (cs->soundc>=cs->sounda) {
    int na=cs->sounda+16;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(cs->soundv,sizeof(void*)*na);
    if (!nv) return 0;
    cs->soundv=nv;
    cs->sounda=na;
  }
  struct cheapsynth_sound *sound=cheapsynth_sound_print(cs,config);
  if (!sound) return 0;
  memmove(cs->soundv+p+1,cs->soundv+p,sizeof(void*)*(cs->soundc-p));
  cs->soundc++;
  cs->soundv[p]=sound;
  return sound;
}

/* Begin sound.
 */

void cheapsynth_sound_play(struct cheapsynth *cs,struct cheapsynth_sound *sound) {
  if (!cs||!sound) return;
  
  /* Find an unused voice, or overwrite the oldest.
   */
  struct cheapsynth_voice *voice=0;
  if (cs->voicec<CHEAPSYNTH_VOICE_LIMIT) {
    voice=cs->voicev+cs->voicec++;
  } else {
    voice=cs->voicev;
    struct cheapsynth_voice *q=cs->voicev;
    int i=CHEAPSYNTH_VOICE_LIMIT;
    for (;i-->0;q++) {
      if (!q->sound) { // voice not actually in use -- perfect
        voice=q;
        break;
      }
      if (q->p>voice->p) { // older, keep it for now
        voice=q;
      }
    }
  }
  
  voice->sound=sound;
  voice->p=0;
}

/* Add signals.
 */
 
static inline void cheapsynth_fv_add(float *dst,const float *src,int c) {
  for (;c-->0;dst++,src++) (*dst)+=(*src);
}

/* Generate floating-point mono signal.
 */

static void cheapsynth_updatef_mono(float *v,int c,struct cheapsynth *cs) {
  if (c<1) return;
  memset(v,0,sizeof(float)*c);
  
  int defunct=0;
  struct cheapsynth_voice *voice=cs->voicev;
  int i=cs->voicec;
  for (;i-->0;voice++) {
    if (!voice->sound) {
      defunct=1;
      continue;
    }
    int cpc=voice->sound->c-voice->p;
    if (cpc>c) cpc=c;
    if (cpc<1) {
      voice->sound=0;
      defunct=1;
      continue;
    }
    cheapsynth_fv_add(v,voice->sound->v+voice->p,cpc);
    voice->p+=cpc;
  }
  
  // If we detected or produced any gaps in (voicev), compact it.
  if (defunct) {
    for (i=cs->voicec;i-->0;) {
      if (cs->voicev[i].sound) continue;
      cs->voicec--;
      memmove(cs->voicev+i,cs->voicev+i+1,sizeof(struct cheapsynth_voice)*(cs->voicec-i));
    }
  }
}

/* Generate floating-point signal, any channel count.
 */
 
static void cheapsynth_expand_stereo(float *v,int framec) {
  float *dst=v+(framec<<1);
  const float *src=v+framec;
  while (framec-->0) {
    src--;
    *(--dst)=*src;
    *(--dst)=*src;
  }
}

static void cheapsynth_expand_multi(float *v,int framec,int chanc) {
  float *dst=v+framec*chanc;
  const float *src=v+framec;
  while (framec-->0) {
    src--;
    int i=chanc;
    while (i-->0) *(--dst)=*src;
  }
}
 
void cheapsynth_updatef(float *v,int c,struct cheapsynth *cs) {
  if (cs->chanc==1) {
    cheapsynth_updatef_mono(v,c,cs);
  } else if (cs->chanc==2) {
    int framec=c>>1;
    cheapsynth_updatef_mono(v,framec,cs);
    cheapsynth_expand_stereo(v,framec);
  } else {
    int framec=c/cs->chanc;
    cheapsynth_updatef_mono(v,framec,cs);
    cheapsynth_expand_multi(v,framec,cs->chanc);
  }
}

/* Generate and quantize signal.
 */
 
static inline void cheapsynth_quantize(int16_t *dst,const float *src,int c) {
  for (;c-->0;dst++,src++) *dst=(*src)*32767.0f;
}
 
void cheapsynth_updatei(int16_t *v,int c,struct cheapsynth *cs) {
  while (c>=CHEAPSYNTH_QBUF_SIZE) {
    cheapsynth_updatef(cs->qbuf,CHEAPSYNTH_QBUF_SIZE,cs);
    cheapsynth_quantize(v,cs->qbuf,CHEAPSYNTH_QBUF_SIZE);
    v+=CHEAPSYNTH_QBUF_SIZE;
    c-=CHEAPSYNTH_QBUF_SIZE;
  }
  if (c>0) {
    cheapsynth_updatef(cs->qbuf,c,cs);
    cheapsynth_quantize(v,cs->qbuf,c);
  }
}
