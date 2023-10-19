/* RootUi.js
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";
import { NavBarUi } from "./NavBarUi.js";
import { SearchUi } from "./SearchUi.js";
import { NowPlayingUi } from "./NowPlayingUi.js";
import { DbUi } from "./DbUi.js";
import { AdminUi } from "./AdminUi.js";

export class RootUi {
  static getDependencies() {
    return [HTMLElement, Dom, Comm, Window];
  }
  constructor(element, dom, comm, window) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    this.window = window;
    
    this.navBar = null;
    this.mainController = null;
    
    this.hashListener = e => this.onHashChange(e.newURL || "");
    this.window.addEventListener("hashchange", this.hashListener);
    
    this.buildUi();
    this.onHashChange(this.window.location.href || ""); // Capture the initial fragment.
  }
  
  onRemoveFromDom() {
    if (this.hashListener) {
      this.window.removeEventListener("hashchange", this.hashListener);
      this.hashListener = null;
    }
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.navBar = this.dom.spawnController(this.element, NavBarUi);
    this.navBar.onTabChange = (tabid) => this.onTabChange(tabid);
    this.dom.spawn(this.element, "DIV", ["main"]);
  }
  
  onTabChange(tabid) {
    this.window.location = "#" + tabid;
  }
  
  onHashChange(newUrl) {
    const fragment = newUrl.split("#")[1] || "";
    const path = fragment.split('/');
    this.navBar.selectTab(path[0]);
    const main = this.element.querySelector(".main");
    main.innerHTML = "";
    this.mainController = null;
    let ctlcls = null;
    switch (path[0]) {
      case "search": ctlcls = SearchUi; break;
      case "nowPlaying": ctlcls = NowPlayingUi; break;
      case "db": ctlcls = DbUi; break;
      case "admin": ctlcls = AdminUi; break;
    }
    if (!ctlcls) return;
    this.mainController = this.dom.spawnController(main, ctlcls);
    this.mainController.setup?.(path);
  }
}
