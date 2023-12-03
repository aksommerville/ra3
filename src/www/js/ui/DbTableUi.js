/* DbTableUi.js
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";
import { DbService } from "../model/DbService.js";
import { DbRecordModal } from "./DbRecordModal.js";
import { UploadModal } from "./UploadModal.js";

export class DbTableUi {
  static getDependencies() {
    return [HTMLElement, Dom, Comm, Window, DbService];
  }
  constructor(element, dom, comm, window, dbService) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    this.window = window;
    this.dbService = dbService;
    
    this.PAGE_SIZE = 100;
    
    this.name = "";
    this.schema = null;
    this.records = [];
    this.recordsPaged = [];
    this.pagep = 1; // one-based, as we display to user
    this.pagec = 1;
    
    this.buildUi();
  }
  
  setTable(name) {
    this.name = name;
    this.schema = this.dbService.getTableSchema(this.name);
    this.element.querySelector(".name").innerText = this.name;
    if (this.tableSupportsUpload(name)) this.element.querySelector(".upload").classList.remove("hidden");
    else this.element.querySelector(".upload").classList.add("hidden");
    this.refreshCount();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const card = this.dom.spawn(this.element, "DIV", ["card"]);
    const header = this.dom.spawn(card, "DIV", ["header"]);
    this.dom.spawn(header, "H2", ["name"]);
    this.dom.spawn(header, "DIV", ["spacer", "pager"]);
    this.dom.spawn(header, "INPUT", ["upload", "hidden"], { type: "button", value: "Upload", "on-click": () => this.onUpload() });
    this.dom.spawn(header, "INPUT", ["new"], { type: "button", value: "New", "on-click": () => this.onNew() });
    this.dom.spawn(header, "INPUT", ["openToggle"], { type: "button", value: "Open", "on-click": () => this.toggleOpen() });
    this.dom.spawn(card, "DIV", ["togglePanel", "hidden"]);
  }
  
  rebuildRecordList() {
  
    this.pagec = Math.ceil(this.records.length / this.PAGE_SIZE);
    if (this.pagep > this.pagec) this.pagep = this.pagec;
    const pager = this.element.querySelector(".pager");
    pager.innerHTML = "";
    if (this.pagec <= 1) {
      this.dom.spawn(pager, "DIV", `Showing all ${this.records.length} records.`);
      this.recordsPaged = this.records;
    } else {
      this.dom.spawn(pager, "INPUT", { type: "button", value: "<", "on-click": () => this.changePage(-1) });
      this.dom.spawn(pager, "DIV", ["text"], `Page ${this.pagep} of ${this.pagec}`);
      this.dom.spawn(pager, "INPUT", { type: "button", value: ">", "on-click": () => this.changePage(1) });
      this.recordsPaged = this.records.slice((this.pagep - 1) * this.PAGE_SIZE, this.pagep * this.PAGE_SIZE);
    }
  
    const panel = this.element.querySelector(".togglePanel");
    panel.innerHTML = "";
    for (const record of this.recordsPaged) {
      const text = this.dbService.reprRecordForList(this.name, record);
      this.dom.spawn(panel, "PRE", ["record"], text, { "on-click": () => this.onEdit(record) });
    }
  }
  
  changePage(d) {
    const np = this.pagep + d;
    if (np < 1) this.pagep = 1;
    else if (np > this.pagec) this.pagep = this.pagec;
    else this.pagep = np;
    this.rebuildRecordList();
  }
  
  toggleOpen() {
    const button = this.element.querySelector("input.openToggle");
    if (button.value === "Open") {
      button.value = "Close";
      this.element.querySelector(".togglePanel").classList.remove("hidden");
      this.refreshRecords();
    } else {
      button.value = "Open";
      this.element.querySelector(".togglePanel").classList.add("hidden");
    }
  }
  
  forceOpen(open) {
    const button = this.element.querySelector("input.openToggle");
    if (button.value === "Open") {
      if (!open) return;
      button.value = "Close";
      this.element.querySelector(".togglePanel").classList.remove("hidden");
      this.refreshRecords();
    } else {
      if (open) return;
      button.value = "Open";
      this.element.querySelector(".togglePanel").classList.add("hidden");
    }
  }
  
  tableSupportsUpload(tableName) {
    switch (tableName) {
      case "game":
      case "blob":
        return true;
    }
    return false;
  }
  
  onUpload() {
    const modal = this.dom.spawnModal(UploadModal);
    switch (this.name) {
      case "game": modal.setupGame(); break;
      case "blob": modal.setupBlob(); break;
      default: this.dom.dismissModalByController(modal); return;
    }
    modal.onSaved = () => {
      this.refreshCount();
      this.refreshRecordsIfVisible();
    };
  }
  
  onNew() {
    this.forceOpen(true);
    const modal = this.dom.spawnModal(DbRecordModal);
    modal.setupNew(this.name, this.schema);
    modal.onSave = record => this.insertRecord(record);
    modal.onDelete = record => this.deleteRecord(record);
  }
  
  onEdit(record) {
    const modal = this.dom.spawnModal(DbRecordModal);
    modal.setupEdit(this.name, this.schema, record);
    modal.onSave = record => this.updateRecord(record);
    modal.onDelete = record => this.deleteRecord(record);
  }
  
  refreshCount() {
    return this.dbService.fetchTableLength(this.name).then(c => {
      if (typeof(c) === "number") {
        this.element.querySelector(".name").innerText = `${this.name} (${c})`;
      }
    }).catch(e => {
      this.window.console.error(`Error fetching ${this.name} count.`, e);
    });
  }
  
  refreshRecords() {
    return this.dbService.fetchRecords(this.name).then(rv => {
      this.records = rv;
      this.rebuildRecordList();
    }).catch(e => {
      this.window.console.error(`DbTableUi ${this.name} error`, e);
    });
  }
  
  refreshRecordsIfVisible() {
    const togglePanel = this.element.querySelector(".togglePanel");
    if (togglePanel.classList.contains("hidden")) return;
    this.refreshRecords();
  }
  
  insertRecord(record) {
    return this.dbService.insertRecord(this.name, record)
      .then(() => this.refreshCount())
      .then(() => this.refreshRecords());
  }
  
  updateRecord(record) {
    return this.dbService.updateRecord(this.name, record)
      .then(() => this.refreshRecords());
  }
  
  deleteRecord(record) {
    return this.dbService.deleteRecord(this.name, record)
      .then(() => this.refreshCount())
      .then(() => this.refreshRecords());
  }
}
