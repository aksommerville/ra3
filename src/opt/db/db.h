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
#define DB_FORMAT_ingame 2 /* JSON for substructures of game, a slightly reduced format. */
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
#define DB_DETAIL_plays    5 /* comments + {plays:[{time,dur},...]} */
#define DB_DETAIL_lists    6 /* plays + {lists:[{id,name},...]} */
#define DB_DETAIL_blobs    6 /* lists + {blobs:[path...]} */
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
 */
uint32_t db_time_now();
uint32_t db_time_eval(const char *src,int srcc);
int db_time_repr(char *dst,int dsta,uint32_t dbtime);
uint32_t db_time_pack(int year,int month,int day,int hour,int minute);
int db_time_unpack(int *year,int *month,int *day,int *hour,int *minute,uint32_t dbtime);
uint32_t db_time_advance(uint32_t from); // => lowest valid time > from
uint32_t db_time_diff_m(uint32_t older,uint32_t newer); // => difference in minutes (result is not a time)

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

/* (gameid) zero to choose any available one, that's recommended.
 */
struct db_game *db_game_insert(struct db *db,uint32_t gameid);

/* Update any fields in (game) from the encoded JSON object in (src).
 * Fields not named in (src) are left as is.
 * If (id) is present in both, it must match.
 * (game) may be null to insert.
 * We take pains to operate transactionally; on a failure (db) should remain untouched.
 * Dirty flags are updated as needed.
 * *** comments, plays, and blobs are ignored if present ***
 */
int db_game_patch_json(struct db *db,struct db_game *game,const char *src,int srcc);

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

/* Serialize this game, and depending on (detail), its comments, plays, and blob list too.
 */
int db_game_encode(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_game *game,
  int format,
  int detail
);

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

/* Zero (time) to use current time.
 * (v) initially zero.
 */
struct db_comment *db_comment_insert(struct db *db,uint32_t gameid,uint32_t time,uint32_t k);

/* Add a comment from JSON-encoded content.
 * (src) must contain "gameid" and that game must exist.
 * This doesn't support updating existing records, since I expect that to be rare.
 * (Use db_comment_set_v if you need to).
 */
int db_comment_insert_json(struct db *db,const char *src,int srcc);

int db_comment_set_v(struct db *db,struct db_comment *comment,const char *src,int srcc);

void db_comment_dirty(struct db *db,struct db_comment *comment);

int db_comment_encode(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_comment *comment,
  int format
);

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

/* Generate a new record at current time, with zero duration.
 */
struct db_play *db_play_insert(struct db *db,uint32_t gameid);

/* If a play exists for this (gameid) with zero (dur_m), update it according to current time.
 * If that works out to less than a minute, clamp to 1.
 */
int db_play_finish(struct db *db,uint32_t gameid);

int db_play_set_dur_m(struct db *db,struct db_play *play,uint32_t dur_m);

void db_play_dirty(struct db *db,struct db_play *play);

int db_play_encode(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_play *play,
  int format
);

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

/* Normally (launcherid==0) for insert, we make one up.
 */
struct db_launcher *db_launcher_insert(struct db *db,uint32_t launcherid);
int db_launcher_delete(struct db *db,uint32_t launcherid);

/* (launcher) may be null to create a new one.
 * If ID present in both, they must match.
 * Fields absent in the JSON are left as is.
 */
int db_launcher_patch_json(struct db *db,struct db_launcher *launcher,const char *src,int srcc);

int db_launcher_set_name(struct db *db,struct db_launcher *launcher,const char *src,int srcc);
int db_launcher_set_platform(struct db *db,struct db_launcher *launcher,const char *src,int srcc);
int db_launcher_set_glob(struct db *db,struct db_launcher *launcher,const char *src,int srcc);
int db_launcher_set_cmd(struct db *db,struct db_launcher *launcher,const char *src,int srcc);
int db_launcher_set_desc(struct db *db,struct db_launcher *launcher,const char *src,int srcc);

void db_launcher_dirty(struct db *db,struct db_launcher *launcher);

int db_launcher_encode(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_launcher *launcher,
  int format
);

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

int db_list_count(const struct db *db);
struct db_list *db_list_get_by_index(const struct db *db,int p);
struct db_list *db_list_get_by_id(const struct db *db,uint32_t listid);

int db_list_delete(struct db *db,uint32_t listid);

/* Copy a list, either resident or non-resident.
 * You must eventually del non-resident lists.
 * The new list will not have (gamev) populated initially, regardless of what's in (src).
 */
struct db_list *db_list_copy_nonresident(const struct db_list *src);
struct db_list *db_list_copy_resident(struct db *db,const struct db_list *src);

/* Normally (listid==0), we make one up.
 */
struct db_list *db_list_insert(struct db *db,uint32_t listid);

/* If input contains "games", this replaces the entire games list.
 * (meaning, it doesn't "patch" the array).
 */
int db_list_patch_json(struct db *db,struct db_list *list,const char *src,int srcc);

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

/* Be mindful of (detail), you can cause an enormous amount of compute and serialize here.
 * For JSON format, produces {id,name,desc,sorted,games:[...]}.
 */
int db_list_encode(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_list *list,
  int format,
  int detail
);

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
  const struct db *db,
  int include_invalid,
  int (*cb)(uint32_t gameid,const char *type,int typec,const char *time,int timec,const char *path,void *userdata),
  void *userdata
);

/* Even without (include_invalid), this will ignore the requirement that (gameid) exist.
 * Beyond that, all the same rules as full iteration.
 */
int db_blob_for_gameid(
  const struct db *db,
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

int db_blob_delete_for_gameid(struct db *db,uint32_t gameid);

/* High-level queries.
 * Results come back in a new sorted non-resident list which you must delete.
 * Queries take an optional (src) list. If null, they operate against the entire DB.
 * We won't modify (src) lists, but will force (gamev) in them.
 ***************************************************************************/

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

/* List all known values of a given game field, and the count of occurrences.
 * You must clean up the histogram when done.
 * (platform) may include zero-count entries from launchers too.
 */
int db_histogram_platform(struct db_histogram *hist,const struct db *db);
int db_histogram_author(struct db_histogram *hist,const struct db *db);
int db_histogram_genre(struct db_histogram *hist,const struct db *db);

/* Only JSON is currently supported.
 * Use DB_DETAIL_id for an array of strings, discarding count.
 * Or DB_DETAIL_record for [{v,c}...].
 */
int db_histogram_encode(
  struct sr_encoder *dst,
  const struct db *db,
  const struct db_histogram *hist,
  int format,
  int detail
);

#endif
