/* SearchFormUi.js
 * Entry form for detailed game search.
 */
 
import { Dom } from "../Dom.js";
import { SearchUi } from "./SearchUi.js";
import { DbService } from "../model/DbService.js";
import { SearchFlagsUi } from "./SearchFlagsUi.js";
import { LoHiUi } from "./LoHiUi.js";

export class SearchFormUi {
  static getDependencies() {
    return [HTMLElement, Dom, SearchUi, Window, "discriminator", DbService];
  }
  constructor(element, dom, searchUi, window, discriminator, dbService) {
    this.element = element;
    this.dom = dom;
    this.searchUi = searchUi;
    this.window = window;
    this.discriminator = discriminator;
    this.dbService = dbService;
    
    this.SEARCH_DEBOUNCE_TIME = 1500;
    
    this.searchTimeout = null;
    this.flagsController = null;
    this.ratingController = null;
    this.pubtimeController = null;
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    if (this.searchTimeout) {
      this.window.clearTimeout(this.searchTimeout);
      this.searchTimeout = null;
    }
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": e => this.onSubmit(e) });
    this.dom.spawn(form, "INPUT", { type: "submit" }, ["hidden"]);
    this.spawnField(form, "text");
    this.spawnField(form, "list"); // API allows multiple lists, should we support that?
    this.spawnField(form, "platform");
    this.spawnField(form, "author");
    this.spawnField(form, "genre");
    this.spawnField(form, "flags");
    this.spawnField(form, "rating");
    this.spawnField(form, "pubtime");
    this.spawnField(form, "sort");
    this.element.querySelector("input[name='text']").focus();
  }
  
  spawnField(form, k) {
    const container = this.dom.spawn(form, "DIV", ["field"]);
    
    if (k === "flags") {
      this.flagsController = this.dom.spawnController(container, SearchFlagsUi);
      this.flagsController.onChange = () => this.dirty();
      return;
    }
    if (k === "rating") {
      this.ratingController = this.dom.spawnController(container, LoHiUi);
      this.ratingController.setRange(0, 100, "Rating:");
      this.ratingController.onChange = () => this.dirty();
      return;
    }
    if (k === "pubtime") {
      this.pubtimeController = this.dom.spawnController(container, LoHiUi);
      this.pubtimeController.setRange(1970, 2050, "Published:"); // remember to update me in twenty years or so -ak, 2023
      this.pubtimeController.onChange = () => this.dirty();
      return;
    }
    if (k === "sort") {
      const select = this.dom.spawn(container, "SELECT", { name: "sort", "on-input": () => this.dirty() });
      for (const mode of ["none", "id", "name", "pubtime", "rating", "playtime", "playcount", "fullness"]) {
        this.dom.spawn(select, "OPTION", { value: mode }, "+" + mode);
        this.dom.spawn(select, "OPTION", { value: "-" + mode }, "-" + mode);
      }
      return;
    }
    
    const input = this.dom.spawn(container, "INPUT", {
      type: "text",
      name: k,
      placeholder: k,
      "on-input": () => this.dirty(),
    });
    
    let dataListRetrieval = null;
    switch (k) {
      case "author": dataListRetrieval = this.dbService.suggestAuthors(); break;
      case "platform": dataListRetrieval = this.dbService.suggestPlatforms(); break;
      case "genre": dataListRetrieval = this.dbService.suggestGenres(); break;
      case "list": dataListRetrieval = this.dbService.suggestLists(); break;
    }
    if (dataListRetrieval) {
      const id = `SearchFormUi-${this.discriminator}-${k}-data`;
      input.setAttribute("list", id);
      const list = this.dom.spawn(container, "DATALIST", { id });
      dataListRetrieval.then(options => {
        for (const option of options) {
          this.dom.spawn(list, "OPTION", { value: option });
        }
      });
    }
    
    return input;
  }
  
  readQueryFromUi() {
    const query = {
      flags: this.flagsController.encodeFlags(),
      notflags: this.flagsController.encodeNotFlags(),
      rating: this.ratingController.getRangeAsString(),
      pubtime: this.pubtimeController.getRangeAsString(),
      sort: this.element.querySelector("select[name='sort']").value,
    };
    for (const input of this.element.querySelectorAll(".field > input")) {
      query[input.name] = input.value;
    }
    return query;
  }
  
  dirty() {
    this.element.classList.add("dirty");
    if (this.searchTimeout) {
      this.window.clearTimeout(this.searchTimeout);
    }
    this.searchTimeout = this.window.setTimeout(() => {
      this.searchTimeout = null;
      this.element.classList.remove("dirty");
      this.searchUi.search(this.readQueryFromUi());
    }, this.SEARCH_DEBOUNCE_TIME);
  }
  
  onSubmit(event) {
    event.preventDefault();
    if (this.searchTimeout) {
      this.window.clearTimeout(this.searchTimeout);
      this.searchTimeout = null;
      this.element.classList.remove("dirty");
    }
    this.searchUi.search(this.readQueryFromUi());
  }
}
