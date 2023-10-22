/* GameListsForm.js
 * Part of GameDetailsModal.
 */
 
import { Dom } from "../Dom.js";
import { GameDetailsModal } from "./GameDetailsModal.js";
import { DbService } from "../model/DbService.js";
import { DbRecordModal } from "./DbRecordModal.js";

export class GameListsForm {
  static getDependencies() {
    return [HTMLElement, Dom, GameDetailsModal, DbService];
  }
  constructor(element, dom, gameDetailsModal, dbService) {
    this.element = element;
    this.dom = dom;
    this.gameDetailsModal = gameDetailsModal;
    this.dbService = dbService;
    
    // Remains constant (doesn't match UI, after user changes something).
    this.game = null;
    
    this.suggestions = [];
    
    this.buildUi();
    
    this.dbService.suggestLists().then(suggestions => {
      this.suggestions = suggestions;
      this.populate();
    });
  }
  
  setGame(game) {
    this.game = game;
    this.populate();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const fieldset = this.dom.spawn(this.element, "FIELDSET");
    this.dom.spawn(fieldset, "LEGEND", "Lists");
    const mainRow = this.dom.spawn(fieldset, "DIV", ["mainRow"]);
    this.spawnWell(mainRow, "Present");
    this.spawnSeparator(mainRow);
    this.spawnWell(mainRow, "Available");
  }
  
  spawnWell(mainRow, title) {
    const container = this.dom.spawn(mainRow, "DIV", ["well"]);
    this.dom.spawn(container, "H3", title);
    this.dom.spawn(container, "SELECT", { multiple: "multiple", "data-title": title });
  }
  
  spawnSeparator(mainRow) {
    const container = this.dom.spawn(mainRow, "DIV", ["separator"]);
    this.dom.spawn(container, "INPUT", { type: "button", value: "<", "on-click": () => this.onAddList() });
    this.dom.spawn(container, "INPUT", { type: "button", value: "+", "on-click": () => this.onCreateList() });
    this.dom.spawn(container, "INPUT", { type: "button", value: ">", "on-click": () => this.onRemoveList() });
  }
  
  populate() {
    const present = this.element.querySelector("*[data-title='Present']");
    present.innerHTML = "";
    if (this.game?.lists) {
      for (const { listid, name } of this.game.lists) {
        this.addToList(present, listid, name);
      }
    }
    const available = this.element.querySelector("*[data-title='Available']");
    available.innerHTML = "";
    for (const name of this.suggestions) {
      if (this.game?.lists.find(l => ((l.listid == name) || (l.name === name)))) continue;
      this.addToList(available, 0, name);
    }
  }
  
  addToList(list, listid, name) {
    if (!name) {
      if (!(name = listid)) return;
    }
    this.dom.spawn(list, "OPTION", { value: name }, name);
  }
  
  setSaveInProgress(sip) {
    for (const input of this.element.querySelectorAll("input")) {
      input.disabled = sip;
    }
  }
  
  gatherChanges() {
    const desired = Array.from(this.element.querySelectorAll("*[data-title='Present'] option")).map(o => o.value);
    const existing = [...(this.game?.lists || [])];
    
    // Any item present in both lists, remove it from both.
    for (let i=desired.length; i-->0; ) {
      const name = desired[i];
      const existingIndex = existing.findIndex(l => (l.listid == name) || (l.name === name));
      if (existingIndex >= 0) {
        desired.splice(i, 1);
        existing.splice(existingIndex, 1);
      }
    }
    
    // Now everything that remains is a necessary action.
    const changes = [];
    for (const name of desired) changes.push({ action: "addToList", gameid: this.game.gameid, list: name });
    for (const { listid, name } of existing) changes.push({ action: "removeFromList", gameid: this.game.gameid, listid });
    return changes;
  }
  
  onAddList() {
    const present = this.element.querySelector("*[data-title='Present']");
    const available = this.element.querySelector("*[data-title='Available']");
    let changed = false;
    for (const option of Array.from(available.selectedOptions)) {
      option.remove();
      present.appendChild(option);
      changed = true;
    }
    if (changed) this.gameDetailsModal.dirty();
  }
  
  onRemoveList() {
    const present = this.element.querySelector("*[data-title='Present']");
    const available = this.element.querySelector("*[data-title='Available']");
    let changed = false;
    for (const option of Array.from(present.selectedOptions)) {
      option.remove();
      available.appendChild(option);
      changed = true;
    }
    if (changed) this.gameDetailsModal.dirty();
  }
  
  onCreateList() {
    // We don't interact with GameDetailsModal much for this; creating a new list is outside its scope.
    // Creating lists at all, from this point in the UI, is really not our responsibility. But it's convenient to the user.
    const modal = this.dom.spawnModal(DbRecordModal);
    modal.setupNew("list", this.dbService.getTableSchema("list"));
    modal.onSave = record => {
      this.setSaveInProgress(true);
      return this.dbService.insertRecord("list", record).then(list => {
        // OK the list exists. Now adding it to Present will cause us to add it to the game at the next gatherChanges.
        this.addToList(this.element.querySelector("*[data-title='Present']"), list.listid, list.name);
        this.gameDetailsModal.dirty();
        this.setSaveInProgress(false);
      });
    };
  }
}
