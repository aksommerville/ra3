#ifndef EH_AUTO_COLLECT_METADATA_H
#define EH_AUTO_COLLECT_METADATA_H

struct eh_auto_collect_metadata {
  int stage;
};

int eh_auto_collect_metadata_update(struct eh_auto_collect_metadata *acm);
int eh_auto_collect_metadata_fb(const void *fb);

#endif
