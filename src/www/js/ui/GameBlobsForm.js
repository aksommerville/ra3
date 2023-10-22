/* GameBlobsForm.js
 * Part of GameDetailsModal.
 */
 
import { Dom } from "../Dom.js";
import { GameDetailsModal } from "./GameDetailsModal.js";
import { ChoiceModal } from "./ChoiceModal.js";

export class GameBlobsForm {
  static getDependencies() {
    return [HTMLElement, Dom, GameDetailsModal];
  }
  constructor(element, dom, gameDetailsModal) {
    this.element = element;
    this.dom = dom;
    this.gameDetailsModal = gameDetailsModal;
    
    // Remains constant (doesn't match UI, after user changes something).
    this.game = null;
    
    this.pendingDeletes = [];
    
    this.buildUi();
  }
  
  setGame(game) {
    this.game = game;
    this.populate();
  }
  
  buildUi() {
    this.element.innerHTML = "";
  }
  
  populate() {
    this.element.innerHTML = "";
    if (!this.game?.blobs) return;
    //TODO Filter to "scap" only.
    //TODO Limit two? Arrange an artificial test.
    for (const path of this.game.blobs) {
      this.dom.spawn(this.element, "IMG", { src: "/api/blob?path=" + path, "on-click": () => this.onClick(path) });
    }
  }
  
  setSaveInProgress(sip) {
  }
  
  gatherChanges() {
    const changes = this.pendingDeletes.map(path => ({ action: "deleteBlob", path }));
    this.pendingDeletes = [];
    return changes;
  }
  
  onClick(path) {
    const modal = this.dom.spawnModal(ChoiceModal);
    modal.setup("Really delete this screencap?", ["Delete"]).then(() => {
      this.element.querySelector(`img[src='/api/blob?path=${path}']`)?.remove();
      this.pendingDeletes.push(path);
      this.gameDetailsModal.dirty();
    });
  }
}
