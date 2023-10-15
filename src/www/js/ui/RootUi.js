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
    return [HTMLElement, Dom, Comm];
  }
  constructor(element, dom, comm) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    
    this.navBar = null;
    this.mainController = null;
    
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.navBar = this.dom.spawnController(this.element, NavBarUi);
    this.navBar.onTabChange = (tabid) => this.onTabChange(tabid);
    this.dom.spawn(this.element, "DIV", ["main"]);
  }
  
  onTabChange(tabid) {
    //TODO We'll probably want to incorporate URL fragments at some point, so the tab selection persists across refreshes.
    const main = this.element.querySelector(".main");
    main.innerHTML = "";
    this.mainController = null;
    let ctlcls = null;
    switch (tabid) {
      case "search": ctlcls = SearchUi; break;
      case "nowPlaying": ctlcls = NowPlayingUi; break;
      case "db": ctlcls = DbUi; break;
      case "admin": ctlcls = AdminUi; break;
    }
    if (!ctlcls) return;
    this.mainController = this.dom.spawnController(main, ctlcls);
  }
}
