/* GamePlaysForm.js
 * Part of GameDetailsModal.
 */
 
import { Dom } from "../Dom.js";
import { GameDetailsModal } from "./GameDetailsModal.js";
import { DbService } from "../model/DbService.js";
import { DbRecordModal } from "./DbRecordModal.js";

export class GamePlaysForm {
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
    
    this.pendingPatches = [];
    this.pendingDeletes = [];
    
    this.buildUi();
  }
  
  setGame(game) {
    this.game = game;
    this.populate();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const fieldset = this.dom.spawn(this.element, "FIELDSET");
    this.dom.spawn(fieldset, "LEGEND", "Plays");
    this.dom.spawn(fieldset, "UL", ["playList"]);
    // We don't have a "create" form like comments. Adding play records is a silly thing to do. (but can use the DB view if you want)
  }
  
  populate() {
    const playList = this.element.querySelector(".playList");
    playList.innerHTML = "";
    let plays = this.game?.plays || [];
    if (plays.length > 10) {
      const rmc = plays.length - 10;
      this.dom.spawn(playList, "LI", `(${rmc} more not shown)`);
      plays = plays.slice(rmc);
    }
    for (const play of plays) {
      this.spawn(playList, play);
    }
  }
  
  spawn(playList, play) {
    const li = this.dom.spawn(playList, "LI", ["play"], `${play.time} : ${play.dur_m} min`);
    li.addEventListener("click", () => this.editPlay(playList, play, li));
  }
  
  setSaveInProgress(sip) {
  }
  
  gatherChanges() {
    const changes = [
      ...this.pendingPatches.map(play => ({ action: "patchPlay", body: play })),
      ...this.pendingDeletes.map(play => ({ action: "deletePlay", body: play })),
    ];
    this.pendingPatches = [];
    this.pendingDeletes = [];
    return changes;
  }
  
  editPlay(playList, play, li) {
    const modal = this.dom.spawnModal(DbRecordModal);
    modal.setupEdit("play", this.dbService.getTableSchema("play"), play);
    modal.onSave = record => {
      this.pendingPatches.push(record);
      this.gameDetailsModal.dirty();
      li.innerText = `${record.time} : ${record.dur_m} min`;
      return Promise.resolve(); // we lie to the modal and pretend it commits immediately
    };
    modal.onDelete = record => {
      this.pendingDeletes.push(record);
      this.gameDetailsModal.dirty();
      li.remove();
      return Promise.resolve();
    };
  }
}
