#include "ra_internal.h"
#include <time.h>

/* Local time for logging.
 */
 
static char ra_local_time_buffer[15];

const char *ra_local_time() {
  time_t now=time(0);
  struct tm tm={0};
  localtime_r(&now,&tm);
  int year=1900+tm.tm_year; if (year<0) year=0;
  int month=1+tm.tm_mon; if (month<0) month=0; else if (month>99) month=99;
  int day=tm.tm_mday; if (day<0) day=0; else if (day>99) day=99;
  int hour=tm.tm_hour; if (hour<0) hour=0; else if (hour>99) hour=99;
  int minute=tm.tm_min; if (minute<0) minute=0; else if (minute>99) minute=99;
  int second=tm.tm_sec; if (second<0) second=0; else if (second>99) second=99;
  ra_local_time_buffer[0]='0'+(year/1000)%10;
  ra_local_time_buffer[1]='0'+(year/100)%10;
  ra_local_time_buffer[2]='0'+(year/10)%10;
  ra_local_time_buffer[3]='0'+year%10;
  ra_local_time_buffer[4]='0'+month/10;
  ra_local_time_buffer[5]='0'+month%10;
  ra_local_time_buffer[6]='0'+day/10;
  ra_local_time_buffer[7]='0'+day%10;
  ra_local_time_buffer[8]='0'+hour/10;
  ra_local_time_buffer[9]='0'+hour%10;
  ra_local_time_buffer[10]='0'+minute/10;
  ra_local_time_buffer[11]='0'+minute%10;
  ra_local_time_buffer[12]='0'+second/10;
  ra_local_time_buffer[13]='0'+second%10;
  ra_local_time_buffer[14]=0;
  return ra_local_time_buffer;
}
