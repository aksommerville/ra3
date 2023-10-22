#!/usr/bin/env node

/* populate-ra3.js
 * Designed to migrate a vast metadata collection from Romassist v2 to Romassist v3.
 * Run dump-ra2.sh first to gather the metadata.
 * The ra3 server should be running, and its database should be empty of games.
 */
 
const fs = require("fs");
const http = require("http");

function httpJson(method, url, query, headers, body) {
  return new Promise((resolve, reject) => {
    url = "http://localhost:6502" + url;
    if (query) url += "?" + Object.keys(query).map(k => encodeURIComponent(k) + "=" + encodeURIComponent(query[k])).join("&");
    if (!headers) headers = {};
    http.request(url, { method, headers }, (rsp) => {
      let rspBody = "";
      rsp.on("data", data => rspBody += data);
      rsp.on("end", () => {
        if (rsp.statusCode !== 200) {
          console.log(`${method} ${url} => ${rsp.statusCode} ${rsp.statusMessage}`);
          reject(rsp.statusMessage);
        } else {
          resolve(rspBody ? JSON.parse(rspBody) : "");
        }
      });
      rsp.on("error", error => reject(error));
    }).end(body);
  });
}
 
const odbGames = JSON.parse(fs.readFileSync("game.json"));
console.log(`game.json: Found ${odbGames.length} games`);

function guessNameFromPath(path) {
  const stem = path.replace(/^.*\/([^.]*).*$/, "$1");
  const pad = "                                                     ";
  path = pad.substring(path.length) + path;
  const name = stem.split(/[\s_-]+/).filter(v => v).map((word, i) => {
    if (i) switch (word) { // conjunctions, articles, and certain prepositions, beyond the first word, stay lower.
      // English is a stupid fucking language.
      case "a":
      case "an":
      case "the":
      case "in":
      case "on":
      case "of":
      case "to":
      case "and":
      case "or":
        return word;
    }
    switch (word) { // If it looks like a roman numeral, go upper. These are pretty common in titles, and "Ikari Warriors Iii" looks super weird.
      case "ii": return "II";
      case "iii": return "III";
      case "iv": return "IV";
      case "v": return "V";
      case "vi": return "VI";
      case "VII": return "VII";
      case "viii": return "VIII";
      case "ix": return "IX";
      // Similar treatment for some known sports leagues, these come up a lot.
      case "nhl": return "NHL";
      case "nhlpa": return "NHLPA";
      case "nba": return "NBA";
      case "mlb": return "MLB";
      case "mlbpa": return "MLBPA";
      case "nfl": return "NFL";
      case "ncaa": return "NCAA";
      case "fifa": return "FIFA";
      case "wwf": return "WWF";
      case "wcw": return "WCW";
      case "pga": return "PGA";
      case "rbi": return "RBI"; // ok not a "league" but same treatment
    }
    return word[0].toUpperCase() + word.substring(1).toLowerCase();
  }).join(" ");
  return name;
}

function ra3GameFromRa2(ra2, inferredPlatform) {
  // I didn't change much in the game record, going from v2 to v3. Almost pass-thru.
  // We do capture comments here. Beware that they don't get inserted when we insert the game. Will have to make a second pass for comments.
  const ra3 = {
    gameid: ra2.id || 0,
    name: ra2.name || guessNameFromPath(ra2.path),
    path: ra2.path,
    flags: ra2.flags || "",
    rating: ra2.rating || "",
    pubtime: ra2.pubtime || "",
    platform: ra2.platform || inferredPlatform || "",
    author: ra2.author || "",
    genre: ra2.genre || "",
  };
  if (ra2.comments && ra2.comments.length) {
    ra3.comments = ra2.comments.map(cmt => ({
      time: cmt.time,
      k: "text",
      v: cmt.text,
    }));
    if (ra2.desc) ra3.comments.push({
      k: "text",
      v: ra2.desc,
    });
  } else if (ra2.desc) {
    ra3.comments = [{
      k: "text",
      v: ra2.desc,
    }];
  }
  return ra3;
}

function ra3GameFromPath(path, platform) {
  return {
    gameid: 0,
    name: guessNameFromPath(path),
    platform,
    path,
  };
}

/* Returns a partial ra3 Game if this file should be added.
 */
function considerFile(path, platform) {

  // This is by no means mandatory, but I happen to know that all of my games will acquire (platform) along the way.
  if (!platform) return null;
  
  // If it exists in the ra2 database, that should be all we need to know. Keep it, ID and everything.
  const existing = odbGames.find(g => g.path === path);
  if (existing) {
    //console.log(`${path} exists, id ${existing.id}`);
    return ra3GameFromRa2(existing, platform);
  }
  
  // If it's not already listed, verify that the path looks kosher for platform, make up a name, and keep it.
  if (path.endsWith(".sav")) return null; // i ended up with tons of these, oops
  let keep = false;
  switch (platform) {
    case "scv": keep = path.endsWith(".bin"); break;
    case "nes": keep = path.endsWith(".nes"); break;
    case "n64": keep = path.endsWith(".z64"); break;
    case "genesis": keep = path.endsWith(".gen"); break;
    case "gameboy": keep = path.endsWith(".gbc") || path.endsWith(".gb"); break;
    case "c64": keep = path.endsWith(".d64.gz"); break;
    case "atari7800": keep = path.endsWith(".a78") || path.endsWith(".A78") || path.endsWith(".bin"); break;
    case "atari5200": keep = path.endsWith(".bin") || path.endsWith(".BIN") || path.endsWith(".a52"); break;
    case "atari2600": keep = path.endsWith(".bin"); break;
  }
  if (keep) return ra3GameFromPath(path, platform);
  return null;
}

function isPlatformName(name) {
  // We could complicatedly ask ra3 for launchers and compare (name) to (platform) in those, but it's easier to hard-code them.
  // Also this way, the launchers don't actually need to exist yet.
  switch (name) {
    case "atari2600":
    case "atari5200":
    case "atari7800":
    case "c64":
    case "gameboy":
    case "genesis":
    case "n64":
    case "nes":
    case "pico8":
    case "scv":
    case "snes":
      return true;
  }
  return false;
}

/* Returns synchronously: {
 *   filesToAdd: { ...v3 Game, always includes "path" and that might be all }[]
 *   totalFileCount: number
 * }
 */
function walkDirectory(dir, platform, results) {
  if (!results) results = {
    filesToAdd: [],
    totalFileCount: 0,
  };
  for (const base of fs.readdirSync(dir)) {
    const path = dir + "/" + base;
    const st = fs.statSync(path);
    
    if (st.isFile()) {
      results.totalFileCount++;
      const file = considerFile(path, platform);
      if (file) {
        results.filesToAdd.push(file);
      }
      
    } else if (st.isDirectory()) {
      if (base === "gb") walkDirectory(path, "gameboy", results); // oops i really shouldn't have called the directory "gb"
      else if (isPlatformName(base)) walkDirectory(path, base, results);
      else walkDirectory(path, platform, results);
    }
  }
  return results;
}

/* Review ra2 game set and add files that exist but weren't found in the fs walk.
 * This will mostly be native games.
 */
function addResultsNotFoundInWalk(results) {
  for (const ra2 of odbGames) {
    if (ra2.path.startsWith("/home/andy/rom/")) continue;
    if (results.filesToAdd.find(g => g.gameid === ra2.id)) continue;
    if (!fs.existsSync(ra2.path)) continue;
    results.filesToAdd.push(ra3GameFromRa2(ra2));
  }
}

function deliverCommentsToRa3_next(comments) {
  if (!comments.length) return Promise.resolve();
  const comment = comments[0];
  comments.splice(0, 1);
  //console.log(`+ PUT /api/comment ${comment.v}`);
  return httpJson("PUT", "/api/comment", null, null, JSON.stringify(comment))
    .then(() => deliverCommentsToRa3_next(comments));
}

function deliverCommentsToRa3(comments) {
  return deliverCommentsToRa3_next(comments); // no need to copy; we know the caller did (see below)
}

function deliverGamesToRa3_next(addGames) {
  if (!addGames.length) return Promise.resolve();
  const game = addGames[0];
  addGames.splice(0, 1);
  let logMe = false;
  if ((game.gameid === 1830) || (game.path === "/home/andy/rom/snes/s/street_racer.smc") || (game.path === "/home/andy/rom/atari2600/c/crypts_of_chaos.bin")) {
    console.log(`PUT /api/game ${game.path} gameid=${game.gameid}`);
    logMe = true;
  }
  const serial = JSON.stringify(game);
  return httpJson("PUT", "/api/game", { detail: "id" }, null, serial).then(gameid => {
    if (logMe || (gameid === 1830)) {
      console.log(`...saved as gameid ${gameid}`);
    }
    if (game.gameid && (gameid !== game.gameid)) {
      console.log(`!!! Tried to save gameid ${game.gameid} and got '${gameid}' back !!! (${game.path})`);
      process.exit(1);
    }
    if (game.comments) {
      return deliverCommentsToRa3(game.comments.map(c => ({ ...c, gameid })));
    }
  }).then(() => deliverGamesToRa3_next(addGames))
  .catch(e => {
    console.log(`ERROR SAVING GAME!`, e);
    console.log(`Offending request body: ${serial}`);
    process.exit(1);
  });
}

function deliverGamesToRa3(results) {
  const addGames = [...results.filesToAdd];
  return deliverGamesToRa3_next(addGames);
}

function validateGamesBeforeSending(games) {
  let fatal = false;
  for (const game of games) {
    if (game.name.length > 32) {
      const newName = game.name.substring(0, 32).trim();
      console.log(`!!! Name too long, truncating: ${JSON.stringify(game.name)} => ${JSON.stringify(newName)}`);
      game.name = newName;
    }
    const components = game.path.split('/');
    const base = components[components.length - 1];
    if (base.length > 32) {
      console.log(`!!! FATAL !!! Base name too long. Please rename: ${game.path}`);
      fatal = true;
    }
  }
  if (fatal) throw new Error(`Invalid games.`);
}

httpJson("GET", "/api/game/count").then(c => {
  if (c) throw new Error(`ra3 db contains ${c} games. Must be empty for us to run.`);
  console.log("ra3 db empty, good");
  const results = walkDirectory("/home/andy/rom");
  addResultsNotFoundInWalk(results);
  results.filesToAdd.sort((a, b) => { // Put the zero IDs last to ensure they don't acquire an ID that somebody else wants.
    if (a.gameid && b.gameid) return a.gameid - b.gameid;
    if (a.gameid) return -1;
    if (b.gameid) return 1;
    return 0;
  });
  validateGamesBeforeSending(results.filesToAdd);
  console.log(`Ready to add ${results.filesToAdd.length} of ${results.totalFileCount} files.`);
  return deliverGamesToRa3(results);
}).then(() => {
  console.log(`...added, you're all set!`);
}).catch(e => {
  console.error(e);
  process.exit(1);
});
