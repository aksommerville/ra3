/* NavBarUi.js
 * Ribbon across the top of the page, child of RootUi and always present.
 */
 
import { Dom } from "../Dom.js";

export class NavBarUi {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.onTabChange = (name) => {}; // parent should set
    
    this.tabid = "";
    
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const tabBar = this.dom.spawn(this.element, "UL", ["tabBar"], { "on-click": e => this.onTabBarClick(e) });
    this.dom.spawn(tabBar, "LI", "Search", { "data-tabid": "search" });
    this.dom.spawn(tabBar, "LI", "Now Playing", { "data-tabid": "nowPlaying" });
    this.dom.spawn(tabBar, "LI", "DB", { "data-tabid": "db" });
    this.dom.spawn(tabBar, "LI", "Admin", { "data-tabid": "admin" });
  }
  
  onTabBarClick(e) {
    if (e.target.tagName !== "LI") return;
    const tabid = e.target.getAttribute("data-tabid");
    if (!tabid) return;
    if (tabid === this.tabid) return;
    this.tabid = tabid;
    for (const element of this.element.querySelectorAll("li.selected")) {
      element.classList.remove("selected");
    }
    e.target.classList.add("selected");
    this.onTabChange(tabid);
  }
  
  selectTab(name) {
    if (name === this.tabid) return;
    this.tabid = name;
    for (const element of this.element.querySelectorAll("li.selected")) {
      element.classList.remove("selected");
    }
    this.element.querySelector(`li[data-tabid='${name}']`)?.classList.add("selected");
  }
}
