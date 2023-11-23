/* db.h
 * The Romassist database.
 */
 
#ifndef DB_H
#define DB_H

#include <stdint.h>

struct db;
struct db_game;
struct db_launcher;
struct db_list;
struct db_play;
struct db_comment;
struct sr_encoder;
struct sr_decoder;

/* Format for encoding.
 * We'll surely support JSON. Other formats... maybe?
 */
#define DB_FORMAT_json 1
#define DB_FORMAT_ingame 2 /* JSON for comment and play within a game, a slightly reduced format. Internal use, generally. */
#define DB_FORMAT_FOR_EACH \
  _(json) \
  _(ingame)

/* Detail for encoding files.
 * Zero for the default, "record".
 */
#define DB_DETAIL_id       1 /* Just the ID, as an integer. */
#define DB_DETAIL_name     2 /* {id,name} */
#define DB_DETAIL_record   3 /* {id,name,platform,author,genre,flags,rating,pubtime,path}, everything in the main record. */
#define DB_DETAIL_comments 4 /* record + {comments:[{time,k,v},...]} */
#define DB_DETAIL_plays    5 /* comments + {plays:[{time,dur_m},...]} */
#define DB_DETAIL_lists    6 /* plays + {lists:[{listid,name},...]} */
#define DB_DETAIL_blobs    7 /* lists + {blobs:[path...]} */
#define DB_DETAIL_EVERYTHING DB_DETAIL_blobs
#define DB_DETAIL_FOR_EACH \
  _(id) \
  _(name) \
  _(record) \
  _(comments) \
  _(plays) \
  _(lists) \
  _(blobs)

/* Database context.
 * Normally you will have just one, for the program's lifetime.
 *********************************************************************/
 
void db_del(struct db *db);

/* New database.
 * If you provide (root), we record it and load before returning.
 */
struct db *db_new(const char *root);

/* Root path is a directory whose contents are managed exclusively by db.
 * We create it on save if missing, but we won't create parent directories of it.
 * Changing root does not clear content or load.
 */
const char *db_get_root(struct db *db);
int db_set_root(struct db *db,const char *root,int rootc);

/* Save, load, and an overall dirty flag.
 * Saving is smart, it only touches things we think are dirty.
 * Call db_dirty() first to forcibly rewrite everything, if you somehow know better than us.
 */
int db_save(struct db *db);
int db_load(struct db *db);
int db_is_dirty(const struct db *db);
void db_dirty(struct db *db);

/* Drop all content.
 * This is not a normal thing to do.
 */
void db_clear(struct db *db);

/* You should do this from time to time to eliminate unused strings, etc.
 * Potentially very expensive.
 */
int db_gc(struct db *db);

/* Call if you happen to know that blobs have been added or removed.
 * Invalidating one gameid also invalidates the hundred surrounding it.
 * Left on its own, the db generally does not invalidate its blob cache.
 */
void db_invalidate_blobs(struct db *db);
void db_invalidate_blobs_for_gameid(struct db *db,uint32_t gameid);

/* Primitives.
 *********************************************************************/

/* Strings are an atom store.
 * If you intern the same value twice, we guarantee to return the same stringid.
 * String zero is implicitly the empty string.
 */
uint32_t db_string_intern(struct db *db,const char *src,int srcc);
uint32_t db_string_lookup(const struct db *db,const char *src,int srcc);
int db_string_get(void *dstpp,const struct db *db,uint32_t stringid);
int db_encode_json_string(struct sr_encoder *dst,const struct db *db,const char *k,int kc,uint32_t stringid);
int db_decode_json_string(uint32_t *stringid,struct db *db,struct sr_decoder *src);

/* Timestamps are multiple human-friendly fields packed into 32 bits, at one minute resolution.
 * These use the local time zone, and if that changes on us, pfft whatever.
 * In general you can blank out fields, so "2023" is a sensible timestamp meaning only the year 2023.
 * Text is always ISO 8601: "YYYY[-MM[-DD[THH[:MM]]]]". You can include ":SS.FFF+ZZZZ" and we quietly ignore.
 *   0xfff00000 Year 0..4095 AD.
 *   0x000f0000 Month 1..12. (0=unspec)
 *   0x0000f800 Day of month 1..31. (0=unspec)
 *   0x000007c0 Hour 0..23.
 *   0x0000003f Minute 0..59.
 * When the more precise fields are omitted, regular eval returns the lowest described timestamp (eg "2023" => "2023-00-00T00:00").
 * Use db_time_eval_upper() to get the highest (eg "2023" => "2023-15-31T31:63").
 */
uint32_t db_time_now();
uint32_t db_time_eval(const char *src,int srcc);
uint32_t db_time_eval_upper(const char *src,int srcc);
int db_time_repr(char *dst,int dsta,uint32_t dbtime);
uint32_t db_time_pack(int year,int month,int day,int hour,int minute);
int db_time_unpack(int *year,int *month,int *day,int *hour,int *minute,uint32_t dbtime);
uint32_t db_time_advance(uint32_t from); // => lowest valid time > from, 0 if overflowed
uint32_t db_time_diff_m(uint32_t older,uint32_t newer); // => difference in minutes (result is not a time)
#define DB_YEAR_FROM_TIME(time) ((time)>>20)

/* Flags are 32 bits with hard-coded meanings, for each game.
 * One debates making these user-configurable. I've opted to hard-code to keep it simple.
 * Flag names are canned C identifiers.
 * Ones I haven't named yet are formed "flag0" .. "flag31", and those strings can always evaluate even if defined.
 * The plural form produces a space-delimited limit, and consumes all kinds of things.
 */
const char *db_flag_repr(uint32_t flag_1bit);
uint32_t db_flag_eval(const char *src,int srcc); // => single bit, or zero if invalid
int db_flags_repr(char *dst,int dsta,uint32_t flags);
int db_flags_eval(uint32_t *dst,const char *src,int srcc);
#define DB_FLAG_player1      0x00000001
#define DB_FLAG_player2      0x00000002
#define DB_FLAG_player3      0x00000004
#define DB_FLAG_player4      0x00000008
#define DB_FLAG_playermore   0x00000010
#define DB_FLAG_faulty       0x00000020
#define DB_FLAG_foreign      0x00000040
#define DB_FLAG_hack         0x00000080
#define DB_FLAG_hardware     0x00000100
#define DB_FLAG_review       0x00000200
#define DB_FLAG_obscene      0x00000400
#define DB_FLAG_favorite     0x00000800
#define DB_FLAG_FOR_EACH /* only the ones named here */ \
  _(player1) \
  _(player2) \
  _(player3) \
  _(player4) \
  _(playermore) \
  _(faulty) \
  _(foreign) \
  _(hack) \
  _(hardware) \
  _(review) \
  _(obscene) \
  _(favorite)

/* Game table.
 * Stored as fixed-length records in one file.
 ***********************************************************************/
 
#define DB_GAME_NAME_LIMIT 32
#define DB_GAME_BASE_LIMIT 32
 
struct db_game {
  uint32_t gameid; // READONLY
  uint32_t platform; // string
  uint32_t author; // string
  uint32_t genre; // string
  uint32_t flags;
  uint32_t rating; // 0..99, with 0 meaning unset
  uint32_t pubtime; // Usually just the year.
  uint32_t dir; // string. Combine with (base) for the file's path.
  char name[DB_GAME_NAME_LIMIT];
  char base[DB_GAME_BASE_LIMIT];
};

int db_game_count(const struct db *db);
struct db_game *db_game_get_by_index(const struct db *db,int p);
struct db_game *db_game_get_by_id(const struct db *db,uint32_t gameid);

/* Remove this record and cascade:
 *  - Remove from all lists.
 *  - Drop all comments and plays.
 *  - Drop all blobs.
 * This likely orphans some strings, so you'll want to db_gc() at some point after.
 * Note that dropping blobs is immediate, but other changes aren't written until you save.
 */
int db_game_delete(struct db *db,uint32_t gameid);

/* Input is optional for insert. If you supply (gameid) it must not exist yet.
 * Input is required for update, and (gameid) must exist.
 * In both cases, you get the resident record back on success.
 */
struct db_game *db_game_insert(struct db *db,const struct db_game *game);
struct db_game *db_game_update(struct db *db,const struct db_game *game);

/* Conveniences to modify content in the record.
 * Strongly recommend using these instead of writing directly.
 * We manage the dirty flags and everything.
 */
int db_game_set_platform(struct db *db,struct db_game *game,const char *src,int srcc);
int db_game_set_author(struct db *db,struct db_game *game,const char *src,int srcc);
int db_game_set_flags(struct db *db,struct db_game *game,uint32_t flags);
int db_game_set_rating(struct db *db,struct db_game *game,uint32_t rating);
int db_game_set_pubtime(struct db *db,struct db_game *game,const char *iso8601,int c);
int db_game_set_path(struct db *db,struct db_game *game,const char *src,int srcc);
int db_game_set_name(struct db *db,struct db_game *game,const char *src,int srcc);

/* If you modify the record directly, call this.
 */
void db_game_dirty(struct db *db,struct db_game *game);

int db_game_get_path(char *dst,int dsta,const struct db *db,const struct db_game *game);

/* Comment table.
 * A comment is loose key=value data associated with one game.
 * Comments do not have IDs of their own; (gameid,time,k) is its primary key.
 ******************************************************************/
 
struct db_comment {
  uint32_t gameid; // READONLY
  uint32_t time; // READONLY
  uint32_t k; // string; READONLY
  uint32_t v; // string
};

int db_comment_count(const struct db *db);
struct db_comment *db_comment_get_by_index(const struct db *db,int p);
int db_comments_get_by_gameid(struct db_comment **dst,const struct db *db,uint32_t gameid); // => c

/* (comment) is for matching, it doesn't need to be the resident record.
 * But it does need to match (gameid,time,k) exactly.
 */
int db_comment_delete(struct db *db,const struct db_comment *comment);
int db_comment_delete_for_gameid(struct db *db,uint32_t gameid);

struct db_comment *db_comment_insert(struct db *db,const struct db_comment *comment);
struct db_comment *db_comment_update(struct db *db,const struct db_comment *comment);

int db_comment_set_v(struct db *db,struct db_comment *comment,const char *src,int srcc);

void db_comment_dirty(struct db *db,struct db_comment *comment);

/* Play table.
 * Each play record is one launch of the game.
 * (gameid,time) is a primary key. When creating a record, we may cheat time forward to avoid collisions.
 *****************************************************************/
 
struct db_play {
  uint32_t gameid; // READONLY
  uint32_t time; // READONLY
  uint32_t dur_m; // duration in minutes, since that's the resolution of our timestamps
};

int db_play_count(const struct db *db);
struct db_play *db_play_get_by_index(const struct db *db,int p);
struct db_play *db_play_get_by_gameid_time(const struct db *db,uint32_t gameid,uint32_t time);
int db_plays_get_by_gameid(struct db_play **dst,const struct db *db,uint32_t gameid); // => c

int db_play_delete(struct db *db,uint32_t gameid,uint32_t time);
int db_play_delete_for_gameid(struct db *db,uint32_t gameid);

struct db_play *db_play_insert(struct db *db,const struct db_play *play);
struct db_play *db_play_update(struct db *db,const struct db_play *play);

/* If a play exists for this (gameid) with zero (dur_m), update it according to current time.
 * If that works out to less than a minute, clamp to 1.
 */
struct db_play *db_play_finish(struct db *db,uint32_t gameid);

int db_play_set_dur_m(struct db *db,struct db_play *play,uint32_t dur_m);

void db_play_dirty(struct db *db,struct db_play *play);

/* Launcher table.
 * Record of one means of launching games.
 * This is usually a description of an emulator.
 * There should also be a generic "native" launcher.
 * And any exotic launch scenarios might want their own.
 *******************************************************************************/
 
struct db_launcher {
  uint32_t launcherid; // READONLY
  uint32_t name; // string; unique to this launcher, eg "akfceu"
  uint32_t platform; // string; for matching games, eg "nes"
  uint32_t suffixes; // string; comma-delimited list of path suffixes
  uint32_t cmd; // string; shell command. "$FILE" replaced with game's path.
  uint32_t desc; // string; commentary from user
};

int db_launcher_count(const struct db *db);
struct db_launcher *db_launcher_get_by_index(const struct db *db,int p);
struct db_launcher *db_launcher_get_by_id(const struct db *db,uint32_t launcherid);

/* Look up a game and select the most appropriate launcher.
 */
struct db_launcher *db_launcher_for_gameid(const struct db *db,uint32_t gameid);

int db_launcher_delete(struct db *db,uint32_t launcherid);

struct db_launcher *db_launcher_insert(struct db *db,const struct db_launcher *launcher);
struct db_launcher *db_launcher_update(struct db *db,const struct db_launcher *launcher);

int db_launcher_set_name(struct db *db,struct db_launcher *launcher,const char *src,int srcc);
int db_launcher_set_platform(struct db *db,struct db_launcher *launcher,const char *src,int srcc);
int db_launcher_set_glob(struct db *db,struct db_launcher *launcher,const char *src,int srcc);
int db_launcher_set_cmd(struct db *db,struct db_launcher *launcher,const char *src,int srcc);
int db_launcher_set_desc(struct db *db,struct db_launcher *launcher,const char *src,int srcc);

void db_launcher_dirty(struct db *db,struct db_launcher *launcher);

/* List.
 * Lists are db records, and also serve as a utility type.
 * Unlike most records, they are variable-length, and so are not dumped verbatim into the backing file.
 ************************************************************************/
 
struct db_list {
  uint32_t listid; // READONLY; zero for non-resident lists
  uint32_t name; // string
  uint32_t desc; // string
  uint32_t sorted; // If nonzero, we force (gameidv) to remain sorted by gameid ascending.
  int gameidc,gameida;
  uint32_t *gameidv; // the canonical data, as stored
  int gamec,gamea; // Optionally copy game records into the list for convenience.
  struct db_game *gamev;
};

/* Free this object and its content.
 * *** DO NOT DO THIS TO RESIDENT LISTS ***
 */
void db_list_del(struct db_list *list);

/* WARNING: Unlike all other records, you can't increment a list returned by index.
 * If you want a range of lists by index, you must call individually for each.
 */
int db_list_count(const struct db *db);
struct db_list *db_list_get_by_index(const struct db *db,int p);
struct db_list *db_list_get_by_id(const struct db *db,uint32_t listid);

/* ID or Name.
 */
struct db_list *db_list_get_by_string(const struct db *db,const char *src,int srcc);

int db_list_delete(struct db *db,uint32_t listid);

/* Copy a list, either resident or non-resident.
 * You must eventually del non-resident lists.
 * The new list will not have (gamev) populated initially, regardless of what's in (src).
 */
struct db_list *db_list_copy_nonresident(const struct db_list *src);
struct db_list *db_list_copy_resident(struct db *db,const struct db_list *src);

/* Updating replaces the entire gameid array.
 */
struct db_list *db_list_insert(struct db *db,const struct db_list *list);
struct db_list *db_list_update(struct db *db,const struct db_list *list);

int db_list_set_name(struct db *db,struct db_list *list,const char *src,int srcc);
int db_list_set_desc(struct db *db,struct db_list *list,const char *src,int srcc);

void db_list_dirty(struct db *db,struct db_list *list);

int db_list_has(const struct db_list *list,uint32_t gameid);
int db_list_append(struct db *db,struct db_list *list,uint32_t gameid);
int db_list_remove(struct db *db,struct db_list *list,uint32_t gameid);
int db_list_clear(struct db *db,struct db_list *list);

/* The two (gameid) sorts do not use (gamev) -- (gamev) goes out of sync if present.
 * Generic sort populates (gamev) first automatically, and both (gameidv) and (gamev) will get sorted.
 * Sorting by gameid asc also sets the (sorted) flag, so it stays that way.
 */
int db_list_sort_gameid_asc(struct db *db,struct db_list *list);
int db_list_sort_gameid_desc(struct db *db,struct db_list *list);
int db_list_sort(
  struct db *db,
  struct db_list *list,
  int (*cmp)(const struct db_game *a,const struct db_game *b,void *userdata),
  void *userdata
);

/* Populates (gamev) automatically and updates both (gamev) and (gameidv).
 * Return nonzero to keep a game.
 */
int db_list_filter(
  struct db *db,
  struct db_list *list,
  int (*filter)(const struct db_game *game,void *userdata),
  void *userdata
);

/* For your convenience, we can make a parallel array of copied game records.
 * By default we only store IDs.
 * (gamev) goes out of sync on any other change.
 */
void db_list_gamev_drop(struct db_list *list);
int db_list_gamev_populate(const struct db *db,struct db_list *list);

/* Blobs.
 * Blobs are unlike other stores in that each record is a discrete file on disk.
 * We don't do any additional bookkeeping on them, the filesystem takes care of it all.
 * A consequence of this is all blob changes take effect immediately, not at save/load.
 * Also, listing blobs hits the filesystem every time, it could be expensive.
 * The blob's path tells us everything about it:
 *   DBROOT/blob/1200/1234-scap-202310141153.png
 *          |    |    |    |    ^^^^^^^^^^^^ ---- Timestamp (higher resolution than db timestamps)
 *          |    |    |    ^^^^ ----------------- Data type. "scap"=screencap, and maybe we'll do other things in the future.
 *          |    |    ^^^^ ---------------------- gameid
 *          |    ^^^^ --------------------------- Games bucketted by the hundred.
 *          ^^^^ -------------------------------- All blobs under this directory.
 * Valid blob paths must begin with gameid, contain two dashes, be in the correct bucket, and the game must exist.
 **********************************************************************************/

/* Trigger callback for every blob in the db, until you return nonzero.
 * If (include_invalid), we report every regular file under in the "blob" directory, 
 * and (gameid,type,time) might be unavailable.
 * We do not read the individual blob files during iteration.
 */
int db_blob_for_each(
  struct db *db,
  int include_invalid,
  int (*cb)(uint32_t gameid,const char *type,int typec,const char *time,int timec,const char *path,void *userdata),
  void *userdata
);

/* Even without (include_invalid), this will ignore the requirement that (gameid) exist.
 * Beyond that, all the same rules as full iteration.
 */
int db_blob_for_gameid(
  struct db *db,
  uint32_t gameid,
  int include_invalid,
  int (*cb)(uint32_t gameid,const char *type,int typec,const char *time,int timec,const char *path,void *userdata),
  void *userdata
);

/* Generate a path for a new blob, using current time.
 * (gameid) must exist.
 * Creates parent directories if necessary. But does not write the file, obviously.
 * (sfx) is optional. eg ".png" for screencaps.
 */
char *db_blob_compose_path(
  const struct db *db,
  uint32_t gameid,
  const char *type,int typec,
  const char *sfx,int sfxc
);

/* >=0 if this looks like a blob path.
 * The HTTP server should call this before serving blob content, to ensure it's not serving blobs like "/etc/shadow".
 * We don't check whether the file actually exists, you'll find out soon enough.
 */
int db_blob_validate_path(
  const struct db *db,
  const char *path,int pathc
);

int db_blob_delete_for_gameid(struct db *db,uint32_t gameid);

/* Encoding and decoding records.
 **************************************************************************/

/* Encode one record to a given format and detail level.
 * db_list_encode_array() only encodes its game records; plain db_list_encode() includes the list's own header.
 * The only format currently allowed is DB_FORMAT_json.
 * Games are special, they may include content from other tables depending on (detail).
 * All encode functions begin with a "no_setup" encoder call. If you're inside some other JSON structure, use sr_encode_json_setup() first.
 */
int db_game_encode(struct sr_encoder *dst,const struct db *db,const struct db_game *game,int format,int detail);
int db_comment_encode(struct sr_encoder *dst,const struct db *db,const struct db_comment *comment,int format,int detail);
int db_play_encode(struct sr_encoder *dst,const struct db *db,const struct db_play *play,int format,int detail);
int db_launcher_encode(struct sr_encoder *dst,const struct db *db,const struct db_launcher *launcher,int format,int detail);
int db_list_encode(struct sr_encoder *dst,const struct db *db,const struct db_list *list,int format,int detail);
int db_list_encode_array(struct sr_encoder *dst,const struct db *db,const struct db_list *list,int format,int detail);

/* Normally you decode to a nonresident scratch record.
 * Beware that strings are interned against (db) as needed, at this time.
 * But the record itself doesn't enter the database until you insert or update, presumably right after decode.
 * When decoding JSON, we guarantee not to touch fields that aren't named in the input.
 * Beware that we *will* decode primary keys just like any data field. If you're expecting a specific id, you must check after decoding.
 * The external data in an encoded game (comments, plays, lists, blobs) is ignored on decode.
 */
int db_game_decode(struct db_game *game,struct db *db,int format,const void *src,int srcc);
int db_comment_decode(struct db_comment *comment,struct db *db,int format,const void *src,int srcc);
int db_play_decode(struct db_play *play,struct db *db,int format,const void *src,int srcc);
int db_launcher_decode(struct db_launcher *launcher,struct db *db,int format,const void *src,int srcc);
int db_list_decode(struct db_list *list,struct db *db,int format,const void *src,int srcc);
int db_list_decode_array(struct db_list *list,struct db *db,int format,const void *src,int srcc);

/* High-level queries.
 * Results come back in a new sorted non-resident list which you must delete.
 * Queries take an optional (src) list. If null, they operate against the entire DB.
 * We won't modify (src) lists, but will force (gamev) in them.
 ***************************************************************************/
 
/* Extra high-level support for our HTTP service.
 * Coordinates multiple query methods, pagination, encoding, the works.
 * db_query_add_parameter() is carefully crafted to fit http_xfer_for_query() exactly.
 * Search parameters:
 *   text: Loose text search.
 *   list: Restrict to this list ID or exact name. May provide more than once.
 *   platform,author,genre: Exact string match against header. Empty is not valid, it will match all, as if unspecified.
 *   flags,notflags: Comma-delimited list of flag names.
 *   rating: "N" "..N" "N.." "N..N". It's weird to search for exactly one value.
 *   pubtime: "T" "..T" "T.." "T..T"
 * Formatting etc:
 *   detail: eg "name", "record"
 *   sort: DB_SORT_*
 *   limit: Maximum result count to return.
 *   page: 1-based page number, if (limit) in play.
 */
struct db_query;
void db_query_del(struct db_query *query);
struct db_query *db_query_new(struct db *db);
int db_query_add_parameter(const char *k,int kc,const char *v,int vc,void *query);
int db_query_finish(struct sr_encoder *dst,struct db_query *query); // Null (dst) if you don't need it encoded.
int db_query_get_page_count(const struct db_query *query); // => 0 if pagination not requested
struct db_list *db_query_get_results(const struct db_query *query); // Finish first.

/* Select among (gameidv), one game we can randomly recommend to the player.
 *  - Eliminate any with (launcher=never). (NB we search for the string "never", unlike the smarter db_launcher_for_gameid).
 *  - Eliminate any with "faulty" flag.
 *  - Eliminate any with (rating<10), unless they all are.
 *  - Weight all remaining games by essentially (rating*age), age in weeks to the last play, both clamped to 1..100.
 * The age parameter helps keep results fresh: If you play whatever we return, it becomes very unlikely in the next few calls.
 * But it's always random, and anything that doesn't get eliminated could be selected.
 */
uint32_t db_query_choose_random(struct db *db,const uint32_t *gameidv,int gameidc);

/* Loose text.
 * Return anything that matches case-insensitively in name, basename, or comments.
 * If you're also applying a header query, do that first!
 */
struct db_list *db_query_text(
  struct db *db,
  struct db_list *src,
  const char *q,int qc
);

/* Simple case, query against the game record only.
 * Empty strings match all, otherwise exact matches only.
 * Numeric fields have (lo,hi), but (0,0) matches all.
 * "prelookupped" if you already have stringids for the string fields.
 */
struct db_list *db_query_header(
  struct db *db,
  struct db_list *src,
  const char *platform,int platformc,
  const char *author,int authorc,
  const char *genre,int genrec,
  uint32_t flags_require,
  uint32_t flags_forbid,
  uint32_t rating_lo,
  uint32_t rating_hi,
  uint32_t pubtime_lo,
  uint32_t pubtime_hi
);
struct db_list *db_query_header_prelookupped(
  struct db *db,
  struct db_list *src,
  uint32_t platform,
  uint32_t author,
  uint32_t genre,
  uint32_t flags_require,
  uint32_t flags_forbid,
  uint32_t rating_lo,
  uint32_t rating_hi,
  uint32_t pubtime_lo,
  uint32_t pubtime_hi
);

/* You do the matching.
 */
struct db_list *db_query_generic(
  struct db *db,
  struct db_list *src,
  int (*filter)(const struct db_game *game,void *userdata),
  void *userdata
);

/* Logical operations on lists.
 * These can work much more efficiently if you sort both lists by gameid.
 */
struct db_list *db_query_list_and(struct db *db,const struct db_list *a,const struct db_list *b);
struct db_list *db_query_list_or(struct db *db,const struct db_list *a,const struct db_list *b);
struct db_list *db_query_list_not(struct db *db,const struct db_list *from,const struct db_list *remove);

/* New nonresident list with every game added.
 */
struct db_list *db_list_everything(const struct db *db);

#define DB_SORT_none      0
#define DB_SORT_id        1
#define DB_SORT_name      2
#define DB_SORT_pubtime   3
#define DB_SORT_rating    4 /* ...to this point tend to be cheap. */
#define DB_SORT_playtime  5 /* The rest are increasingly expensive, requiring calls out to other tables... */
#define DB_SORT_playcount 6
#define DB_SORT_fullness  7 /* Subjective judgment, does this record look like something's missing? */
#define DB_SORT_FOR_EACH \
  _(none) \
  _(id) \
  _(name) \
  _(pubtime) \
  _(rating) \
  _(playtime) \
  _(playcount) \
  _(fullness)

/* Identifiers as above, plus an optional leading "+" or "-". ("+" is noop)
 */
int db_sort_eval(int *descend,const char *src,int srcc);
  
int db_list_sort_auto(struct db *db,struct db_list *list,int sort,int descend);

/* Convenience, and only valid for non-resident lists.
 * Filters (list) down to no more than (page_size) contiguous records, aligned to a multiple of (page_size).
 * (page_index) is from zero.
 * Returns the count of pages.
 */
int db_list_paginate(struct db_list *list,int page_size,int page_index);

struct db_histogram {
  struct db_histogram_entry {
    uint32_t stringid;
    uint32_t count;
  } *v;
  int c,a;
};

void db_histogram_cleanup(struct db_histogram *hist);

int db_histogram_search(const struct db_histogram *hist,uint32_t stringid);
struct db_histogram_entry *db_histogram_insert(struct db_histogram *hist,int p,uint32_t stringid);
int db_histogram_add(struct db_histogram *hist,uint32_t stringid,int addc); // zero is ok, to assert the entry exists but no count

/* Histograms sort by stringid, by default, and that's not an order you'd ever want.
 * If order matters at all to the caller, sort before encoding, only after you've fully populated the histogram.
 */
void db_histogram_sort_alpha(struct db_histogram *hist,const struct db *db);
void db_histogram_sort_c_desc(struct db_histogram *hist,const struct db *db);

/* List all known values of a given game field, and the count of occurrences.
 * You must clean up the histogram when done.
 * (platform) may include zero-count entries from launchers too.
 */
int db_histogram_platform(struct db_histogram *hist,const struct db *db);
int db_histogram_author(struct db_histogram *hist,const struct db *db);
int db_histogram_genre(struct db_histogram *hist,const struct db *db);

/* We can also histogram by rating and pubtime. Beware that "stringid" in the output is not actual strings.
 * (pubtime) uses granularity of one year, always (because that's usually all we have).
 * (rating) you can provide a bucket size. Output keys will be multiples of that, starting at zero.
 */
int db_histogram_rating(struct db_histogram *hist,const struct db *db,int bucket_size);
int db_histogram_pubtime(struct db_histogram *hist,const struct db *db);

/* Only JSON is currently supported.
 * Use DB_DETAIL_name for an array of strings, discarding count.
 * Or DB_DETAIL_record for [{v,c}...].
 * Or DB_DETAIL_id for [{v,c}...] but with numeric keys (for rating or pubtime).
 */
int db_histogram_encode(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_histogram *hist,
  int format,
  int detail
);

#endif
