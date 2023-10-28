/* SearchUi.js
 * Contains two major children: SearchFormUi and SearchResultsUi.
 * We manage the coordination between them, and their contact with the outside world.
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";
import { DbService } from "../model/DbService.js";
import { StateService } from "../model/StateService.js";
import { SearchModel } from "../model/SearchModel.js";
import { SearchFormUi } from "./SearchFormUi.js";
import { SearchResultsUi } from "./SearchResultsUi.js";
 
export class SearchUi {
  static getDependencies() {
    return [HTMLElement, Dom, Comm, DbService, Window, StateService];
  }
  constructor(element, dom, comm, dbService, window, stateService) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    this.dbService = dbService;
    this.window = window;
    this.stateService = stateService;
    
    this.searchForm = null;
    this.searchResults = null;
    this.searchModel = new SearchModel();
    
    this.buildUi();
    
    this.stateListener = this.stateService.listen("", () => this.onStateChange());
    this.onStateChange();
  }
  
  onRemoveFromDom() {
    if (this.stateListener) {
      this.stateService.unlisten(this.stateListener);
      this.stateListener = null;
    }
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.searchForm = this.dom.spawnController(this.element, SearchFormUi, [this]);
    this.searchResults = this.dom.spawnController(this.element, SearchResultsUi, [this]);
  }
  
  onStateChange() {
    const newModel = SearchModel.fromState(this.stateService.state);
    if (newModel.equivalent(this.searchModel)) return;
    this.searchForm.populateWithQuery(newModel);
    this.search(newModel);
  }
  
  search(model) {
    this.searchModel = model;
    this.dbService.query(this.searchModel.toQuery({
      limit: this.searchResults.PAGE_SIZE,
      detail: "blobs",
    })).then(rsp => {
      this.searchResults.pagep = this.searchModel.page;
      this.searchResults.setResults(rsp.results, rsp.pageCount);
      this.stateService.patchState(this.searchModel.toState());
    }).catch(e => {
      console.log(`search failed!`, e);
    });
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

SearchUi.TAB_LABEL = "Search";
