/* DbService.js
 */
 
import { Comm } from "../Comm.js";

export class DbService {
  static getDependencies() {
    return [Comm];
  }
  constructor(comm) {
    this.comm = comm;
    
    // null, Promise, or Array, for suggestions.
    // We cache these and don't worry much about keeping fresh.
    this.authors = null;
    this.platforms = null;
    this.genres = null;
    this.lists = null;
    
    // SearchUi abuses this to preserve its path across tab navigations.
    this.recentSearchPath = null;
  }
  
  suggestAuthors() {
    if (!this.authors) {
      this.authors = this.comm.httpJson("GET", "/api/meta/author", { detail: "id" })
        .then(rsp => this.authors = rsp)
        .catch(() => { this.authors = null; return []; });
    } else if (this.authors instanceof Array) {
      return Promise.resolve(this.authors);
    }
    return this.authors;
  }
  
  suggestPlatforms() {
    if (!this.platforms) {
      this.platforms = this.comm.httpJson("GET", "/api/meta/platform", { detail: "id" })
        .then(rsp => this.platforms = rsp)
        .catch(() => { this.authors = null; return []; });
    } else if (this.platforms instanceof Array) {
      return Promise.resolve(this.platforms);
    }
    return this.platforms;
  }
  
  suggestGenres() {
    if (!this.genres) {
      this.genres = this.comm.httpJson("GET", "/api/meta/genre", { detail: "id" })
        .then(rsp => this.genres = rsp)
        .catch(() => { this.genres = null; return []; });
    } else if (this.genres instanceof Array) {
      return Promise.resolve(this.genres);
    }
    return this.genres;
  }
  
  suggestLists() {
    if (!this.lists) {
      this.lists = this.comm.httpJson("GET", "/api/list", { index: 0, count: 999999, detail: "id" })
        .then(rsp => this.lists = rsp.map(list => list.name || list.id))
        .catch(() => { this.lists = null; return []; });
    } else if (this.lists instanceof Array) {
      return Promise.resolve(this.lists);
    }
    return this.lists;
  }
  
  getTableNames() {
    return ["launcher", "list", "game", "comment", "play", "blob"];
  }
  
  /* Null if unknown, or an object where keys are the record key and values the type:
   *  "any": Complicated type. Consumer should pick these off specifically.
   *  "string"
   *  "integer"
   *  "boolean"
   *  "time": ISO 8601 timestamp, usually 1-minute resolution. Time zone never specified.
   *  "flags": Space-delimited list of flag names.
   *  "gameid","launcherid","listid": Foreign key (integer).
   *  "readonly": Primary key or otherwise immutable.
   */
  getTableSchema(tableName) {
    switch (tableName) {
      case "launcher": return {
          launcherid: "readonly",
          name: "string",
          platform: "string",
          suffixes: "string",
          cmd: "string",
          desc: "string",
        };
      case "list": return {
          listid: "readonly",
          name: "string",
          desc: "string",
          sorted: "boolean",
          games: "any",
        };
      case "game": return {
          // You can fetch games with related entites attached; those aren't mentioned here.
          // ie this is for "detail=record"
          gameid: "readonly",
          name: "string",
          platform: "string",
          author: "string",
          genre: "string",
          flags: "flags",
          rating: "integer",
          pubtime: "time",
          path: "string",
        };
      case "comment": return {
          gameid: "gameid",
          time: "time",
          k: "string",
          v: "string",
        };
      case "play": return {
          gameid: "gameid",
          time: "time",
          dur_m: "integer",
        };
      // No schema for blobs; they are just strings.
    }
    return null;
  }
  
  generateBlankRecord(schema) {
    if (!schema) return "";
    const record = {};
    for (const k of Object.keys(schema)) {
      let v = null;
      switch (schema[k]) {
        case "any": break;
        case "string": v = ""; break;
        case "integer": v = 0; break;
        case "boolean": v = false; break;
        case "time": v = "0000-00-00"; break;
        case "flags": v = ""; break;
        case "gameid": v = 0; break;
        case "launcherid": v = 0; break;
        case "listid": v = 0; break;
        case "readonly": v = 0; break;
      }
      record[k] = v;
    }
    return record;
  }
  
  /* Array of record keys which should be read-only in existing records.
   * This is not knowable from the schema alone.
   */
  listReadOnlyKeys(tableName) {
    switch (tableName) {
      case "launcher": return ["launcherid"];
      case "list": return ["listid"];
      case "game": return ["gameid"];
      case "comment": return ["gameid", "time", "k"];
      case "play": return ["gameid", "time"];
    }
    return [];
  }
  
  reprRecordForList(tableName, record) {
    switch (tableName) {
      case "launcher": return `${record.launcherid}: ${record.name}`;
      case "list": return `${record.listid}: ${record.name}`;
      case "game": return `${record.gameid}: ${record.name}`;
      case "comment": {
          if (record.v?.length < 100) return `${record.gameid}@${record.time} ${JSON.stringify(record.k)} = ${JSON.stringify(record.v)}`;
          return `${record.gameid}@${record.time} ${JSON.stringify(record.k)} (long content)`;
        }
      case "play": return `${record.gameid}@${record.time}: ${record.dur_m} min`;
      case "blob": return record;
    }
    return JSON.stringify(record);
  }
  
  extractRecordIdAsHttpQuery(tableName, record) {
    if (!record) return null;
    switch (tableName) {
      case "launcher": return record.launcherid ? { launcherid: record.launcherid } : null;
      case "list": return record.lsitid ? { listid: record.listid } : null;
      case "game": return record.gameid ? { gameid: record.gameid } : null;
      case "comment": return { gameid: record.gameid, time: record.time, k: record.k };
      case "play": return { gameid: record.gameid, time: record.time };
      case "blob": return { path: record };
    }
    return null;
  }
  
  /* Resolves to null if the table is not countable.
   * Rejects if table not defined.
   */
  fetchTableLength(tableName) {
    if (!tableName) return Promise.reject("invalid table");
    if (tableName === "blob") return Promise.resolve(null);
    return this.comm.httpJson("GET", `/api/${tableName}/count`);
  }
  
  fetchRecords(tableName) {
    let httpCall;
    if (!tableName) return Promise.reject("invalid table");
    if (tableName === "blob") {
      httpCall = this.comm.httpJson("GET", "/api/blob/all");
    } else {
      httpCall = this.comm.httpJson("GET", `/api/${tableName}`, { index: 0, count: 999999, detail: "record" });
    }
    return httpCall.then(rsp => {
      if (rsp instanceof Array) return rsp;
      throw new Error(`Expected array, fetching table '${tableName}'`);
    });
  }
  
  insertRecord(tableName, record) {
    if (tableName === "blob") {
      return Promise.reject("Can't update blobs like this. Edit directly in the filesystem instead.");
    }
    let query = null;
    if (tableName === "list") query = { detail: "id" };
    return this.comm.httpJson("PUT", `/api/${tableName}`, query, null, JSON.stringify(record));
  }
  
  updateRecord(tableName, record) {
    // Update is exactly the same thing as insert, except method PATCH instead of PUT.
    if (tableName === "blob") {
      return Promise.reject("Can't update blobs like this. Edit directly in the filesystem instead.");
    }
    let query = null;
    if (tableName === "list") query = { detail: "id" };
    return this.comm.httpJson("PATCH", `/api/${tableName}`, query, null, JSON.stringify(record));
  }
  
  deleteRecord(tableName, record) {
    const query = this.extractRecordIdAsHttpQuery(tableName, record);
    if (!query) return Promise.reject("Unable to determine record ID.");
    return this.comm.http("DELETE", `/api/${tableName}`, query);
  }
  
  query(params) {
    if (!this.sanityCheckQuery(params)) return Promise.resolve({ results: [], pageCount: 1 });
    return this.comm.http("POST", "/api/query", params, null, null, "response").then(rsp => {
      return rsp.json().then(results => ({ results, pageCount: +rsp.headers.get("X-Page-Count") || 1 }));
    });
  }
  
  /* An empty query matches the whole database.
   * Likewise, a text query for eg "A" will match pretty much everything, after conducting an expensive text search.
   * Require at least 3 characters of text, or at least one other criterion.
   * (If full-db-matching queries slip thru now and then, it's really not a big deal. Just don't want it happening a lot.)
   */
  sanityCheckQuery(params) {
    if (params.list) return true;
    if (params.platform) return true;
    if (params.author) return true;
    if (params.genre) return true;
    if (params.flags) return true;
    if (params.notflags) return true;
    if (params.rating) return true;
    if (params.pubtime) return true;
    if (params.text && (params.text.length >= 3)) return true;
    return false;
  }
  
  fetchGame(gameid, detail) {
    if (!detail) detail = "record";
    return this.comm.httpJson("GET", "/api/game", { gameid, detail });
  }
  
  patchGame(game, detail) {
    if (!detail) detail = "record";
    const { comments, plays, lists, blobs, ...header } = game; // strip some things we know the backend doesn't want.
    return this.comm.httpJson("PATCH", "/api/game", { detail }, null, JSON.stringify(header));
  }
  
  putComment(comment) {
    return this.comm.httpJson("PUT", "/api/comment", null, null, JSON.stringify(comment));
  }
  
  patchComment(comment) {
    return this.comm.httpJson("PATCH", "/api/comment", null, null, JSON.stringify(comment));
  }
  
  deleteComment(comment) {
    const query = { gameid: comment.gameid, time: comment.time, k: comment.k };
    return this.comm.http("DELETE", "/api/comment", query);
  }
  
  patchPlay(play) {
    return this.comm.httpJson("PATCH", "/api/play", null, null, JSON.stringify(play));
  }
  
  deletePlay(play) {
    const query = { gameid: play.gameid, time: play.time };
    return this.comm.http("DELETE", "/api/play", query);
  }
  
  addToList(gameid, listidOrName) {
    return this.comm.httpJson("POST", "/api/list/add", { gameid, listid: listidOrName, detail: "id" });
  }
  
  removeFromList(gameid, listid) {
    return this.comm.httpJson("POST", "/api/list/remove", { gameid, listid, detail: "id" });
  }
}

DbService.singleton = true;

/* These match the database, little-endianly.
 * Though they don't actually need to; when we treat them as numbers, it's only for encoding app state.
 */
DbService.FLAG_NAMES = [
  "player1",
  "player2",
  "player3",
  "player4",
  "playermore",
  "faulty",
  "foreign",
  "hack",
  "hardware",
  "review",
  "obscene",
  "favorite",
  "seeother",
];
