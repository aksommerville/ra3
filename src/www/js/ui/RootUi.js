/* RootUi.js
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";
import { NavBarUi } from "./NavBarUi.js";
import { StateService } from "../model/StateService.js";
import { GameDetailsModal } from "./GameDetailsModal.js";
import { ScreencapModal } from "./ScreencapModal.js";

export class RootUi {
  static getDependencies() {
    return [HTMLElement, Dom, Comm, Window, StateService];
  }
  constructor(element, dom, comm, window, stateService) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    this.window = window;
    this.stateService = stateService;
    
    this.navBar = null;
    this.mainController = null;
    this.activePage = -1;
    
    this.hashListener = e => this.onHashChange(e.newURL || "");
    this.window.addEventListener("hashchange", this.hashListener);
    
    this.buildUi();
    
    this.stateListener = this.stateService.listen("", () => this.onStateChange());
    
    // Capture initial fragment. If setting it doesn't change anything (eg it's blank),
    // force a state change anyway to get the initial SearchUi.
    if (!this.onHashChange(this.window.location.href || "")) {
      this.onStateChange();
    }
    
    this.wsListener = this.comm.listenWs((type, packet) => this.onWsMessage(type, packet));
  }
  
  onRemoveFromDom() {
    if (this.hashListener) {
      this.window.removeEventListener("hashchange", this.hashListener);
      this.hashListener = null;
    }
    if (this.stateListener) {
      this.stateService.unlisten(this.stateListener);
      this.stateListener = null;
    }
    if (this.wsListener) {
      this.comm.unlistenWs(this.wsListener);
      this.wsListener = null;
    }
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.navBar = this.dom.spawnController(this.element, NavBarUi);
    this.dom.spawn(this.element, "DIV", ["main"]);
  }
  
  onHashChange(newUrl) {
    const fragment = newUrl.split("#")[1] || "";
    return this.stateService.setEncoded(fragment);
  }
  
  onStateChange() {
    const fragment = this.stateService.encode();
    this.window.history.replaceState(null, "", this.window.location.pathname + "#" + fragment);
    this.replaceActivePageIfChanged();
    this.replaceDetailsModalIfChanged();
  }
  
  replaceActivePageIfChanged() {
    if (this.activePage === this.stateService.state.page) return;
    this.activePage = this.stateService.state.page;
    const main = this.element.querySelector(".main");
    main.innerHTML = "";
    this.mainController = null;
    const ctlcls = NavBarUi.controllerClassForIndex(this.stateService.state.page);
    if (ctlcls) {
      this.mainController = this.dom.spawnController(main, ctlcls);
    }
  }
  
  replaceDetailsModalIfChanged() {
    let modal = this.dom.findModalByControllerClass(GameDetailsModal);
    const desiredGameid = this.stateService.state.detailsGameid;
    if (desiredGameid) {
      if (!modal) {
        modal = this.dom.spawnModal(GameDetailsModal);
        modal.setupGameid(desiredGameid);
      } else if (modal.game?.gameid !== desiredGameid) {
        modal.setupGameid(desiredGameid);
      } else {
        // already open
      }
    } else if (modal) {
      this.dom.dismissModalByController(modal);
    } else {
      // already closed
    }
  }
  
  onWsMessage(type, packet) {
    //console.log(`RootUi.onWsMessage`, { type, packet });
    if (type === "binary") {
      if (this.decodeAndDisplayScreencap(packet)) ;
    }
  }
  
  // Returns false if it's not a PNG file; we might add other binary things in the future.
  // (but PNG packets will always be screencaps)
  decodeAndDisplayScreencap(packet) {
    if (packet instanceof ArrayBuffer) packet = new Uint8Array(packet);
    if (packet.length < 8) return false;
    // memcmp.... this is less elegant in JS than in C :(
    const header = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];
    for (let i=0; i<8; i++) if (packet[i] !== header[i]) return false;
    // OK it's close enough to PNG. Let's show it in a modal.
    const modal = this.dom.spawnModal(ScreencapModal);
    modal.setupPng(packet);
    return true;
  }
}
