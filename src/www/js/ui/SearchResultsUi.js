/* SearchResultsUi.js
 * Pager and list of games, where you can launch or open the editor.
 */
 
import { Dom } from "../Dom.js";
import { SearchUi } from "./SearchUi.js";
import { SearchResultCard } from "./SearchResultCard.js";

export class SearchResultsUi {
  static getDependencies() {
    return [HTMLElement, Dom, SearchUi];
  }
  constructor(element, dom, searchUi) {
    this.element = element;
    this.dom = dom;
    this.searchUi = searchUi;
    
    this.PAGE_SIZE = 12;
    
    this.pagep = 1;
    this.pagec = 1;
    this.results = [];
    
    this.buildUi();
  }
  
  setResults(results, pageCount) {
    this.results = results || [];
    this.pagec = pageCount || 1;//Math.ceil(this.results.length / this.PAGE_SIZE);
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
    if (np < 1) this.pagep = 1;
    else if (np > this.pagec) this.pagep = this.pagec;
    else this.pagep = np;
    this.depopulate();
    this.searchUi.searchAgain();
  }
  
  depopulate() {
    this.element.querySelector(".ribbon > .message").innerText = "";
    this.element.querySelector(".field").innerHTML = "";
  }
  
  populate() {
    this.element.querySelector(".ribbon > .message").innerText = this.results.length ? `Page ${this.pagep} of ${this.pagec}` : "";
    const field = this.element.querySelector(".field");
    field.innerHTML = "";
    for (const game of this.results) {
      const card = this.dom.spawnController(field, SearchResultCard);
      card.setGame(game);
    }
  }
}
