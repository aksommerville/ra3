#include "ra_internal.h"
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"

/* Cleanup.
 */
 
void ra_game_upload_cleanup(struct ra_game_upload *upload) {
  if (upload->path) free(upload->path);
}

/* Truncate (src) so it fits in (dst), trying to preserve anything after a dot.
 * "jack_nicklaus_18_holes_of_professional_championship_golf.smc" should produce something like "jack_nicklaus_18_holes_o.smc".
 * "zelda.NotSureWhyThisIntermediatePortionWouldExist.nes" should produce exactly "zelda.nes".
 */
 
static int ra_game_upload_truncate_basename(char *dst,int dsta,const char *src,int srcc) {
  if (dsta<1) return -1;
  int i=0,pfxc=0,pfxfinal=0,lastdotp=-1;
  for (;i<srcc;i++) {
    if (src[i]=='.') {
      int sfxc=srcc-i; // including this dot
      if (pfxc+sfxc<=dsta) {
        memcpy(dst,src,pfxc);
        memcpy(dst+pfxc,src+i,sfxc);
        return pfxc+sfxc;
      }
      pfxfinal=1;
      lastdotp=i;
    } else if (!pfxfinal) {
      pfxc++;
    }
  }
  if (lastdotp>=0) { // try to keep the last suffix but truncate the prefix
    int sfxc=srcc-lastdotp; // including the dot
    if (sfxc<dsta) { // sic '<' not '<=', ensure at least one character before the dot
      int truncpfxc=dsta-sfxc;
      if (truncpfxc>pfxc) truncpfxc=pfxc;
      memcpy(dst,src,truncpfxc);
      memcpy(dst+truncpfxc,src+lastdotp,sfxc);
      return truncpfxc+sfxc;
    }
  }
  // ok let's play hardball
  memcpy(dst,src,dsta);
  return dsta;
}

/* Compose local path.
 * Use platform (required), and base if present.
 * The loose platform text and stringid must be the same thing; we'll use the loose text.
 * Make up an ugly basename if needed.
 * On success, (dst) is written and terminated.
 */
 
static int ra_game_upload_compose_path(struct sr_encoder *dst,struct ra_game_upload *upload) {

  // Take basename from the request, or make up something ugly.
  // We also enforce db's basename length limit here, since it's convenient to do so.
  char base[DB_GAME_BASE_LIMIT];
  int basec=0;
  if (upload->basec>sizeof(base)) {
    basec=ra_game_upload_truncate_basename(base,sizeof(base),upload->base,upload->basec);
  } else if (upload->basec>0) {
    memcpy(base,upload->base,upload->basec);
    basec=upload->basec;
  } else {
    basec=db_compose_basename_for_anonymous_game(base,sizeof(base),ra.db);
  }
  if ((basec<1)||(basec>sizeof(base))) return -1;
  
  // Start with the user's home directory.
  const char *src=getenv("HOME");
  if (src&&src[0]) {
    if (sr_encode_raw(dst,src,-1)<0) return -1;
  } else if ((src=getenv("USER"))&&src[0]) {
    if (sr_encode_fmt(dst,"/home/%s",src)<0) return -1;
  } else {
    return -1;
  }
  if (dst->c<1) return -1;
  if (dst->v[dst->c-1]!='/') {
    if (sr_encode_u8(dst,'/')<0) return -1;
  }
  
  // Then "rom/PLATFORM/"
  if (sr_encode_fmt(dst,"rom/%.*s/",upload->platformc,upload->platform)<0) return -1;
  
  // Then either the lowercase first letter of the basename, or "09".
  if ((base[0]>='a')&&(base[0]<='z')) {
    if (sr_encode_u8(dst,base[0])<0) return -1;
  } else if ((base[0]>='A')&&(base[0]<='Z')) {
    if (sr_encode_u8(dst,base[0]+0x20)<0) return -1;
  } else {
    if (sr_encode_raw(dst,"09",2)<0) return -1;
  }
  if (sr_encode_u8(dst,'/')<0) return -1;
  
  // And finally the basename and terminator.
  if (sr_encode_raw(dst,base,basec)<0) return -1;
  if (sr_encoder_terminate(dst)<0) return -1;
  return 0;
}

/* Compose a display name for the given basename.
 */
 
// (midline) nonzero if this is not the first token -- minor words may be all lowercase.
static void ra_game_upload_mangle_display_token_case(char *token,int tokenc,int midline) {
  int i;
  if (tokenc<1) return;
  
  // First, force lowercase.
  for (i=tokenc;i-->0;) if ((token[i]>='A')&&(token[i]<='Z')) token[i]+=0x20;
  
  // Next, look for known words that should be all uppercase. Opinionated.
  // If it matches, do it and we're done.
  int forceupper=0;
  switch (tokenc) {
    case 2: {
             if (!memcmp(token,"ea",2)) forceupper=1;
        else if (!memcmp(token,"eu",2)) forceupper=1;
        else if (!memcmp(token,"un",2)) forceupper=1;
        else if (!memcmp(token,"uk",2)) forceupper=1;
        // Ambiguous, don't touch: "us"
      } break;
    case 3: {
             if (!memcmp(token,"usa",3)) forceupper=1;
        else if (!memcmp(token,"mlb",3)) forceupper=1;
        else if (!memcmp(token,"nhl",3)) forceupper=1;
        else if (!memcmp(token,"nba",3)) forceupper=1;
        else if (!memcmp(token,"nfl",3)) forceupper=1;
        else if (!memcmp(token,"pga",3)) forceupper=1;
        else if (!memcmp(token,"nes",3)) forceupper=1;
        else if (!memcmp(token,"wwf",3)) forceupper=1;
        else if (!memcmp(token,"wwe",3)) forceupper=1;
        else if (!memcmp(token,"wcw",3)) forceupper=1;
        else if (!memcmp(token,"abc",3)) forceupper=1;
      } break;
    case 4: {
             if (!memcmp(token,"fifa",4)) forceupper=1;
        else if (!memcmp(token,"ncaa",4)) forceupper=1;
        else if (!memcmp(token,"snes",4)) forceupper=1;
      } break;
    case 5: {
             if (!memcmp(token,"mlbpa",5)) forceupper=1;
        else if (!memcmp(token,"nhlpa",5)) forceupper=1;
      } break;
  }
  if (forceupper) {
    for (i=tokenc;i-->0;) if ((token[i]>='a')&&(token[i]<='z')) token[i]-=0x20;
    return;
  }
  
  // If we're mid-line, look for known words that should *not* uppercase the first letter.
  if (midline) {
    switch (tokenc) {
      case 1: switch (token[0]) {
          case 'a':
            return;
        } break;
      case 2: {
               if (!memcmp(token,"le",2)) return;
          else if (!memcmp(token,"la",2)) return;
          else if (!memcmp(token,"an",2)) return;
          else if (!memcmp(token,"in",2)) return;
          else if (!memcmp(token,"on",2)) return;
          else if (!memcmp(token,"of",2)) return;
          else if (!memcmp(token,"or",2)) return;
          else if (!memcmp(token,"vs",2)) return;
        } break;
      case 3: {
               if (!memcmp(token,"the",3)) return;
          else if (!memcmp(token,"and",3)) return;
        } break;
    }
  }
  
  // Capitalize first letter.
  if ((tokenc>=1)&&(token[0]>='a')&&(token[0]<='z')) token[0]-=0x20;
}
 
static int ra_game_upload_prettify_name(char *dst/*DB_GAME_NAME_LIMIT*/,const char *src,int srcc) {
  memset(dst,0,DB_GAME_NAME_LIMIT);
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  /* Discard everything after the first dot.
   * Apologies to "Dr. Mario.nes" but hey file names are not display names, take better care of your files!
   */
  int i=0; for (;i<srcc;i++) if (src[i]=='.') srcc=i;
  
  /* Copy tokens.
   * Input tokens are separated by anything outside [a-zA-Z0-9], and also break at letter/digit junctions.
   * Output tokens, first letter is forced upper and the rest lower, and a single space separates them.
   * Pick off a few known tokens that should be all upper-case too.
   * And after the first output token, force some conjunctions, articles, and prepositions lowercase.
   * English is annoying.
   * If the output runs longer than DB_GAME_NAME_LIMIT, (a) what the fuck are you doing to your file names, and (b) truncate.
   */
  int dstc=0,srcp=0;
  while (srcp<srcc) {
  
    const char *srctoken=src+srcp;
    int srctokenc=0;
    if (((src[srcp]>='a')&&(src[srcp]<='z'))||((src[srcp]>='A')&&(src[srcp]<='Z'))) {
      srctokenc++;
      srcp++;
      while (srcp<srcc) {
        if ((src[srcp]>='a')&&(src[srcp]<='z')) { srcp++; srctokenc++; continue; }
        if ((src[srcp]>='A')&&(src[srcp]<='Z')) { srcp++; srctokenc++; continue; }
        break;
      }
    } else if ((src[srcp]>='0')&&(src[srcp]<='9')) {
      srctokenc++;
      srcp++;
      while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) { srcp++; srctokenc++; }
    } else {
      srcp++;
      continue;
    }
    
    if (dstc) {
      if (dstc>=DB_GAME_NAME_LIMIT) return dstc;
      dst[dstc++]=' ';
    }
    
    // No matter what kind of mangling we do to the token, its length holds constant.
    // Truncate the source token if output will overrun.
    if (dstc>DB_GAME_NAME_LIMIT-srctokenc) srctokenc=DB_GAME_NAME_LIMIT-dstc;
    
    // Copy that token, then prettify it.
    memcpy(dst+dstc,srctoken,srctokenc);
    ra_game_upload_mangle_display_token_case(dst+dstc,srctokenc,dstc);
    dstc+=srctokenc;
    
    // Reached the end of output? No sense proceeding, take what we got.
    if (dstc>=DB_GAME_NAME_LIMIT) return dstc;
  }
  // No need to terminate. And if it's empty, that's fine too.
  return dstc;
}

/* Guess platform from suffix.
 * This gets called one unit at a time, back to front.
 * eg for "Dr. Mario.nes.gz" we'll get: "gz", "nes", " Mario".
 * On a success, we populate (upload->platform[c]) and return >=0.
 */
 
static int ra_game_upload_platform_from_suffix(struct ra_game_upload *upload,const char *src,int srcc) {
  char norm[16];
  if ((srcc<1)||(srcc>sizeof(norm))) return -1;
  int i=srcc; while (i-->0) {
    if ((src[i]>='A')&&(src[i]<='Z')) norm[i]=src[i]+0x20;
    else norm[i]=src[i];
  }
  #define CHK(sfx,plat) if (!memcmp(norm,sfx,srcc)) { upload->platform=plat; upload->platformc=sizeof(plat)-1; return 0; }
  switch (srcc) {
    case 2: {
        CHK("p8","pico8")
        CHK("gb","gameboy")
      } break;
    case 3: {
        CHK("nes","nes")
        CHK("smc","snes")
        CHK("a26","atari2600")
        CHK("a52","atari5200")
        CHK("a78","atari7800")
        CHK("d64","c64")
        CHK("gbc","gameboy")
        CHK("32x","gen32x")
        CHK("gen","genesis")
        CHK("z64","n64")
      } break;
  }
  #undef OK
  // We could review the launcher table too; it has lists of suffixes. But that seems like overkill.
  return -1;
}

/* Prepare.
 */
 
int ra_game_upload_prepare(struct ra_game_upload *upload) {

  /* No length is allowed to be negative.
   * (serial) is allowed to be null with a positive length, the others not.
   */
  if (upload->basec<0) return -1;
  if (upload->platformc<0) return -1;
  if (upload->serialc<0) return -1;
  if (upload->basec&&!upload->base) return -1;
  if (upload->platformc&&!upload->platform) return -1;
  
  /* Guess platform if not provided.
   * Prefer to guess from known suffixes of the basename.
   * If that fails and serial was provided, look for file signatures.
   * If we can't guess, fail. Platform is required to assemble the path.
   */
  if (!upload->platformc) {
    int sfxp=upload->basec,sfxc=0;
    while (sfxp>0) {
      if (upload->base[sfxp-1]=='.') {
        if (ra_game_upload_platform_from_suffix(upload,upload->base+sfxp,sfxc)>=0) break;
        sfxp--;
        sfxc=0;
      } else {
        sfxp--;
        sfxc++;
      }
    }
    if (!upload->platformc&&upload->serial) {
      if ((upload->serialc>=8)&&!memcmp(upload->serial,"\x89PNG\r\n\x1a\n",8)) {
        // PNG = pico8. You'd expect some Pico-8-specific chunk to more firmly identify it, but there actually isn't one. Weird.
        upload->platform="pico8";
        upload->platformc=5;
      } else if ((upload->serialc>=4)&&!memcmp(upload->serial,"NES\x1a",4)) {
        upload->platform="nes";
        upload->platformc=3;
      } else if ((upload->serialc>=10)&&!memcmp(upload->serial,"\1ATARI7800",10)) {
        upload->platform="atari7800";
        upload->platformc=9;
      }
      // snes, atari2600, gameboy: No signature I can see.
    }
    if (!upload->platformc) return -1;
  }
  if (!(upload->platform_stringid=db_string_intern(ra.db,upload->platform,upload->platformc))) return -1;
  
  /* If basename was provided, make up a name.
   * Name isn't mandatory. If we're going to make up a basename, don't bother with this display name.
   */
  if (upload->basec) {
    if (ra_game_upload_prettify_name(upload->name,upload->base,upload->basec)<0) return -1;
  }
  
  /* Make up a local path.
   * This is the opinionated part, and it's very specific to my installations.
   * It's also the most critical part. Where are we going to write this thing to?
   */
  struct sr_encoder path={0};
  if (ra_game_upload_compose_path(&path,upload)<0) {
    sr_encoder_cleanup(&path);
    return -1;
  }
  if (upload->path) free(upload->path);
  upload->path=path.v; // HANDOFF
  upload->pathc=path.c;
  
  return 0;
}

/* Commit.
 */
 
int ra_game_upload_commit(struct ra_game_upload *upload) {

  /* Path and serial must both exist.
   * I guess it would be prudent to validate the path somehow,
   * but in our case we know that we are the one that generated it.
   * (and what would validation mean, anyway?)
   * Confirm that (path) is NUL-terminated, possibly segfaulting in the process.
   */
  if (!upload->serial||(upload->serialc<1)) return -1;
  if (!upload->path||(upload->pathc<1)) return -1;
  if (upload->path[upload->pathc]) return -1;
  
  /* If the file already exists, it must match the upload byte for byte.
   * If it doesn't exist, write it.
   */
  char ftype=file_get_type(upload->path);
  if (!ftype) {
    if (dir_mkdirp_parent(upload->path)<0) return -1;
    if (file_write(upload->path,upload->serial,upload->serialc)<0) return -1;
  } else if (ftype=='f') {
    void *trial=0;
    int trialc=file_read(&trial,upload->path);
    if ((trialc!=upload->serialc)||memcmp(trial,upload->serial,trialc)) {
      fprintf(stderr,"%s: This file already exists and does not match the %d bytes being uploaded. Aborting upload.\n",upload->path,upload->serialc);
      if (trial) free(trial);
      return -1;
    }
  } else {
    fprintf(stderr,"%s: Unexpected file type '%c' at path we intended to upload to.\n",upload->path,ftype);
    return -1;
  }
  
  /* Add to db.
   */
  struct db_game gamereq={
    .platform=upload->platform_stringid,
  };
  memcpy(gamereq.name,upload->name,sizeof(upload->name));
  if (db_game_set_path(ra.db,&gamereq,upload->path,upload->pathc)<0) return -1;
  struct db_game *game=db_game_insert(ra.db,&gamereq);
  if (!game) return -1;
  upload->gameid=game->gameid;
  
  return 0;
}
