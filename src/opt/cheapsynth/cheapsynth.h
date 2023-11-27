/* cheapsynth.h
 * "Cheap" as in easy to write. Not necessarily high-performance.
 * The only thing we can play are non-sustaining PCM dumps.
 * We generate those dumps synchronously when you request them.
 * Sound effects are a single voice of FM.
 * Only mono output is supported, and you can ask for float or int16_t.
 */
 
#ifndef CHEAPSYNTH_H
#define CHEAPSYNTH_H

#include <stdint.h>

struct cheapsynth;
struct cheapsynth_sound;

struct cheapsynth_envelope_point {
  int delay_ms; // How many milliseconds before reaching this point?
  float v;
};

#define CHEAPSYNTH_LEVEL_LIMIT 3
#define CHEAPSYNTH_PITCH_LIMIT 5
#define CHEAPSYNTH_MOD_LIMIT 5

struct cheapsynth_sound_config {
  int id; // Must be unique for every sound you give us.
  float master; // 0..1
  float modrate; // Hz if (modabsolute), otherwise multiplier against pitch.
  int modabsolute;
  struct cheapsynth_envelope_point level[CHEAPSYNTH_LEVEL_LIMIT]; // (v) in (0..1). Implicit leading zero. Last value must be zero.
  struct cheapsynth_envelope_point pitch[CHEAPSYNTH_PITCH_LIMIT]; // (v) in Hz. First delay must be zero.
  struct cheapsynth_envelope_point mod[CHEAPSYNTH_MOD_LIMIT]; // (v) is modulation range, zero to five-ish.
};

/* Create or destroy the synthesizer.
 * It's ready to use immediately after new.
 */
void cheapsynth_del(struct cheapsynth *cs);
struct cheapsynth *cheapsynth_new(int rate,int chanc);

/* Print a sound effect or pull it from our cache.
 * Returns a WEAK reference that you can deliver to cheapsynth_sound_play().
 * Once created, sound effects exist and are immutable for as long as the synthesizer lives.
 * This is separate from cheapsynth_sound_play() because you don't need to lock for this.
 * (in fact, it is wise not to lock, since printing PCM might be expensive).
 * Looking up a sound effect that already exists is cheap, it's no problem to intern every time you play the effect.
 */
struct cheapsynth_sound *cheapsynth_sound_intern(struct cheapsynth *cs,const struct cheapsynth_sound_config *config);

/* Begin playing an effect that you previously interned against this synthesizer.
 * Don't share sounds across synthesizers.
 * You must lock your driver before calling this.
 */
void cheapsynth_sound_play(struct cheapsynth *cs,struct cheapsynth_sound *sound);

/* Generate output.
 */
void cheapsynth_updatef(float *v,int c,struct cheapsynth *cs);
void cheapsynth_updatei(int16_t *v,int c,struct cheapsynth *cs);

#endif
