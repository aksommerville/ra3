/* GameDetailsModal.js
 * The modal manages its own contact with the backend.
 * Clients can be notified of changes, but they're not expected to do anything about it.
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";
import { DbService } from "../model/DbService.js";
import { GameHeaderForm } from "./GameHeaderForm.js";
import { GameCommentsForm } from "./GameCommentsForm.js";
import { GamePlaysForm } from "./GamePlaysForm.js";
import { GameListsForm } from "./GameListsForm.js";
import { GameBlobsForm } from "./GameBlobsForm.js";
import { ChoiceModal } from "./ChoiceModal.js";
import { StateService } from "../model/StateService.js";

export class GameDetailsModal {
  static getDependencies() {
    return [HTMLElement, Dom, DbService, Window, Comm, StateService];
  }
  constructor(element, dom, dbService, window, comm, stateService) {
    this.element = element;
    this.dom = dom;
    this.dbService = dbService;
    this.window = window;
    this.comm = comm;
    this.stateService = stateService;
    
    // We call this after a change has been saved.
    this.onChanged = (game) => {};
    
    this.SAVE_TIMEOUT = 3000;
    
    this.game = null;
    this.saveTimeout = null;
    this.saveInProgress = false;
    this.gameHeaderForm = null;
    this.gameCommentsForm = null;
    this.gamePlaysForm = null;
    this.gameListsForm = null;
    this.gameBlobsForm = null;
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    if (this.saveTimeout) {
      this.saveNow();
    }
    this.stateService.patchState({ detailsGameid: 0 });
  }
  
  setupFull(game) {
    this.game = game;
    this.populate();
    this.stateService.patchState({ detailsGameid: this.game?.gameid || 0 });
  }
  
  setupGameid(gameid) {
    return this.dbService.fetchGame(gameid, "blobs").then(game => this.setupFull(game));
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const form = this.dom.spawn(this.element, "FORM", {
      "on-submit": e => {
        e.preventDefault();
        this.saveNow();
      },
    });
    
    const columner = this.dom.spawn(form, "DIV", ["columner"]);
    const leftColumn = this.dom.spawn(columner, "DIV");
    const rightColumn = this.dom.spawn(columner, "DIV");
    this.gameHeaderForm = this.dom.spawnController(leftColumn, GameHeaderForm, [this]);
    this.gameCommentsForm = this.dom.spawnController(leftColumn, GameCommentsForm, [this]);
    this.gamePlaysForm = this.dom.spawnController(leftColumn, GamePlaysForm, [this]);
    this.gameListsForm = this.dom.spawnController(rightColumn, GameListsForm, [this]);
    this.gameBlobsForm = this.dom.spawnController(rightColumn, GameBlobsForm, [this]);
    
    const buttonsRow = this.dom.spawn(form, "DIV", ["buttonsRow"]);
    this.dom.spawn(buttonsRow, "INPUT", ["gameModalOuterSaveButton"], {
      type: "submit",
      value: "Save",
      "on-click": () => this.onSave(),
      disabled: "disabled",
    });
    this.dom.spawn(buttonsRow, "INPUT", {
      type: "button",
      value: "Delete",
      "on-click": () => this.onDelete(),
    });
    this.dom.spawn(buttonsRow, "INPUT", {
      type: "button",
      value: "Launch",
      "on-click": () => this.onLaunch(),
    });
  }
  
  populate() {
    this.gameHeaderForm.setGame(this.game);
    this.gameCommentsForm.setGame(this.game);
    this.gamePlaysForm.setGame(this.game);
    this.gameListsForm.setGame(this.game);
    this.gameBlobsForm.setGame(this.game);
  }
  
  dirty() {
    if (this.saveTimeout) {
      this.window.clearTimeout(this.saveTimeout);
      this.saveTimeout = null;
    }
    this.saveTimeout = this.window.setTimeout(() => {
      this.saveTimeout = null;
      this.saveNow();
    }, this.SAVE_TIMEOUT);
    this.element.querySelector(".gameModalOuterSaveButton").disabled = false;
  }
  
  onSave() {
    this.saveNow();
  }
  
  saveNow() {
    if (this.saveTimeout) {
      this.window.clearTimeout(this.saveTimeout);
      this.saveTimeout = null;
    }
    
    const changes = [
      ...this.gameHeaderForm.gatherChanges(),
      ...this.gameCommentsForm.gatherChanges(),
      ...this.gamePlaysForm.gatherChanges(),
      ...this.gameListsForm.gatherChanges(),
      ...this.gameBlobsForm.gatherChanges(),
    ];
    if (!changes.length) return Promise.resolve();
    changes.push({ action: "refresh" });
    
    const update = (pendingChanges) => {
      if (!pendingChanges.length) return Promise.resolve();
      const change = pendingChanges[0];
      pendingChanges.splice(0, 1);
      return this.commitOneChange(change).then(() => update(pendingChanges));
    };
    
    this.setSaveInProgress(true);
    return update(changes).then(() => {
      this.setSaveInProgress(false);
      this.onChanged(this.game);
    }).catch(error => {
      this.setSaveInProgress(false);
      this.window.console.error(`Failed to save game ${this.game.gameid}`, error);
    });
  }
  
  setSaveInProgress(sip) {
    this.saveInProgress = sip;
    this.gameHeaderForm.setSaveInProgress(sip);
    this.gameCommentsForm.setSaveInProgress(sip);
    this.gamePlaysForm.setSaveInProgress(sip);
    this.gameListsForm.setSaveInProgress(sip);
    this.gameBlobsForm.setSaveInProgress(sip);
    this.element.querySelector("input.gameModalOuterSaveButton").disabled = true;
  }
  
  commitOneChange(change) {
    switch (change.action) {
      case "refresh": return this.setupGameid(this.game.gameid);
      case "header": return this.dbService.patchGame(change.body, "id");
      case "putComment": return this.dbService.putComment({ ...change.body, gameid: this.game.gameid });
      case "patchComment": return this.dbService.patchComment({ ...change.body, gameid: this.game.gameid });
      case "deleteComment": return this.dbService.deleteComment({ ...change.body, gameid: this.game.gameid });
      case "patchPlay": return this.dbService.patchPlay({ ...change.body, gameid: this.game.gameid });
      case "deletePlay": return this.dbService.deletePlay({ ...change.body, gameid: this.game.gameid });
      case "addToList": return this.dbService.addToList(change.gameid, change.list);
      case "removeFromList": return this.dbService.removeFromList(change.gameid, change.listid);
      case "deleteBlob": return this.dbService.deleteRecord("blob", change.path);
    }
    return Promise.reject({
      message: "Unexpected change at GameDetailsModal.commitOneChange",
      change,
    });
  }
  
  onDelete() {
    if (!this.game) return;
    const modal = this.dom.spawnModal(ChoiceModal);
    modal.setup("Really delete this game record and all associated records? The ROM file will not be deleted.", ["Delete"]).then(() => {
      this.dbService.deleteRecord(game, this.game).then(() => {
        this.dom.dismissModalByController(this);
      });
    });
  }
  
  onLaunch() {
    if (!this.game) return;
    this.comm.http("POST", `/api/launch?gameid=${this.game.gameid}`).then(() => {
      console.log("launch ok");
    }).catch(error => {
      console.log("launch failed", error);
    });
  }
}
