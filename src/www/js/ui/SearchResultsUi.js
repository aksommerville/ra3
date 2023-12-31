/* SearchResultsUi.js
 * Pager and list of games, where you can launch or open the editor.
 */
 
import { Dom } from "../Dom.js";
import { SearchUi } from "./SearchUi.js";
import { SearchResultCard } from "./SearchResultCard.js";
import { StateService } from "../model/StateService.js";

export class SearchResultsUi {
  static getDependencies() {
    return [HTMLElement, Dom, SearchUi, StateService];
  }
  constructor(element, dom, searchUi, stateService) {
    this.element = element;
    this.dom = dom;
    this.searchUi = searchUi;
    this.stateService = stateService;
    
    this.PAGE_SIZE = 12;
    
    this.pagep = 1;
    this.pagec = 1;
    this.results = [];
    
    this.buildUi();
    
    // We don't listen to StateService; SearchUi handles that for us.
  }
  
  setResults(results, pageCount) {
    this.results = results || [];
    this.pagec = pageCount || 1;
    if (this.pagep > this.pagec) this.pagep = this.pagec;
    this.populate();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const ribbon = this.dom.spawn(this.element, "DIV", ["ribbon"]);
    this.dom.spawn(ribbon, "INPUT", { type: "button", value: "<", "on-click": () => this.changePage(-1) });
    this.dom.spawn(ribbon, "DIV", ["message"]);
    this.dom.spawn(ribbon, "INPUT", { type: "button", value: ">", "on-click": () => this.changePage(1) });
    const field = this.dom.spawn(this.element, "DIV", ["field"]);
  }
  
  changePage(d) {
    const np = this.pagep + d;
    if (np < 1) return;
    if (np > this.pagec) return;
    else this.pagep = np;
    this.depopulate();
    this.stateService.patchState({ searchPage: this.pagep });
  }
  
  depopulate() {
    this.element.querySelector(".ribbon > .message").innerHTML = "";
    this.element.querySelector(".field").innerHTML = "";
  }
  
  populate() {
  
    const message = this.element.querySelector(".ribbon > .message");
    message.innerHTML = "";
    if (this.results.length) {
      this.dom.spawn(message, "SPAN", `Page ${this.pagep} of ${this.pagec}`);
      this.dom.spawn(message, "INPUT", { type: "button", value: "Play Random", "on-click": () => this.searchUi.playRandom() });
    }
    
    const field = this.element.querySelector(".field");
    field.innerHTML = "";
    for (const game of this.results) {
      const card = this.dom.spawnController(field, SearchResultCard);
      card.setGame(game);
    }
  }
}
