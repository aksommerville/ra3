#ifndef RA_MIGRATE_INTERNAL_H
#define RA_MIGRATE_INTERNAL_H

#include "ra_internal.h"
#include "ra_minhttp.h"
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"
 
struct ra_migrate_counts {
  int beforec; // what we had initially
  int incomingc; // what the remote has
  int addc; // how many records added
  int modc; // how many existing records modified
  int ignorec; // something we don't have but couldn't add, probably interesting
  int skipc; // incoming things with no change
};
 
struct ra_migrate_context {
  struct ra_minhttp minhttp;
  const char *username;
  uint32_t str_native;
  uint32_t str_never;
  uint32_t str_git_make;
  
  // Raw JSON from the remote. Read only!
  struct sr_encoder games;
  struct sr_encoder lists;
  struct sr_encoder launchers;
  struct sr_encoder upgrades;
  // No need for comments, plays, or blobs; we get those with games.
  // Blob content, we'll fetch one by one.
  
  struct ra_migrate_counts gamecounts;
  struct ra_migrate_counts listcounts;
  struct ra_migrate_counts launchercounts;
  struct ra_migrate_counts upgradecounts;
  struct ra_migrate_counts commentcounts;
};

/* Startup. Call these three in order.
 */
int ra_migrate_introduce(struct ra_migrate_context *ctx);
int ra_migrate_connect(struct ra_migrate_context *ctx);
int ra_migrate_fetch(struct ra_migrate_context *ctx);

/* Last chance to report errors before committing database.
 * No work should be done here, just assertions.
 */
int ra_migrate_validate_final(struct ra_migrate_context *ctx);

/* Final report.
 * "_after_failed_commit" if saving the database failed.
 * In that case it's important that we tell the user about blobs and ROMs and such, that were committed immediately.
 * We do not try to roll those changes back.
 */
void ra_migrate_report_changes(struct ra_migrate_context *ctx);
void ra_migrate_report_changes_after_failed_commit(struct ra_migrate_context *ctx);

/* Find a record by ID in a JSON array. Depending on output vectors provided:
 *  - Decode to a nonresident record. Mind that strings will be interned, the database does change.
 *  - Populate a decoder with the JSON object.
 * No outputs is also sensible: Returns <0 if absent or >=0 if present.
 * You don't need the general version unless I forgot something.
 */
int ra_migrate_get_record(
  void *dbrecord,
  struct sr_decoder *text,
  const char *src,int srcc,
  const char *k,int kc, // eg "gameid", "listid", ...must be an integer.
  int id,
  void *decodefn // (int db_XXX_decode(XXX *record,struct db *db,int format,const void *src,int srcc))
);
int ra_migrate_get_game(struct db_game *game,struct sr_decoder *text,struct ra_migrate_context *ctx,int gameid);
int ra_migrate_get_list(struct db_list *list,struct sr_decoder *text,struct ra_migrate_context *ctx,int listid);
int ra_migrate_get_launcher(struct db_launcher *launcher,struct sr_decoder *text,struct ra_migrate_context *ctx,int launcherid);
int ra_migrate_get_upgrade(struct db_upgrade *upgrade,struct sr_decoder *text,struct ra_migrate_context *ctx,int upgradeid);
int ra_migrate_get_upgrade_by_gameid(struct db_upgrade *upgrade,struct sr_decoder *text,struct ra_migrate_context *ctx,int gameid);

/* Decode game JSON to a nonresident db record, and optionally capture JSON arrays of comments and blobs.
 * In general, decoding records should use db_XXX_decode(), but games have these two other fields we need.
 * (db_game_decode deliberately ignores comments, blobs, lists, and plays).
 */
int ra_migrate_decode_game(
  struct db_game *game,
  struct sr_decoder *comments,
  struct sr_decoder *blobs,
  struct ra_migrate_context *ctx,
  const char *src,int srcc
);

/* For a nonresident game record drawn from the remote (incoming), 
 * search our database and if we feel we already have this game, return that resident record.
 * Matching games is a subtle proposition.
 */
struct db_game *ra_migrate_find_existing_game(const struct db_game *incoming);

/* Return >0 and populate (update) if we feel this game should be updated or inserted.
 * Update is a hairy question: If we have different values for a field, which one does the user want?
 * We play that conservatively. Only fields that are zero in (existing) will cause an update.
 * When "insert" returns true, we have made up a sensible local path for it, ready to go.
 */
int ra_migrate_should_update_game(struct db_game *update,struct ra_migrate_context *ctx,const struct db_game *existing,const struct db_game *incoming);
int ra_migrate_should_insert_game(struct db_game *update,struct ra_migrate_context *ctx,const struct db_game *incoming);

/* Rewrite (dst) **in place** by replacing the remote username with our own.
 * Returns >0 only if replacement happened and it fits in (dsta) with a terminator.
 * (dst) must be NUL-terminated initially.
 */
int ra_migrate_replace_home_ip(char *dst,int dsta,struct ra_migrate_context *ctx);

/* (localgame) must have a final path.
 * If a file already exists at that path, great, we return fast.
 * If platform is not "native", copy it from the remote.
 * For native games, we'll report success but not do anything -- We don't know the URL of the git repo to clone, even if there is one.
 * "native" is meant to be a sort of "user will take care of it" fallback, just let it pass.
 * If we do take action, we log it to stderr.
 * In a fresh migration, this is probably the step where we'll spend 99% of our time.
 */
int ra_migrate_copy_rom_file_if_needed(struct ra_migrate_context *ctx,const struct db_game *localgame,const struct db_game *remotegame);

#endif
