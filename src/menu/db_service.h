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

#define DB_SERVICE_INTERACTION_other 0
#define DB_SERVICE_INTERACTION_list 1

struct db_service {

  // Current search results.
  char *gamelist; int gamelistc; // JSON array
  int gamelistseq; // Increments each time the list changes, so clients can poll.
  int gameid;
  int page,pagec; // 1-based
  int totalc;
  
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
  
  // DB_SERVICE_INTERACTION_*; the last thing that was set with a structured query parameter call.
  // Carousel uses this to know whether to propose deleting an empty list.
  int last_search_interaction;
  
  // Internal plumbing.
  int next_correlation_id;
  int search_correlation_id;
  int meta_correlation_id;
  char *state_path;
  struct dbs_listener {
    int corrid;
    void (*cb)(struct eh_http_response *rsp,void *userdata);
    void *userdata;
  } *listenerv;
  int listenerc,listenera;
  
  // Metadata.
  char *flagnames; int flagnamesc;
  char *lists; int listsc;
  char *genres; int genresc;
  char *authors; int authorsc;
  char *platforms; int platformsc;
  int daterange[2];
};

void dbs_cleanup(struct db_service *dbs);

int dbs_init(struct db_service *dbs);

int dbs_state_read(struct db_service *dbs);
int dbs_state_write(struct db_service *dbs);

void dbs_http_response(struct db_service *dbs,const char *src,int srcc);
void dbs_refresh_search(struct db_service *dbs);

// Repeats the search and changes gameid.
void dbs_randomize(struct db_service *dbs);

void dbs_refresh_all_metadata(struct db_service *dbs);

/* Don't set (dbs->gameid) directly, call this instead.
 * I might attach some logic to the changes.
 */
void dbs_select_game(struct db_service *dbs,int gameid);

/* If this game is in the current list, return >=0 and populate (dst) with a decoder inside the game's record.
 */
int dbs_get_game_from_list(struct sr_decoder *dst,const struct db_service *dbs,int gameid);

/* Convenience above dbs_get_game_from_list, to split out the most common fields.
 * Strings are encoded JSON tokens; use sr_string_eval().
 * Same for objects and arrays.
 */
struct dbs_game {
  int gameid;
  const char *name; int namec;
  const char *platform; int platformc;
  const char *author; int authorc;
  const char *genre; int genrec;
  const char *flags; int flagsc;
  int rating;
  int pubtime; // year only
  const char *path; int pathc;
  const char *comments; int commentsc; // {time,k,v}[]
  const char *lists; int listsc; // {?}[]
  const char *blobs; int blobsc; // string[]
};
int dbs_game_get(struct dbs_game *game,const struct db_service *dbs,int gameid);
int dbs_game_decode(struct dbs_game *game,const char *src,int srcc);

/* Sends PATCH /api/game, then refreshes the search.
 */
int dbs_replace_game_field_string(struct db_service *dbs,int gameid,const char *k,int kc,const char *v,int vc);
int dbs_replace_game_field_int(struct db_service *dbs,int gameid,const char *k,int kc,int v);

int dbs_create_list(struct db_service *dbs,const char *name,int namec);
int dbs_add_to_list(struct db_service *dbs,int gameid,const char *listid,int listidc);
int dbs_remove_from_list(struct db_service *dbs,int gameid,const char *list,int listidc);

void dbs_request_shutdown(struct db_service *dbs);

/* Instead of adding bespoke plumbing for every API call, for things without request bodies, use this.
 * Requesting returns 'corrid', which you can use to cancel it later.
 */
int dbs_request_http(
  struct db_service *dbs,
  const char *method,const char *path,
  void (*cb)(struct eh_http_response *rsp,void *userdata),
  void *userdata
);
void dbs_cancel_request(struct db_service *dbs,int corrid);

/* On a success, we may assume that the backend is going to terminate us.
 */
int dbs_launch(struct db_service *dbs,int gameid);

int dbs_search_set_list(struct db_service *dbs,const char *v,int c);
int dbs_search_set_text(struct db_service *dbs,const char *v,int c);
int dbs_search_set_pubtime(struct db_service *dbs,int lo,int hi);
int dbs_search_set_rating(struct db_service *dbs,int lo,int hi);
int dbs_search_set_flags(struct db_service *dbs,const char *v,int c);
int dbs_search_set_author(struct db_service *dbs,const char *v,int c);
int dbs_search_set_genre(struct db_service *dbs,const char *v,int c);
int dbs_search_set_platform(struct db_service *dbs,const char *v,int c);
int dbs_search_set_sort(struct db_service *dbs,const char *v,int c);

#endif
