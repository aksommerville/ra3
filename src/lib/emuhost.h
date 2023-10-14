/* emuhost.h
 * Interface to platform I/O geared for emulators.
 * With special awareness of Romassist.
 */
 
#ifndef EMUHOST_H
#define EMUHOST_H

#include <stdint.h>

#define EH_VIDEO_FORMAT_I1 1
#define EH_VIDEO_FORMAT_I2 2
#define EH_VIDEO_FORMAT_I4 3
#define EH_VIDEO_FORMAT_I8 4
#define EH_VIDEO_FORMAT_RGB16 5
#define EH_VIDEO_FORMAT_RGB24 6
#define EH_VIDEO_FORMAT_RGB32 7
#define EH_VIDEO_FORMAT_RGB16SWAP 8

#define EH_AUDIO_FORMAT_S16N 1
#define EH_AUDIO_FORMAT_S16LE 2
#define EH_AUDIO_FORMAT_S16BE 3
#define EH_AUDIO_FORMAT_S32N 4
#define EH_AUDIO_FORMAT_S32LE 5
#define EH_AUDIO_FORMAT_S32BE 6
#define EH_AUDIO_FORMAT_F32N 7

#define EH_BTN_LEFT     0x0001
#define EH_BTN_RIGHT    0x0002
#define EH_BTN_UP       0x0004
#define EH_BTN_DOWN     0x0008
#define EH_BTN_SOUTH    0x0010
#define EH_BTN_WEST     0x0020
#define EH_BTN_EAST     0x0040
#define EH_BTN_NORTH    0x0080
#define EH_BTN_L1       0x0100
#define EH_BTN_R1       0x0200
#define EH_BTN_L2       0x0400
#define EH_BTN_R2       0x0800
#define EH_BTN_AUX1     0x1000
#define EH_BTN_AUX2     0x2000
#define EH_BTN_AUX3     0x4000
#define EH_BTN_CD       0x8000

/* Inversion of control.
 * Consumers must surrender control of the main loop entirely to emuhost.
 * Call eh_main() from your main(), and it triggers the delegate as warranted.
 ********************************************************************************/
 
struct eh_delegate {

  /* Fixed framebuffer size.
   * This may be (0,0) to request direct OpenGL access instead.
   * If you request an "RGB" format, you must set rmask, gmask, bmask.
   * For RGB16 and RGB32, masks are set as if pixels were read as native words.
   * For RGB24, phrase them big-endianly. eg PNG's 24-bit RGB would have (rmask=0xff0000,gmask=0xff00,bmask=0xff).
   * 16-bit RGB might have channels crossing the byte boundary.
   * If you generate in a specific byte order regardless of the host byte order, detect the host order and use RGB16SWAP if mismatched.
   * Pixels 8 bits and smaller always use a color table, are always packed big-endianly, and pad rows to 1 byte.
   */
  int video_width;
  int video_height;
  int video_format;
  uint32_t rmask,gmask,bmask;
  
  /* Desired update rate in Hertz.
   * Typically 60.0.
   * Zero is legal if you tolerate any timing.
   */
  double video_rate;
  
  /* Audio format.
   * If any conversion or resampling is required, Emuhost takes care of it.
   * We assume that it is unreasonable for the core to provide different choices.
   * Frames must always be interleaved (L0,R0,L1,R1,...).
   * Rate zero if you can tolerate any rate, we'll pick something agreeable to the hardware.
   */
  int audio_rate;
  int audio_chanc;
  int audio_format;
  
  /* How many controller sockets on the front of the device?
   * Emuhost will assign input devices accordingly.
   */
  int playerc;
  
  /* Called before (ready) with individual config fields.
   * These would come from argv, or maybe from a config file.
   * Emuhost will consume fields it understands and you never see those.
   */
  int (*configure)(const char *k,int kc,const char *v,int vc,int vn);
  
  /* Called once, after configuration is acquired and drivers are running.
   * You only need to implement one of these, typically load_serial.
   * load_file is provided in case the emulator's core is set up to read files on its own.
   * Emuhost will never ask you to re-load or to load another different game in the same run.
   */
  int (*load_file)(const char *path);
  int (*load_serial)(const void *v,int c,const char *name);
  
  /* Run the emulator, generate one frame of video and the equivalent amount of audio.
   */
  int (*update)();
  
  /* Optional. Generate (c) samples of PCM, always a multiple of (audio_chanc).
   * If you implement this, it may be called on a separate thread independent of update cycles.
   * This yields much better performance, less risk of overrun/underrun.
   * But most emulators can't do it because they need audio and video out to operate in sync.
   * ** If you implement this, you must never call eh_audio_write **
   */
  void (*generate_pcm)(void *v,int c);
  
  /* Hard restart, as if the machine's Reset button were pressed.
   * (do about what you do at load, if there isn't specific Reset handling in the core).
   */
  int (*reset)();
};

/* Call from your main, typically the only thing your main should do.
 * It's OK to preprocess (argv) and overwrite with empties or nulls, we'll skip them.
 * But it's better to wait for them to arrive via (delegate->configure), to ensure uniform processing.
 * Returns exit status: 0 or 1.
 */
int eh_main(int argc,char **argv,const struct eh_delegate *delegate);

/* Video.
 ****************************************************************************/

/* If you requested a nonzero framebuffer size, you should provide fresh content at each update.
 */
void eh_video_write(const void *fb);

/* If you requested (0,0) framebuffer, you should draw with OpenGL, between these fences, each update.
 */
void eh_video_begin();
void eh_video_end();

/* Size of the full output.
 * Generally not interesting if you are rendering with a framebuffer.
 */
void eh_video_get_size(int *w,int *h);

/* Replace (c) entries beginning with (p) in the global color table.
 * (rgb) points to [r0,g0,b0,r1,g1,b1,...].
 * We supply a 256-entry color table whether you need it or not.
 */
void eh_ctab_write(int p,int c,const void *rgb);

void eh_ctab_read(void *rgb,int p,int c);

/* Audio.
 *********************************************************************/

/* If you specified via delegate, you already know these.
 * But if you provided zeroes, emuhost will pick something appropriate to the hardware during init.
 * After initialization, these will never change.
 */
int eh_audio_get_rate();
int eh_audio_get_chanc();
int eh_audio_get_format();

/* Push out more PCM, in the format you declared via delegate.
 * You should do this each update, typically with the same frame count each time.
 * Do not call if you implement delegate.generate_pcm.
 */
void eh_audio_write(const void *v,int framec);

/* Let the driver determine how many frames are needed.
 * Most emulators don't work like this, but use it if possible, it avoids sync problems.
 */
int eh_audio_guess_framec();

/* Lock and unlock the audio driver.
 * Only necessary if you use delegate.generate_pcm. Ensures your callback is not running.
 */
int eh_audio_lock();
void eh_audio_unlock();

/* Input.
 ******************************************************************/

/* Poll one virtual input device.
 * (plrid) zero is the aggregate of all connected devices.
 */
uint16_t eh_input_get(uint8_t plrid);

#endif
