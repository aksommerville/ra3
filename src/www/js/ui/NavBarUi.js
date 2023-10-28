/* NavBarUi.js
 * Ribbon across the top of the page, child of RootUi and always present.
 */
 
import { Dom } from "../Dom.js";
import { StateService } from "../model/StateService.js";
import { SearchUi } from "./SearchUi.js";
import { ListsUi } from "./ListsUi.js";
import { NowPlayingUi } from "./NowPlayingUi.js";
import { DbUi } from "./DbUi.js";
import { AdminUi } from "./AdminUi.js";

export class NavBarUi {
  static getDependencies() {
    return [HTMLElement, Dom, StateService];
  }
  constructor(element, dom, stateService) {
    this.element = element;
    this.dom = dom;
    this.stateService = stateService;
    
    this.page = -1;
    
    this.buildUi();
    
    this.stateListener = this.stateService.listen("page", () => this.onStateChange());
    this.onStateChange();
  }
  
  onRemoveFromDom() {
    if (this.stateListener) {
      this.stateService.unlisten(this.stateListener);
      this.stateListener = null;
    }
  }
  
  /* (index) here is the same as StateService.state.page.
   */
  static controllerClassForIndex(page) {
    return NavBarUi.CONTROLLER_CLASSES[page];
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const tabBar = this.dom.spawn(this.element, "UL", ["tabBar"], { "on-click": e => this.onTabBarClick(e) });
    let page = 0;
    for (const cls of NavBarUi.CONTROLLER_CLASSES) {
      this.dom.spawn(tabBar, "LI", { "data-page": page++ }, cls.TAB_LABEL || cls.name);
    }
  }
  
  onTabBarClick(e) {
    if (e.target.tagName !== "LI") return;
    let page = e.target.getAttribute("data-page");
    if (!page) return;
    page = +page;
    if (page === this.page) return;
    this.page = page;
    for (const element of this.element.querySelectorAll("li.selected")) {
      element.classList.remove("selected");
    }
    e.target.classList.add("selected");
    this.stateService.patchState({ page });
  }
  
  onStateChange() {
    const page = this.stateService.state.page;
    if (page === this.page) return;
    this.page = page;
    for (const element of this.element.querySelectorAll("li.selected")) {
      element.classList.remove("selected");
    }
    this.element.querySelector(`li[data-page='${page}']`)?.classList.add("selected");
  }
}

NavBarUi.CONTROLLER_CLASSES = [
  SearchUi,
  ListsUi,
  NowPlayingUi,
  DbUi,
  AdminUi,
];
