/* SearchUi.js
 * Contains two major children: SearchFormUi and SearchResultsUi.
 * We manage the coordination between them, and their contact with the outside world.
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";
import { DbService } from "../model/DbService.js";
import { SearchFormUi } from "./SearchFormUi.js";
import { SearchResultsUi } from "./SearchResultsUi.js";
 
export class SearchUi {
  static getDependencies() {
    return [HTMLElement, Dom, Comm, DbService];
  }
  constructor(element, dom, comm, dbService) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    this.dbService = dbService;
    
    this.searchForm = null;
    this.searchResults = null;
    this.previousQuery = null;
    
    this.buildUi();
  }
  
  setup(path) {
    // TODO It is worthwhile to encode the form state, and results page.
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.searchForm = this.dom.spawnController(this.element, SearchFormUi, [this]);
    this.searchResults = this.dom.spawnController(this.element, SearchResultsUi, [this]);
  }
  
  search(query) {
    this.previousQuery = query;
    this.dbService.query({
      ...query,
      limit: this.searchResults.PAGE_SIZE,
      page: this.searchResults.pagep,
      detail: "blobs",
    }).then(rsp => {
      this.searchResults.setResults(rsp.results, rsp.pageCount);
    }).catch(e => {
      console.log(`search failed!`, e);
    });
  }
  
  // Repeat the last search. Note that page gets pulled dynamically. Typically this gets called because page changed.
  searchAgain() {
    if (!this.previousQuery) return;
    this.search(this.previousQuery);
  }
  
  playRandom() {
    if (!this.searchResults) return;
    if (this.searchResults.length < 1) return;
    if (!this.previousQuery) return;
    this.comm.httpJson("POST", "/api/random", this.previousQuery).then(rsp => {
      console.log(`launched a game`, rsp);
    }).catch(e => {
      console.log(`random launch failed`, e);
    });
  }
}
