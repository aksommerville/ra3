#ifndef EH_CLOCK_H
#define EH_CLOCK_H

#include <stdint.h>

struct eh_clock {
  double rate; // hz
  int64_t frame_time; // us
  int64_t start_time_real;
  int64_t start_time_cpu;
  int64_t recent_time_real;
  int framec;
  int faultc;
  char report_storage[128];
};

int64_t eh_now_real_us();
int64_t eh_now_cpu_us();

/* (rate) should come straight off the delegate.
 */
void eh_clock_init(struct eh_clock *clock,double rate);

/* Delay if needed, and return count of frames to execute, usually 1.
 */
int eh_clock_tick(struct eh_clock *clock);

/* Generate a report at shutdown to detail performance.
 * Never fails to return a NUL-terminated string, may be empty.
 * No need to free. Prior reports become invalid at the next call (but why would you call it more than once?).
 */
const char *eh_clock_report(struct eh_clock *clock);

#endif
