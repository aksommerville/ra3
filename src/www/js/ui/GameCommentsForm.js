/* GameCommentsForm.js
 * Part of GameDetailsModal.
 */
 
import { Dom } from "../Dom.js";
import { GameDetailsModal } from "./GameDetailsModal.js";
import { DbRecordModal } from "./DbRecordModal.js";
import { DbService } from "../model/DbService.js";

export class GameCommentsForm {
  static getDependencies() {
    return [HTMLElement, Dom, GameDetailsModal, "discriminator", DbService];
  }
  constructor(element, dom, gameDetailsModal, discriminator, dbService) {
    this.element = element;
    this.dom = dom;
    this.gameDetailsModal = gameDetailsModal;
    this.discriminator = discriminator;
    this.dbService = dbService;
    
    // Remains constant (doesn't match UI, after user changes something).
    this.game = null;
    
    this.pendingComments = []; // {k,v}, we flush out at gatherChanges(). Displayed just like committed ones.
    this.pendingDeletes = []; // full records
    this.pendingPatches = []; // ''
    
    this.buildUi();
  }
  
  setGame(game) {
    this.game = game;
    this.populate();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    
    const fieldset = this.dom.spawn(this.element, "FIELDSET");
    this.dom.spawn(fieldset, "LEGEND", "Comments");
    this.dom.spawn(fieldset, "UL", ["commentList"]);
    
    const table = this.dom.spawn(fieldset, "TABLE");
    const trEntry = this.dom.spawn(table, "TR", ["entry"]);
    const tdEntry = this.dom.spawn(trEntry, "TD", { colspan: 2 });
    this.dom.spawn(tdEntry, "TEXTAREA", ["entry"], { name: "v" });
    const trControls = this.dom.spawn(table, "TR", ["controls"]);
    const tdK = this.dom.spawn(trControls, "TD");
    const datalistKId = `GameCommentsForm-${this.discriminator}-k`;
    const datalistK = this.dom.spawn(tdK, "DATALIST", { id: datalistKId });
    //TODO populate comment.k datalist
    this.dom.spawn(tdK, "INPUT", { type: "text", name: "k", list: datalistKId, value: "text" });
    const tdSave = this.dom.spawn(trControls, "TD");
    this.dom.spawn(tdSave, "INPUT", { type: "button", value: "Save", "on-click": () => this.onSaveNewComment() });
  }
  
  populate() {
    const commentList = this.element.querySelector(".commentList");
    commentList.innerHTML = "";
    if (this.game?.comments) {
      for (const comment of this.game.comments) {
        // Maybe should check pendingDeletes? I think that won't come up often.
        this.spawnComment(commentList, comment);
      }
    }
    if (this.pendingComments.length) {
      for (const comment of this.pendingComments) {
        this.spawnComment(commentList, comment);
      }
    }
  }
  
  spawnComment(commentList, comment) {
    const li = this.dom.spawn(commentList, "LI", ["comment"]);
    const top = this.dom.spawn(li, "DIV", ["top"]);
    // The "Edit" modal has its own delete button. No need for another.
    //this.dom.spawn(top, "INPUT", { type: "button", tabindex: "-1", value: "X", "on-click": () => this.onDeleteComment(comment, li) });
    this.dom.spawn(top, "DIV", ["time"], comment.time || "");
    this.dom.spawn(top, "DIV", ["k"], comment.k || "");
    this.dom.spawn(top, "INPUT", { type: "button", tabindex: "-1", value: "Edit", "on-click": () => this.onEditComment(comment, li) });
    this.dom.spawn(li, "DIV", ["bottom"], comment.v || "");
  }
  
  setSaveInProgress(sip) {
    if (sip) {
      this.element.querySelector("input[value='Save']").disabled = true;
    } else {
      this.element.querySelector("input[value='Save']").disabled = false;
    }
  }
  
  gatherChanges() {
    const changes = this.pendingComments.map(cmt => ({
      action: "putComment",
      body: cmt,
    }));
    this.pendingComments = [];
    if (this.pendingDeletes.length) {
      for (const comment of this.pendingDeletes) {
        changes.push({
          action: "deleteComment",
          body: comment,
        });
      }
      this.pendingDeletes = [];
    }
    if (this.pendingPatches.length) {
      for (const comment of this.pendingPatches) {
        changes.push({
          action: "patchComment",
          body: comment,
        });
      }
      this.pendingPatches = [];
    }
    return changes;
  }
  
  onSaveNewComment() {
    const inputV = this.element.querySelector("textarea[name='v']");
    const inputK = this.element.querySelector("input[name='k']");
    const v = inputV.value.trim();
    const k = inputK.value.trim();
    if (!v || !k) return;
    this.pendingComments.push({ k, v });
    inputV.value = "";
    //inputK.value = ""; // I don't expect a lot of change to (k), it should usually be "text".
    this.populate();
    this.gameDetailsModal.dirty();
  }
  
  onDeleteComment(comment, li) {
    this.pendingDeletes.push(comment);
    this.gameDetailsModal.dirty();
    li?.remove();
  }
  
  onEditComment(comment, li) {
    const modal = this.dom.spawnModal(DbRecordModal);
    modal.setupEdit("comment", this.dbService.getTableSchema("comment"), comment);
    modal.onSave = record => {
      this.pendingPatches.push(record);
      this.gameDetailsModal.dirty();
      li.querySelector(".bottom").innerText = record.v || "";
      return Promise.resolve(); // we lie to the modal and pretend it commits immediately
    };
    modal.onDelete = record => {
      this.onDeleteComment(comment, li);
      return Promise.resolve();
    };
  }
}
