/* DbService.js
 */
 
import { Comm } from "../Comm.js";

export class DbService {
  static getDependencies() {
    return [Comm];
  }
  constructor(comm) {
    this.comm = comm;
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
}

DbService.singleton = true;
