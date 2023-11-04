/* eh_aucvt.h
 */
 
#ifndef EH_AUCVT_H
#define EH_AUCVT_H

struct eh_auframe {
  float l,r;
};

typedef struct eh_auframe (*eh_auframe_read_fn)(const void *src);
typedef void (*eh_auframe_write_fn)(void *dst,struct eh_auframe frame);

struct eh_aucvt {
  int ready;
  int dstrate,dstchanc,dstfmt;
  int srcrate,srcchanc,srcfmt;
  int dstsamplesize,dstframesize;
  int srcsamplesize,srcframesize;
  int verbatim;
  void *buf;
  int bufp,bufc,bufa; // src samples; circular
  double fbufp; // true bufp if we are resampling
  double fbufpd;
  eh_auframe_read_fn rdframe;
  eh_auframe_write_fn wrframe;
  int badframec;
  int (*output)(void *dst,int samplec,struct eh_aucvt *aucvt); // if not generic
};

void eh_aucvt_cleanup(struct eh_aucvt *aucvt);

int eh_aucvt_init(
  struct eh_aucvt *aucvt,
  int dstrate,int dstchanc,int dstfmt,
  int srcrate,int srcchanc,int srcfmt
);

void eh_aucvt_warn_if_converting(const struct eh_aucvt *aucvt);

/* Add some PCM to our buffer, as provided by the emulator.
 * Returns the sample count consumed.
 * If <samplec, you should pump the output and wait, then send the rest.
 */
int eh_aucvt_input(struct eh_aucvt *aucvt,const void *src,int samplec);

/* Deliver output to the driver.
 * This always produces the requested sample count, whether we have sufficient input or not.
 * TODO That's asymmetric: We create time at output but not at input. Is that going to cause skew over time?
 */
int eh_aucvt_output(void *dst,int samplec,struct eh_aucvt *aucvt);

#endif
