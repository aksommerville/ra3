/* db_service.h
 * Brokers our communication with Romassist and caches the search state.
 */
 
#ifndef DB_SERVICE_H
#define DB_SERVICE_H

struct sr_decoder;

struct db_service {
  char *gamelist; int gamelistc; // JSON array
  int gamelistseq; // Increments each time the list changes, so clients can poll.
  int gameid;
};

void dbs_cleanup(struct db_service *dbs);

void dbs_http_response(struct db_service *dbs,const char *src,int srcc);
void dbs_refresh_search(struct db_service *dbs);

/* Don't set (dbs->gameid) directly, call this instead.
 * I might attach some logic to the changes.
 */
void dbs_select_game(struct db_service *dbs,int gameid);

/* If this game is in the current list, return >=0 and populate (dst) with a decoder inside the game's record.
 */
int dbs_get_game_from_list(struct sr_decoder *dst,const struct db_service *dbs,int gameid);

#endif
