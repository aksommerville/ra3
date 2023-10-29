#include "eh_clock.h"
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>

/* If our expected time disagrees with real time by more than this, panic and reset.
 */
#define EH_MAX_DELAY 1000000ll

/* Current time.
 */
 
int64_t eh_now_real_us() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (int64_t)tv.tv_sec*1000000ll+tv.tv_usec;
}

int64_t eh_now_cpu_us() {
  struct timespec tv={0};
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&tv);
  return (int64_t)tv.tv_sec*1000000ll+tv.tv_nsec/1000;
}

/* Init.
 */
 
void eh_clock_init(struct eh_clock *clock,double rate) {

  /* Permit 1..240 Hz, and default to 60 if unset or negative.
   */
  if (rate<1.0) clock->rate=60.0;
  else if (rate>240.0) clock->rate=240.0;
  else clock->rate=rate;
  clock->frame_time=1000000.0/clock->rate;
  
  clock->start_time_real=eh_now_real_us();
  clock->start_time_cpu=eh_now_cpu_us();
  // Cheat start time backward so we don't delay the first frame:
  clock->recent_time_real=clock->start_time_real-clock->frame_time;
  clock->framec=0;
  clock->faultc=0;
}

/* Tick.
 */

int eh_clock_tick(struct eh_clock *clock) {
  clock->recent_time_real+=clock->frame_time;
  int64_t now=eh_now_real_us();
  while (1) {
    int64_t delay=clock->recent_time_real-now;
    if (delay<-EH_MAX_DELAY) {
      clock->recent_time_real=now;
      clock->faultc++;
      break;
    }
    if (delay<=0) break;
    if (delay>EH_MAX_DELAY) {
      clock->recent_time_real=now;
      clock->faultc++;
      break;
    }
    usleep(delay+1);
    now=eh_now_real_us();
  }
  clock->framec++;
  return 1;
}

/* Report.
 */

const char *eh_clock_report(struct eh_clock *clock) {
  if (clock->framec<1) return "";
  
  double elapsed_real=(clock->recent_time_real-clock->start_time_real)/1000000.0;
  double elapsed_cpu=(eh_now_cpu_us()-clock->start_time_cpu)/1000000.0;
  double avgrate=clock->framec/elapsed_real;
  double cpuusage=elapsed_cpu/elapsed_real;
  
  int c=snprintf(clock->report_storage,sizeof(clock->report_storage),
    "%d frames in %.0f s, %d faults, video rate %.03f Hz, CPU usage %.03f",
    clock->framec,elapsed_real,clock->faultc,avgrate,cpuusage
  );
  if ((c<1)||(c>=sizeof(clock->report_storage))) {
    clock->report_storage[0]=0;
  }
  return clock->report_storage;
}
