/* RootUi.js
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";
import { NavBarUi } from "./NavBarUi.js";
import { StateService } from "../model/StateService.js";
import { GameDetailsModal } from "./GameDetailsModal.js";

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
    
    this.onHashChange(this.window.location.href || ""); // Capture the initial fragment.
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
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.navBar = this.dom.spawnController(this.element, NavBarUi);
    this.dom.spawn(this.element, "DIV", ["main"]);
  }
  
  onHashChange(newUrl) {
    const fragment = newUrl.split("#")[1] || "";
    this.stateService.setEncoded(fragment);
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
}
