/* DbTableUi.js
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";
import { DbLauncherModal } from "./DbLauncherModal.js";
import { DbListModal } from "./DbListModal.js";
import { DbGameModal } from "./DbGameModal.js";
import { DbCommentModal } from "./DbCommentModal.js";
import { DbPlayModal } from "./DbPlayModal.js";
import { DbBlobModal } from "./DbBlobModal.js";

export class DbTableUi {
  static getDependencies() {
    return [HTMLElement, Dom, Comm, Window];
  }
  constructor(element, dom, comm, window) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    this.window = window;
    
    this.name = "";
    this.records = [];
    
    this.buildUi();
  }
  
  setTable(name) {
    this.name = name;
    this.element.querySelector(".name").innerText = this.name;
    this.refreshCount();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const card = this.dom.spawn(this.element, "DIV", ["card"]);
    const header = this.dom.spawn(card, "DIV", ["header"]);
    this.dom.spawn(header, "H2", ["name"]);
    this.dom.spawn(header, "DIV", ["spacer"]);
    this.dom.spawn(header, "INPUT", ["new"], { type: "button", value: "New", "on-click": () => this.onNew() });
    this.dom.spawn(header, "INPUT", ["openToggle"], { type: "button", value: "Open", "on-click": () => this.toggleOpen() });
    this.dom.spawn(card, "DIV", ["togglePanel", "hidden"]);
  }
  
  rebuildRecordList() {
    const panel = this.element.querySelector(".togglePanel");
    panel.innerHTML = "";
    const modalClass = this.getModalClass();
    for (const record of this.records) {
      const text = modalClass ? modalClass.reprRecordForList(record) : JSON.stringify(record);
      this.dom.spawn(panel, "PRE", ["record"], text, { "on-click": () => this.onEdit(record) });
    }
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
  
  onNew() {
    this.forceOpen(true);
    const modalClass = this.getModalClass();
    if (!modalClass) return;
    const modal = this.dom.spawnModal(modalClass);
    modal.setupNew();
    modal.onSave = record => this.insertRecord(record);
  }
  
  onEdit(record) {
    const modalClass = this.getModalClass();
    if (!modalClass) return;
    const modal = this.dom.spawnModal(modalClass);
    modal.setupEdit(record);
    modal.onSave = record => this.updateRecord(record);
  }
  
  getModalClass() {
    switch (this.name) {
      case "launcher": return DbLauncherModal;
      case "list": return DbListModal;
      case "game": return DbGameModal;
      case "comment": return DbCommentModal;
      case "play": return DbPlayModal;
      case "blob": return DbBlobModal;
    }
    return null;
  }
  
  refreshCount() {
    // Blobs don't and shouldn't have a "count" endpoint; the backend scans for them every time, so one might as well just get all.
    if (this.name === "blob") return Promise.resolve(null);
    return this.comm.httpJson("GET", `/api/${this.name}/count`).then(count => {
      this.element.querySelector(".name").innerText = `${this.name} (${count})`;
    }).catch(e => {
      this.window.console.error(`Error fetching ${this.name} count.`, e);
    });
  }
  
  refreshRecords() {
    let httpCall;
    if (this.name === "blob") {
      httpCall = this.comm.httpJson("GET", "/api/blob/all");
    } else {
      httpCall = this.comm.httpJson("GET", `/api/${this.name}`, { index: 0, count: 999999, detail: "id" });
    }
    httpCall.then(rsp => {
      if (rsp instanceof Array) {
        this.records = rsp;
      } else {
        this.records = [];
      }
      this.rebuildRecordList();
    }).catch(e => {
      this.window.console.error(`DbTableUi ${this.name} error`, e);
    });
  }
  
  insertRecord(record) {
    console.log(`TODO DbTableUi ${this.name} insertRecord`, record);
    //TODO probly wrong for blobs. figure it out when we make the blob modal
    return this.comm.httpJson("PUT", `/api/${this.name}`, null, null, JSON.stringify(record)).then(rsp => {
      console.log(`insert ok`, rsp);
      return rsp;
    });
  }
  
  updateRecord(record) {
    console.log(`TODO DbTableUi ${this.name} updateRecord`, record);
    return this.comm.httpJson("PUT", `/api/${this.name}`, null, null, JSON.stringify(record)).then(rsp => {
      console.log(`update ok`, rsp);
      return rsp;
    });
  }
}
