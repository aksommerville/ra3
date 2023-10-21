/* DbRecordModal.js
 */
 
import { Dom } from "../Dom.js";
import { DbService } from "../model/DbService.js";

export class DbRecordModal {
  static getDependencies() {
    return [HTMLElement, Dom, DbService];
  }
  constructor(element, dom, dbService) {
    this.element = element;
    this.dom = dom;
    this.dbService = dbService;
    
    this.onSave = (record) => Promise.reject("Save not implemented.");
    this.onDelete = (record) => Promise.reject("Delete not implemented.");
    
    this.tableName = "";
    this.schema = null;
    this.record = null;
    this.alwaysReadOnlyKeys = [];
    this.existingRecord = false;
    
    this.buildUi();
  }
  
  setupNew(tableName, schema) {
    this.tableName = tableName;
    this.schema = schema;
    this.record = this.dbService.generateBlankRecord(this.schema);
    this.alwaysReadOnlyKeys = []; // New records, everything is mutable, including primary keys.
    this.existingRecord = false;
    this.rebuildTable();
  }
  
  setupEdit(tableName, schema, record) {
    this.tableName = tableName;
    this.schema = schema;
    this.record = record;
    this.alwaysReadOnlyKeys = this.dbService.listReadOnlyKeys(this.tableName);
    this.existingRecord = true;
    this.rebuildTable();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", ["errorMessage", "hidden"], { "on-click": () => this.dismissError() });
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": e => this.onSubmit(e) });
    this.dom.spawn(form, "TABLE");
    const buttonsRow = this.dom.spawn(form, "DIV", ["buttonsRow"]);
    this.dom.spawn(buttonsRow, "INPUT", { type: "button", value: "Delete", "on-click": () => this.onDeleteButton() });
    this.dom.spawn(buttonsRow, "INPUT", { type: "submit", value: "Save" });
  }
  
  rebuildTable() {
    const table = this.element.querySelector("form > table");
    table.innerHTML = "";
    let focusTarget = null;
    
    // Blobs are special; they aren't objects and don't have a schema.
    if (this.tableName === "blob") {
      focusTarget = this.addTableRow(table, "path", "string", this.record || "");
      
    // Everything else we can build generically.
    } else if (this.schema) {
      for (const k of Object.keys(this.schema)) {
        let v = this.record[k];
        
        if ((this.tableName === "list") && (k === "games")) {
          v = this.reprListGames(v);
        }
        
        const input = this.addTableRow(table, k, this.schema[k], v);
        if (!focusTarget && !input.disabled) {
          focusTarget = input;
        }
      }
    }
    
    if (focusTarget) {
      focusTarget.select(0, 99999);
      focusTarget.focus();
    }
    
    // Delete button should be enabled for existing records only.
    this.element.querySelector("input[value='Delete']").disabled = !this.existingRecord;
  }
  
  addTableRow(table, k, t, v) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["key"], k);
    const tdv = this.dom.spawn(tr, "TD", ["value"]);
    const input = this.dom.spawn(tdv, "INPUT", {
      type: "text",//TODO
      name: k,
      value: v,
    });
    if (this.alwaysReadOnlyKeys.includes(k)) {
      input.disabled = true;
    }
    return input;
  }
  
  setMutable(mut) {
    if (mut) {
      // Beware, some fields should be readonly no matter what.
      for (const input of this.element.querySelectorAll("input")) {
        if (!this.alwaysReadOnlyKeys.includes(input.name)) {
          input.disabled = false;
        }
      }
    } else {
      for (const input of this.element.querySelectorAll("input")) {
        input.disabled = true;
      }
    }
  }
  
  readRecordFromUi() {
    if (this.tableName === "blob") {
      return this.element.querySelector("input[name='path']").value || "";
    }
    const record = {};
    for (const input of this.element.querySelectorAll("input")) {
    
      if (input.type === "button") continue;
      if (input.type === "submit") continue;
      
      if ((this.tableName === "list") && (input.name === "games")) {
        record[input.name] = this.evalListGames(input.value);
        continue;
      }
      
      record[input.name] = input.value;
    }
    return record;
  }
  
  // list.games displays as whitespace-delimited IDs, but in the live object it's an array of ID or record.
  // Cases like these where we need per-field eval/repr should be very rare, I think this is the only one.
  reprListGames(games) {
    if (!games || !games.length) return "";
    if (typeof(games[0]) === "object") games = games.map(g => g.gameid);
    return games.join(" ");
  }
  evalListGames(src) {
    return src.split(/[\s,]+/g).map(v => +v).filter(v => v);
  }
  
  displayError(e) {
    if (e?.message) e = e.message;
    if (typeof(e) !== "string") e = JSON.stringify(e);
    const errorMessage = this.element.querySelector(".errorMessage");
    errorMessage.innerText = e;
    errorMessage.classList.remove("hidden");
  }
  
  dismissError(e) {
    this.element.querySelector(".errorMessage").classList.add("hidden");
  }
  
  onSubmit(event) {
    event.preventDefault();
    this.setMutable(false);
    this.dismissError();
    this.onSave(this.readRecordFromUi()).then(() => {
      this.dom.dismissModalByController(this);
    }).catch(e => {
      this.displayError(e);
      this.setMutable(true);
    });
  }
  
  onDeleteButton() {
    if (!this.existingRecord) return;
    this.setMutable(false);
    this.dismissError();
    this.onDelete(this.record).then(() => {
      this.dom.dismissModalByController(this);
    }).catch(e => {
      this.displayError(e);
      this.setMutable(true);
    });
  }
}
