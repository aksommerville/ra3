#include "eh_inmgr_internal.h"
#include "opt/serial/serial.h"

/* Cleanup.
 */
 
void eh_inmgr_config_cleanup(struct eh_inmgr_config *config) {
  if (config->name) free(config->name);
  if (config->rulev) free(config->rulev);
}

/* Set name.
 */
 
int eh_inmgr_config_set_name(struct eh_inmgr_config *config,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { srcc--; src++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (config->name) free(config->name);
  config->name=nv;
  config->namec=srcc;
  return 0;
}

/* Search rules.
 */
 
static int eh_inmgr_config_rulev_search(const struct eh_inmgr_config *config,int srcbtnid) {
  int lo=0,hi=config->rulec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (srcbtnid<config->rulev[ck].srcbtnid) hi=ck;
    else if (srcbtnid>config->rulev[ck].srcbtnid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Add rule.
 */
 
struct eh_inmgr_rule *eh_inmgr_config_add_rule(struct eh_inmgr_config *config,int srcbtnid) {

  int p=eh_inmgr_config_rulev_search(config,srcbtnid);
  if (p>=0) return 0;
  p=-p-1;
  
  if (config->rulec>=config->rulea) {
    int na=config->rulea+16;
    if (na>INT_MAX/sizeof(struct eh_inmgr_rule)) return 0;
    void *nv=realloc(config->rulev,sizeof(struct eh_inmgr_rule)*na);
    if (!nv) return 0;
    config->rulev=nv;
    config->rulea=na;
  }
  
  struct eh_inmgr_rule *rule=config->rulev+p;
  memmove(rule+1,rule,sizeof(struct eh_inmgr_rule)*(config->rulec-p));
  config->rulec++;
  memset(rule,0,sizeof(struct eh_inmgr_rule));
  rule->srcbtnid=srcbtnid;
  
  return rule;
}

/* Get rule.
 */
 
struct eh_inmgr_rule *eh_inmgr_config_get_rule(const struct eh_inmgr_config *config,int srcbtnid) {
  int p=eh_inmgr_config_rulev_search(config,srcbtnid);
  if (p<0) return 0;
  return config->rulev+p;
}

/* Remove rule.
 */
 
void eh_inmgr_config_remove_rule(struct eh_inmgr_config *config,struct eh_inmgr_rule *rule) {
  if (!config||!rule) return;
  int p=rule-config->rulev;
  if ((p<0)||(p>=config->rulec)) return;
  config->rulec--;
  memmove(rule,rule+1,sizeof(struct eh_inmgr_rule)*(config->rulec-p));
}

/* Match config against device ids.
 * An empty config matches everything.
 */
 
static int eh_inmgr_config_match(
  const struct eh_inmgr_config *config,
  int vendor,int product,int version,
  const char *name,int namec
) {
  if (config->vendor&&(config->vendor!=vendor)) return 0;
  if (config->product&&(config->product!=product)) return 0;
  // We don't actually use (version).
  if (config->namec&&!sr_pattern_match(name,namec,config->name,config->namec)) return 0;
  return 1;
}

/* Search configs for best fit for device.
 */
 
struct eh_inmgr_config *eh_inmgr_config_for_device(struct eh_inmgr *inmgr,struct eh_inmgr_device *device) {
  if (!device->devid) return 0;

  uint16_t vendor=0,product=0,version=0;
  const char *name=0;
  int namec=0;
  if (device->driver) {
    name=device->driver->type->get_ids(&vendor,&product,device->driver,device->devid);
    if (name) {
      while (name[namec]) namec++;
    }
  } else {
    name="System Keyboard";
    namec=15;
  }

  struct eh_inmgr_config *config=inmgr->configv;
  int i=inmgr->configc;
  for (;i-->0;config++) {
    if (!eh_inmgr_config_match(config,vendor,product,version,name,namec)) continue;
    return config;
  }
  return 0;
}

/* Add rule during guess-config.
 * Four extra compound button symbols for guesswork only.
 * We insert rules with these temporary outputs.
 * The only real work we do is verify that the source range is valid for the output type.
 */
 
#define EH_BTN_THUMBS (EH_BTN_SOUTH|EH_BTN_WEST|EH_BTN_EAST|EH_BTN_NORTH)
#define EH_BTN_TRIGGERS (EH_BTN_L1|EH_BTN_R1|EH_BTN_L2|EH_BTN_R2)
#define EH_BTN_AXES (EH_BTN_UP|EH_BTN_LEFT)
#define EH_BTN_ANY 0x7fff

static int eh_inmgr_config_guess_rule(struct eh_inmgr_config *config,int srcbtnid,int lo,int hi,int dstbtnid) {
  switch (dstbtnid) {
    case EH_BTN_AXES:
    case EH_BTN_HORZ:
    case EH_BTN_VERT: {
        if (lo>hi-2) return 0;
      } break;
    case EH_BTN_DPAD: {
        if (lo!=hi-7) return 0;
      } break;
    default: {
        if (lo>hi-1) return 0;
      }
  }
  struct eh_inmgr_rule *rule=eh_inmgr_config_add_rule(config,srcbtnid);
  if (!rule) return 0; // maybe (srcbtnid) was reported twice? ignore it
  rule->dstbtnid=dstbtnid;
  return 0;
}

/* Guess config: Receive source button.
 */
 
static int eh_inmgr_config_cb_button(int btnid,uint32_t hidusage,int lo,int hi,int value,void *userdata) {
  struct eh_inmgr_config *config=userdata;
  
  //fprintf(stderr,"  %s 0x%08x %d..%d usage=0x%08x\n",__func__,btnid,lo,hi,hidusage);
  
  // Ignore anything on page 7 (Keyboard). There could be tons of these, and we're not interested.
  if ((hidusage&0xffff0000)==0x00070000) return 0;
  
  // Page 9 (Generic Button), assign anywhere.
  if ((hidusage&0xffff0000)==0x00090000) return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_ANY);
  
  // OK, pick off known HID usages.
  switch (hidusage) {
    case 0x00010030: return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_HORZ);
    case 0x00010031: return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_VERT);
    case 0x00010033: return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_HORZ);
    case 0x00010034: return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_VERT);
    case 0x00010039: return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_DPAD);
    case 0x0001003d: return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_AUX1);
    case 0x0001003e: return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_AUX2);
    case 0x00010090: return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_UP);
    case 0x00010091: return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_DOWN);
    case 0x00010092: return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_RIGHT);
    case 0x00010093: return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_LEFT);
    case 0x00050037: return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_THUMBS);
    case 0x00050039: return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_TRIGGERS);
  }
  
  // Can we make some assumptions based on the range?
  if (lo==-hi) return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_AXES);
  if (lo==hi-7) return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_DPAD);
  if (lo==hi-1) return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_ANY);
  if ((lo==0)&&(hi==2)) return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_ANY);
  if ((lo==0)&&(hi>2)) return eh_inmgr_config_guess_rule(config,btnid,lo,hi,EH_BTN_AXES);
  
  return 0;
}

/* Finalize guessed config.
 * If there are duplicates and gaps in our output, move assignments around to make it work.
 * Return nonzero if the config looks usable.
 */
 
static int eh_inmgr_config_finalize(struct eh_inmgr_config *config) {

  // Fewer than 5 rules can't make a valid config, don't bother analyzing.
  if (config->rulec<5) return 0;

  // The first 15 are ((1<<n)==EH_BUTTON_*).
  // The last 4 are pending guess-assignments.
  // Stateless rules don't get counted.
  const int EH_IX_LEFT=0;
  const int EH_IX_RIGHT=1;
  const int EH_IX_UP=2;
  const int EH_IX_DOWN=3;
  const int EH_IX_SOUTH=4;
  const int EH_IX_WEST=5;
  const int EH_IX_EAST=6;
  const int EH_IX_NORTH=7;
  const int EH_IX_L1=8;
  const int EH_IX_R1=9;
  const int EH_IX_L2=10;
  const int EH_IX_R2=11;
  const int EH_IX_AUX1=12;
  const int EH_IX_AUX2=13;
  const int EH_IX_AUX3=14;
  const int EH_IX_ANY=15;
  const int EH_IX_THUMBS=16;
  const int EH_IX_TRIGGERS=18;
  const int EH_IX_AXES=18;
  int count_by_output[19]={0};

  // Count up the initial state.
  struct eh_inmgr_rule *rule=config->rulev;
  int i=config->rulec;
  for (;i-->0;rule++) {
    switch (rule->dstbtnid) {
      #define _(tag) case EH_BTN_##tag: count_by_output[EH_IX_##tag]++; break;
      _(LEFT)
      _(RIGHT)
      _(UP)
      _(DOWN)
      _(SOUTH)
      _(WEST)
      _(EAST)
      _(NORTH)
      _(L1)
      _(R1)
      _(L2)
      _(R2)
      _(AUX1)
      _(AUX2)
      _(AUX3)
      _(THUMBS)
      _(TRIGGERS)
      _(AXES)
      _(ANY)
      #undef _
      case EH_BTN_HORZ: count_by_output[EH_IX_LEFT]++; count_by_output[EH_IX_RIGHT]++; break;
      case EH_BTN_VERT: count_by_output[EH_IX_UP]++; count_by_output[EH_IX_DOWN]++; break;
      case EH_BTN_DPAD: {
          count_by_output[EH_IX_LEFT]++;
          count_by_output[EH_IX_RIGHT]++;
          count_by_output[EH_IX_UP]++;
          count_by_output[EH_IX_DOWN]++;
        } break;
    }
  }
  
  // Assign axes to HORZ or VERT. Ties break to HORZ, as devices tend to report X before Y.
  while (count_by_output[EH_IX_AXES]>0) {
    count_by_output[EH_IX_AXES]--;
    int hc=(count_by_output[EH_IX_LEFT]<count_by_output[EH_IX_RIGHT])?count_by_output[EH_IX_LEFT]:count_by_output[EH_IX_RIGHT];
    int vc=(count_by_output[EH_IX_UP]<count_by_output[EH_IX_DOWN])?count_by_output[EH_IX_UP]:count_by_output[EH_IX_DOWN];
    for (rule=config->rulev,i=config->rulec;i-->0;rule++) {
      if (rule->dstbtnid==EH_BTN_AXES) {
        if (hc<=vc) {
          rule->dstbtnid=EH_BTN_HORZ;
          count_by_output[EH_IX_LEFT]++;
          count_by_output[EH_IX_RIGHT]++;
        } else {
          rule->dstbtnid=EH_BTN_VERT;
          count_by_output[EH_IX_UP]++;
          count_by_output[EH_IX_DOWN]++;
        }
        break;
      }
    }
  }
  
  // Assign triggers to L1 or R1. Ties break to L. We don't auto-assign to the '2' triggers.
  if (count_by_output[EH_IX_TRIGGERS]) {
    for (rule=config->rulev,i=config->rulec;i-->0;rule++) {
      if (rule->dstbtnid==EH_BTN_TRIGGERS) {
        count_by_output[EH_IX_TRIGGERS]--;
        if (count_by_output[EH_IX_L1]<=count_by_output[EH_IX_R1]) {
          rule->dstbtnid=EH_BTN_L1;
          count_by_output[EH_IX_L1]++;
        } else {
          rule->dstbtnid=EH_BTN_R1;
          count_by_output[EH_IX_R1]++;
        }
      }
    }
  }
  
  // Assign thumb buttons A,B,C,D (in that order).
  if (count_by_output[EH_IX_THUMBS]) {
    #define ac count_by_output[EH_IX_SOUTH]
    #define bc count_by_output[EH_IX_WEST]
    #define cc count_by_output[EH_IX_EAST]
    #define dc count_by_output[EH_IX_NORTH]
    for (rule=config->rulev,i=config->rulec;i-->0;rule++) {
      if (rule->dstbtnid==EH_BTN_THUMBS) {
        count_by_output[EH_IX_THUMBS]--;
        if ((ac<=bc)&&(ac<=cc)&&(ac<=dc)) { rule->dstbtnid=EH_BTN_SOUTH; ac++; }
        else if ((bc<=cc)&&(cc<=dc)) { rule->dstbtnid=EH_BTN_WEST; bc++; }
        else if (cc<=dc) { rule->dstbtnid=EH_BTN_EAST; cc++; }
        else { rule->dstbtnid=EH_BTN_NORTH; dc++; }
      }
    }
    #undef ac
    #undef bc
    #undef cc
    #undef dc
  }
  
  // Rules assigned to ANY can go to any two-state output. Lowest first.
  if (count_by_output[EH_IX_ANY]) {
    for (rule=config->rulev,i=config->rulec;i-->0;rule++) {
      if (rule->dstbtnid==EH_BTN_ANY) {
        count_by_output[EH_IX_ANY]--;
        int ix=0,q=0;
        for (;q<12;q++) {
          if (count_by_output[q]<count_by_output[ix]) ix=q;
        }
        rule->dstbtnid=1<<ix;
        count_by_output[ix]++;
      }
    }
  }
  
  // At this point we could unlump, eg if there's two HORZ and no VERT, reassign one.
  // I think that won't be necessary, because we usually don't assign concrete outputs in the first pass,
  // and the second pass (just above) distributes uniformly.
  
  // Finally, the config is usable if we have a dpad and A.
  if (
    count_by_output[EH_IX_UP]&&
    count_by_output[EH_IX_DOWN]&&
    count_by_output[EH_IX_LEFT]&&
    count_by_output[EH_IX_RIGHT]&&
    count_by_output[EH_IX_SOUTH]
  ) return 1;
  return 0;
}

/* Generate rules for the system keyboard.
 * We don't query capabilities in this case, since it's fair to make a lot of assumptions.
 */
 
static int eh_set_simple_map(struct eh_inmgr_config *config,uint32_t srcbtnid,uint32_t dstbtnid) {
  struct eh_inmgr_rule *rule=eh_inmgr_config_add_rule(config,srcbtnid);
  if (!rule) return -1;
  rule->dstbtnid=dstbtnid;
  return 0;
}
 
static int eh_generate_default_keyboard_mapping(struct eh_inmgr *inmgr,struct eh_inmgr_config *config) {
  if (eh_set_simple_map(config,0x00070050,EH_BTN_LEFT)<0) return -1;
  if (eh_set_simple_map(config,0x0007004f,EH_BTN_RIGHT)<0) return -1;
  if (eh_set_simple_map(config,0x00070052,EH_BTN_UP)<0) return -1;
  if (eh_set_simple_map(config,0x00070051,EH_BTN_DOWN)<0) return -1;
  if (eh_set_simple_map(config,0x0007001d,EH_BTN_SOUTH)<0) return -1;
  if (eh_set_simple_map(config,0x0007001b,EH_BTN_WEST)<0) return -1;
  if (eh_set_simple_map(config,0x00070004,EH_BTN_EAST)<0) return -1;
  if (eh_set_simple_map(config,0x00070016,EH_BTN_NORTH)<0) return -1;
  if (eh_set_simple_map(config,0x00070035,EH_BTN_L1)<0) return -1;
  if (eh_set_simple_map(config,0x0007002a,EH_BTN_R1)<0) return -1;
  if (eh_set_simple_map(config,0x0007002b,EH_BTN_L2)<0) return -1;
  if (eh_set_simple_map(config,0x00070031,EH_BTN_R2)<0) return -1;
  if (eh_set_simple_map(config,0x00070028,EH_BTN_AUX1)<0) return -1;
  if (eh_set_simple_map(config,0x0007002c,EH_BTN_AUX2)<0) return -1;
  if (eh_set_simple_map(config,0x00070038,EH_BTN_AUX3)<0) return -1;
  if (eh_set_simple_map(config,0x00070029,EH_BTN_QUIT)<0) return -1;
  if (eh_set_simple_map(config,0x00070044,EH_BTN_FULLSCREEN)<0) return -1;
  return 0;
}

/* Make up a config for a physical device.
 * (driver) is allowed to be NULL, and in that case we assume it's the System Keyboard.
 * TODO: In that case, we should create a static mapping. Keyboards are all alike.
 */

struct eh_inmgr_config *eh_inmgr_guess_config(struct eh_inmgr *inmgr,struct eh_input_driver *driver,int devid) {
  if (!devid) return 0;
  struct eh_inmgr_config *config=eh_inmgr_configv_append(inmgr);
  if (!config) return 0;
  
  uint16_t vid=0,pid=0;
  const char *name="System Keyboard";
  if (driver) {
    name=driver->type->get_ids(&vid,&pid,driver,devid);
    config->vendor=vid;
    config->product=pid;
  }
  int namec=0;
  if (name) while (name[namec]) namec++;
  if ((namec>0)&&(eh_inmgr_config_set_name(config,name,namec)<0)) {
    eh_inmgr_configv_remove(inmgr,config);
    return 0;
  }
  
  //fprintf(stderr,"%s %04x:%04x %.*s\n",__func__,config->vendor,config->product,namec,name?name:"");

  if (driver) {
    if (driver->type->list_buttons(driver,devid,eh_inmgr_config_cb_button,config)<0) {  
      eh_inmgr_configv_remove(inmgr,config);
      return 0;
    }
  } else {
    if (eh_generate_default_keyboard_mapping(inmgr,config)<0) {
      eh_inmgr_configv_remove(inmgr,config);
      return 0;
    }
  }
  
  if (eh_inmgr_config_finalize(config)<=0) {
    eh_inmgr_configv_remove(inmgr,config);
    return 0;
  }
  
  if (inmgr->delegate.cb_config_dirty) {
    inmgr->delegate.cb_config_dirty(inmgr->delegate.userdata);
  }
  fprintf(stderr,"...%s config=%p\n",__func__,config);
  return config;
}
