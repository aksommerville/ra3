/* NowPlayingUi.js
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";
import { GameDetailsModal } from "./GameDetailsModal.js";
import { DbService } from "../model/DbService.js";
 
export class NowPlayingUi {
  static getDependencies() {
    return [HTMLElement, Dom, Comm, DbService];
  }
  constructor(element, dom, comm, dbService) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    this.dbService = dbService;
    
    this.buildUi();
    
    this.wsListener = this.comm.listenWs((type, packet) => this.onWsRcv(type, packet));
    this.onGameChanged(this.comm.currentGameid);
  }
  
  onRemoveFromDom() {
    this.comm.unlistenWs(this.wsListener);
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "H2", ["title"], "(nothing running)");
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "Details", disabled: "disabled", "on-click": () => this.onDetails() });
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "Screencap", disabled: "disabled", "on-click": () => this.onParamlessJson("requestScreencap") });
    const controlsRow = this.dom.spawn(this.element, "DIV", ["controlsRow"]);
    this.dom.spawn(controlsRow, "INPUT", { type: "button", value: "Pause", disabled: "disabled", "on-click": () => this.onParamlessJson("pause") });
    this.dom.spawn(controlsRow, "INPUT", { type: "button", value: "Resume", disabled: "disabled", "on-click": () => this.onParamlessJson("resume") });
    this.dom.spawn(controlsRow, "INPUT", { type: "button", value: "Step", disabled: "disabled", "on-click": () => this.onParamlessJson("step") });
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "Terminate", disabled: "disabled", "on-click": () => this.onTerminate() });
  }
  
  populateUiNone() {
    this.element.querySelector(".title").innerText = "";
    for (const input of this.element.querySelectorAll("input")) input.disabled = true;
  }
  
  populateUiForGame(game) {
    this.element.querySelector(".title").innerText = game.name || "";
    for (const input of this.element.querySelectorAll("input")) input.disabled = false;
  }
  
  onDetails() {
    if (!this.comm.currentGameid) return;
    const modal = this.dom.spawnModal(GameDetailsModal);
    modal.setupGameid(this.comm.currentGameid);
  }
  
  onParamlessJson(id) {
    if (!this.comm.currentGameid) return;
    this.comm.sendWsJson({ id });
  }
  
  onTerminate() {
    this.comm.http("POST", "/api/terminate");
  }
  
  onWsRcv(type, packet) {
    switch (type) {
      case "json": switch (packet.id) {
          case "game": this.onGameChanged(packet.gameid); break;
        } break;
    }
  }
  
  onGameChanged(gameid) {
    if (gameid) {
      this.dbService.fetchGame(gameid, "record")
        .then(game => this.populateUiForGame(game))
        .catch(error => this.populateUiNone());
    } else {
      this.populateUiNone();
    }
  }
}

NowPlayingUi.TAB_LABEL = "Now Playing";
