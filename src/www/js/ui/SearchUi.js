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
    return [HTMLElement, Dom, Comm, DbService, Window];
  }
  constructor(element, dom, comm, dbService, window) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    this.dbService = dbService;
    this.window = window;
    
    this.searchForm = null;
    this.searchResults = null;
    this.previousQuery = null;
    
    this.buildUi();
  }
  
  setup(path) {
    if (path.length >= 2) {
      this.dbService.recentSearchPath = path;
      const query = {};
      for (const field of path.slice(1)) {
        let [k, v] = field.split("=");
        v = decodeURIComponent(v);
        if (k === "page") this.searchResults.pagep = +v;
        else query[k] = v;
      }
      this.search(query);
      this.searchForm.populateWithQuery(query);
    } else if (this.dbService.recentSearchPath?.length >= 2) {
      this.setup(this.dbService.recentSearchPath);
    }
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.searchForm = this.dom.spawnController(this.element, SearchFormUi, [this]);
    this.searchResults = this.dom.spawnController(this.element, SearchResultsUi, [this]);
  }
  
  reduceQuery(input) {
    const output = {};
    for (const k of ["author", "flags", "notflags", "genre", "list", "platform", "pubtime", "rating", "sort", "text"]) {
      if (input[k]) output[k] = input[k];
    }
    if (output.sort === "none") delete output.sort;
    return output;
  }
  
  encodeUrl(query) {
    let url = this.window.location.pathname + "#search";
    if (query) {
      for (const k of Object.keys(query)) {
        url += "/" + encodeURIComponent(k) + "=" + encodeURIComponent(query[k]);
      }
      if (this.searchResults?.pagep) {
        url += "/page=" + this.searchResults.pagep;
      }
    }
    this.dbService.recentSearchPath = url.substring(1).split("/");
    return url;
  }
  
  search(query) {
    query = this.reduceQuery(query);
    this.window.history.replaceState(null, "", this.encodeUrl(query));
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
