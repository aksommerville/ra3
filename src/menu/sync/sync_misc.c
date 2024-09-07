#include "sync_internal.h"

/* Enums.
 */
 
const char *sync_access_repr(int access) {
  switch (access) {
    case SYNC_ACCESS_READ: return "Read Only";
    case SYNC_ACCESS_WRITE: return "Write Only";
    case SYNC_ACCESS_RW: return "Read/Write";
  }
  return "";
}

const char *sync_no_ask_yes_repr(int v) {
  switch (v) {
    case -1: return "No";
    case 0: return "Ask";
    case 1: return "Yes";
  }
  return "";
}
