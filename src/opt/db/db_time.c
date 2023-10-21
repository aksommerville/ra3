#include "db_internal.h"
#include "opt/serial/serial.h"
#include <time.h>

/* Current time, in db format.
 */
 
uint32_t db_time_now() {
  time_t t=time(0);
  struct tm tm={0};
  if (localtime_r(&t,&tm)!=&tm) return 0;
  return db_time_pack(1900+tm.tm_year,1+tm.tm_mon,tm.tm_mday,tm.tm_hour,tm.tm_min);
}

/* From string.
 */

uint32_t db_time_eval(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int vv[5]={0};
  int vp=0;
  int srcp=0;
  for (;srcp<srcc;srcp++) {
    if ((src[srcp]>='0')&&(src[srcp]<='9')) {
      vv[vp]*=10;
      vv[vp]+=src[srcp]-'0';
    } else {
      vp++;
      if (vp>=5) break;
    }
  }
  return db_time_pack(vv[0],vv[1],vv[2],vv[3],vv[4]);
}

uint32_t db_time_eval_upper(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int vv[5]={0};
  int vp=0;
  int srcp=0;
  for (;srcp<srcc;srcp++) {
    if ((src[srcp]>='0')&&(src[srcp]<='9')) {
      vv[vp]*=10;
      vv[vp]+=src[srcp]-'0';
    } else {
      vp++;
      if (vp>=5) break;
    }
  }
  int defs[5]={4095,15,31,31,63};
  memcpy(defs,vv,sizeof(int)*vp);
  return db_time_pack(defs[0],defs[1],defs[2],defs[3],defs[4]);
}

/* To string.
 */
 
int db_time_repr(char *dst,int dsta,uint32_t dbtime) {
  int vv[5]={0};
  db_time_unpack(vv+0,vv+1,vv+2,vv+3,vv+4,dbtime);
  int vc=5;
  if (vv[3]&&!vv[4]) { // "00:00" is a perfectly valid time, but it's also a perfectly common default.
    vc=3;
    if (!vv[2]) { // day of month shouldn't be zero
      vc=2;
      if (!vv[1]) vc=1; // month shouldn't be zero
    }
  }
  // Single field is a year or zero.
  if (vc==1) return sr_decsint_repr(dst,dsta,vv[0]);
  // When more than one field is present, enforce lengths: 4 digits for year, 2 for others.
  int dstc=sr_decuint_repr(dst+dstc,dsta-dstc,vv[0],4);
  if (vc>=2) {
    if (dstc<dsta) dst[dstc]='-'; dstc++;
    dstc+=sr_decuint_repr(dst+dstc,dsta-dstc,vv[1],2);
    if (vc>=3) {
      if (dstc<dsta) dst[dstc]='-'; dstc++;
      dstc+=sr_decuint_repr(dst+dstc,dsta-dstc,vv[2],2);
      if (vc>=4) { // always emit minutes if we emit hours, never hours alone.
        if (dstc<dsta) dst[dstc]='T'; dstc++;
        dstc+=sr_decuint_repr(dst+dstc,dsta-dstc,vv[3],2);
        if (dstc<dsta) dst[dstc]=':'; dstc++;
        dstc+=sr_decuint_repr(dst+dstc,dsta-dstc,vv[4],2);
      }
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Pack timestamp.
 */
 
uint32_t db_time_pack(int year,int month,int day,int hour,int minute) {
  return (
    ((year&0xfff)<<20)|
    ((month&0xf)<<16)|
    ((day&0x1f)<<11)|
    ((hour&0x1f)<<6)|
    (minute&0x3f)
  );
}

int db_time_unpack(int *year,int *month,int *day,int *hour,int *minute,uint32_t dbtime) {
  if (year) *year=(dbtime>>20)&0xfff;
  if (month) *month=(dbtime>>16)&0x0f;
  if (day) *day=(dbtime>>11)&0x1f;
  if (hour) *hour=(dbtime>>6)&0x1f;
  if (minute) *minute=dbtime&0x3f;
  return 0;
}

/* Next valid time.
 */
 
static int db_is_leap_year(int year) {
  if (year%4) return 0;
  if (year%100) return 1;
  if (year%400) return 0;
  return 1;
}
 
static int db_days_in_month(int year,int month) {
  switch (month) {
    case 1: return 31;
    case 2: return 28+db_is_leap_year(year);
    case 3: return 31;
    case 4: return 30;
    case 5: return 31;
    case 6: return 30;
    case 7: return 31;
    case 8: return 31;
    case 9: return 30;
    case 10: return 31;
    case 11: return 30;
    case 12: return 31;
  }
  return 30;
}
 
uint32_t db_time_advance(uint32_t from) {
  int year,month,day,hour,minute;
  db_time_unpack(&year,&month,&day,&hour,&minute,from);
  if (!year||!month||!day) return from+1; // partial, invalid, something... just increment.
  if (++minute>=60) {
    minute=0;
    if (++hour>=24) {
      hour=0;
      if (++day>db_days_in_month(year,month)) {
        day=1;
        if (++month>12) {
          month=1;
          year++;
          if (year>=0x1000) return 0;
        }
      }
    }
  }
  return db_time_pack(year,month,day,hour,minute);
}

/* Time difference in minutes.
 */
 
uint32_t db_time_diff_m(uint32_t older,uint32_t newer) {
  if (older>=newer) return 0;
  int ov[5]={0},nv[5]={0};
  db_time_unpack(ov+0,ov+1,ov+2,ov+3,ov+4,older);
  db_time_unpack(nv+0,nv+1,nv+2,nv+3,nv+4,newer);
  int32_t diff=nv[4]-ov[4];
  diff+=(nv[3]-ov[3])*60;
  diff+=(nv[2]-ov[2])*1440;
  diff+=(nv[1]-ov[1])*43200; // all months have 30 days
  diff+=(nv[0]-ov[0])*525600; // all years have 365 days
  // This is expected to be used in scenarios where you want a difference in minutes...
  // We don't expect (older,newer) to cross multiple days, and produce slightly incorrect results when it does.
  return diff;
}
