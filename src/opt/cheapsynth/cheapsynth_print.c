#include "cheapsynth_internal.h"

/* Printing context.
 */
 
struct cheapsynth_printer {
  struct cheapsynth *cs;
  const struct cheapsynth_sound_config *config;
  struct cheapsynth_sound *sound;
  int levelc,pitchc,modc; // Length of each config envelope.
  float timescale; // frames/ms
  float pitchscale; // radians/frame
};

static void cheapsynth_printer_cleanup(struct cheapsynth_printer *printer) {
  if (printer->sound) free(printer->sound);
}

/* Validate, measure, and allocate (sound).
 */
 
static int cheapsynth_printer_setup(struct cheapsynth_printer *printer) {
  if (!printer->cs) return -1;
  if (!printer->config) return -1;
  
  printer->timescale=(float)printer->cs->rate/1000.0f;
  printer->pitchscale=(M_PI*2.0f)/(float)printer->cs->rate;
  
  printer->levelc=CHEAPSYNTH_LEVEL_LIMIT;
  while (printer->levelc&&!printer->config->level[printer->levelc-1].delay_ms) printer->levelc--;
  if (!printer->levelc) return -1;
  if (!printer->config->level[0].delay_ms) return -1; // First level delay must be nonzero. There is an implicit (0,0) at the start.
  if (printer->config->level[printer->levelc-1].v>0.0f) return -1; // Last level must be zero.
  
  printer->pitchc=CHEAPSYNTH_PITCH_LIMIT;
  while ((printer->pitchc>1)&&!printer->config->pitch[printer->pitchc-1].delay_ms) printer->pitchc--;
  if (printer->config->pitch[0].delay_ms) return -1;  // First pitch must be at time zero.
  
  printer->modc=CHEAPSYNTH_MOD_LIMIT;
  while ((printer->modc>1)&&!printer->config->mod[printer->modc-1].delay_ms) printer->modc--;
  if (printer->config->mod[0].delay_ms) return -1; // First modulation must be at time zero.
  
  // Calculate total duration.
  float durs=0.0f;
  const struct cheapsynth_envelope_point *point=printer->config->level;
  int i=printer->levelc;
  for (;i-->0;point++) {
    durs+=(float)point->delay_ms/1000.0f;
  }
  if (durs>CHEAPSYNTH_DURATION_SANITY_LIMIT) return -1;
  int samplec=(int)(durs*printer->cs->rate);
  if (samplec<1) return -1;
  if (samplec>CHEAPSYNTH_DURATION_SAMPLES_LIMIT) return -1;
  
  if (!(printer->sound=malloc(sizeof(struct cheapsynth_sound)+sizeof(float)*samplec))) return -1;
  printer->sound->id=printer->config->id;
  printer->sound->c=samplec;
  
  return 0;
}

/* Envelope runner.
 * All legs progress linearly.
 */
 
struct cheapsynth_envelope_runner {
  const struct cheapsynth_envelope_point *pointv;
  int pointc;
  int pointp;
  int remaining;
  int finished;
  float v;
  float dv;
  float timescale;
};

static void cheapsynth_envelope_runner_init(
  struct cheapsynth_envelope_runner *runner,
  const struct cheapsynth_envelope_point *pointv,
  int pointc,
  float timescale // frames per millisecond
) {
  runner->pointv=pointv;
  runner->pointc=pointc;
  runner->timescale=timescale;
  runner->pointp=0;
  if ((runner->pointc>0)&&!runner->pointv[0].delay_ms) {
    runner->v=runner->pointv[0].v;
    runner->pointp++;
  } else {
    runner->v=0.0f;
  }
  if (runner->pointp>=runner->pointc) {
    runner->dv=0.0f;
    runner->finished=1;
    runner->remaining=0;
  } else {
    runner->finished=0;
    if ((runner->remaining=runner->pointv[runner->pointp].delay_ms*runner->timescale)<1) runner->remaining=1;
    runner->dv=(runner->pointv[runner->pointp].v-runner->v)/runner->remaining;
  }
}

static float cheapsynth_envelope_runner_update(struct cheapsynth_envelope_runner *runner) {
  if (!runner->finished) {
    if (runner->remaining) {
      runner->remaining--;
      runner->v+=runner->dv;
    } else {
      runner->pointp++;
      if (runner->pointp>=runner->pointc) {
        runner->finished=1;
      } else {
        if ((runner->remaining=runner->pointv[runner->pointp].delay_ms*runner->timescale)<1) runner->remaining=1;
        runner->dv=(runner->pointv[runner->pointp].v-runner->v)/runner->remaining;
      }
    }
  }
  return runner->v;
}

/* With the context validated and sound allocated, generate the PCM.
 */
 
static void cheapsynth_printer_run(struct cheapsynth_printer *printer) {

  float carp=0.0f; // Carrier phase, -pi..pi
  float modp=0.0f; // Modulator phase, -pi..pi
  struct cheapsynth_envelope_runner levelenv,pitchenv,modenv;
  cheapsynth_envelope_runner_init(&levelenv,printer->config->level,printer->levelc,printer->timescale);
  cheapsynth_envelope_runner_init(&pitchenv,printer->config->pitch,printer->pitchc,printer->timescale);
  cheapsynth_envelope_runner_init(&modenv,printer->config->mod,printer->modc,printer->timescale);

  float *dst=printer->sound->v;
  int dsti=printer->sound->c;
  
  if (printer->config->modabsolute) { // absolute modulator
    float modrate=printer->config->modrate*printer->pitchscale;
    for (;dsti-->0;dst++) {
      *dst=sinf(carp)*cheapsynth_envelope_runner_update(&levelenv);
      float cardp=cheapsynth_envelope_runner_update(&pitchenv)*printer->pitchscale;
      float mod=sinf(modp);
      modp+=modrate;
      if (modp>M_PI) modp-=M_PI*2.0f;
      float modrange=cheapsynth_envelope_runner_update(&modenv);
      cardp+=cardp*mod*modrange;
      carp+=cardp;
      if (carp>M_PI) carp-=M_PI*2.0f;
    }
    
  } else { // relative modulator
    for (;dsti-->0;dst++) {
      *dst=sinf(carp)*cheapsynth_envelope_runner_update(&levelenv);
      float cardp=cheapsynth_envelope_runner_update(&pitchenv)*printer->pitchscale;
      float mod=sinf(modp);
      modp+=printer->config->modrate*cardp;
      if (modp>M_PI) modp-=M_PI*2.0f;
      float modrange=cheapsynth_envelope_runner_update(&modenv);
      cardp+=cardp*mod*modrange;
      carp+=cardp;
      if (carp>M_PI) carp-=M_PI*2.0f;
    }
  }
}

/* Apply master level.
 */
 
static void cheapsynth_printer_normalize(struct cheapsynth_printer *printer) {
  float adj=printer->config->master;
  float *v=printer->sound->v;
  int i=printer->sound->c;
  for (;i-->0;v++) (*v)*=adj;
}

/* Print sound, main entry point.
 */
 
struct cheapsynth_sound *cheapsynth_sound_print(
  struct cheapsynth *cs,
  const struct cheapsynth_sound_config *config
) {
  struct cheapsynth_printer printer={
    .cs=cs,
    .config=config,
  };
  if (cheapsynth_printer_setup(&printer)<0) { cheapsynth_printer_cleanup(&printer); return 0; }
  cheapsynth_printer_run(&printer);
  cheapsynth_printer_normalize(&printer);
  struct cheapsynth_sound *sound=printer.sound;
  printer.sound=0;
  cheapsynth_printer_cleanup(&printer);
  return sound;
}
