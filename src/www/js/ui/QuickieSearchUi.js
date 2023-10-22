/* QuickieSearchUi.js
 * Search for files, but not the big dedicated page.
 * Accepts a query as text only, and shows results in a flat list.
 */
 
import { Dom } from "../Dom.js";
import { DbService } from "../model/DbService.js";
import { GameDetailsModal } from "./GameDetailsModal.js";

export class QuickieSearchUi {
  static getDependencies() {
    return [HTMLElement, Dom, DbService, Window];
  }
  constructor(element, dom, dbService, window) {
    this.element = element;
    this.dom = dom;
    this.dbService = dbService;
    this.window = window;
    
    this.onClickGame = ({gameid,name}) => {};
    this.enableEdit = true; // Set false to disable on further searches.
    
    this.SEARCH_TIMEOUT = 2000;
    this.searchTimeout = null;
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    if (this.searchTimeout) {
      this.window.clearTimeout(this.searchTimeout);
      this.searchTimeout = null;
    }
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": e => this.onSubmit(e) });
    this.dom.spawn(form, "INPUT", ["hidden"], { type: "submit" });
    this.dom.spawn(form, "INPUT", ["query"], { type: "text", "on-input": () => this.onInput() });
    this.dom.spawn(form, "UL", ["results"]);
  }
  
  populateSearchResults(files) {
    const ul = this.element.querySelector(".results");
    ul.innerHTML = "";
    for (const { gameid, name } of files) {
      const li = this.dom.spawn(ul, "LI");
      if (this.enableEdit) this.dom.spawn(li, "INPUT", { type: "button", value: "...", "on-click": () => this.onEditGame(gameid) });
      this.dom.spawn(li, "DIV", `${gameid}: ${name}`, { "on-click": () => this.onClickGame({ gameid, name }) });
    }
  }
  
  onInput() {
    if (this.searchTimeout) {
      this.window.clearTimeout(this.searchTimeout);
      this.searchTimeout = null;
    }
    this.searchTimeout = this.window.setTimeout(() => {
      this.searchTimeout = null;
      this.searchNow();
    }, this.SEARCH_TIMEOUT);
  }
  
  onSubmit(e) {
    e.preventDefault();
    this.searchNow();
  }
  
  searchNow() {
    if (this.searchTimeout) {
      this.window.clearTimeout(this.searchTimeout);
      this.searchTimeout = null;
    }
    const text = this.element.querySelector("input.query").value.trim();
    this.dbService.query({ text, limit: 1000, detail: "name" }).then(files => {
      this.populateSearchResults(files.results);
    }).catch(error => {
      this.window.console.error(`Search failed`, error);
    });
  }
  
  onEditGame(gameid) {
    const modal = this.dom.spawnModal(GameDetailsModal);
    modal.setupGameid(gameid);
  }
}
