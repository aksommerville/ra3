/* db_service.h
 * Brokers our communication with Romassist and caches the search state.
 */
 
#ifndef DB_SERVICE_H
#define DB_SERVICE_H

struct db_service {
  char *gamelist; int gamelistc; // JSON array
  int gamelistseq; // Increments each time the list changes, so clients can poll.
};

void dbs_cleanup(struct db_service *dbs);

void dbs_http_response(struct db_service *dbs,const char *src,int srcc);
void dbs_refresh_search(struct db_service *dbs);

#endif
