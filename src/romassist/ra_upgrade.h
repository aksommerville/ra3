/* ra_upgrade.h
 * Various upgrade-related services, including the main event.
 * Record keeping lives in db.
 */
 
#ifndef RA_UPGRADE_H
#define RA_UPGRADE_H

/* Our context object doesn't get passed around.
 * There's just one: ra.upgrade
 */
struct ra_upgrade {
  int pid,fd;
  struct db_upgrade *inflight; // STRONG, nonresident
  struct db_upgrade *waitingv; // STRONG, nonresident
  int waitingc,waitinga;
  int noop; // Nonzero until we notice something in the output indicating that some action was taken. It's fuzzy at best.
};

int ra_upgrade_startup();
int ra_upgrade_update();

static inline int ra_upgrade_is_idle(const struct ra_upgrade *u) {
  return (!u->inflight&&!u->waitingc)?1:0;
}

/* Return a new array of nonresident upgrade records matching these criteria.
 * Inputs here should be exactly what we take in via HTTP.
 * Caller must free the returned list if return >0. If zero, (*dstpp) is null.
 */
int ra_upgrade_list(struct db_upgrade **dstpp,uint32_t upgradeid,uint32_t before);

/* Queue a new batch of upgrades.
 * Fails immediately if we are not idle.
 * (upgradev) can be resident or nonresident.
 */
int ra_upgrade_begin(struct db_upgrade *upgradev,int upgradec);

#endif
