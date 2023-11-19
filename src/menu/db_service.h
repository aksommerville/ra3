/* db_service.h
 * Brokers our communication with Romassist and caches the search state.
 */
 
#ifndef DB_SERVICE_H
#define DB_SERVICE_H

struct sr_decoder;

/* Hard-coded page size for search results.
 * Keep this low enough that we needn't worry about memory for all those screencaps.
 */
#define DB_SERVICE_SEARCH_LIMIT 40

struct db_service {

  // Current search results.
  char *gamelist; int gamelistc; // JSON array
  int gamelistseq; // Increments each time the list changes, so clients can poll.
  int gameid;
  int page,pagec; // 1-based
  
  // Current query.
  char *list; int listc;
  char *text; int textc;
  int pubtimelo,pubtimehi; // year only
  int ratinglo,ratinghi;
  char *flags; int flagsc;
  char *author; int authorc;
  char *genre; int genrec;
  char *platform; int platformc;
  char *sort; int sortc;
  
  // Internal plumbing.
  int next_correlation_id;
  int search_correlation_id;
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

/* On a success, we may assume that the backend is going to terminate us.
 */
int dbs_launch(struct db_service *dbs,int gameid);

#endif
