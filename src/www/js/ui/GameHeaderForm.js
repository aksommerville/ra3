/* GameHeaderForm.js
 * Part of GameDetailsModal.
 */
 
import { Dom } from "../Dom.js";
import { GameDetailsModal } from "./GameDetailsModal.js";
import { DbService } from "../model/DbService.js";
import { SearchFlagsUi } from "./SearchFlagsUi.js";

export class GameHeaderForm {
  static getDependencies() {
    return [HTMLElement, Dom, GameDetailsModal, DbService];
  }
  constructor(element, dom, gameDetailsModal, dbService) {
    this.element = element;
    this.dom = dom;
    this.gameDetailsModal = gameDetailsModal;
    this.dbService = dbService;
    
    // Remains constant (doesn't match UI, after user changes something).
    this.game = null;
    
    this.searchFlagsUi = null;
    this.autofocus = true; // we'll only do it once (we get repopulated after saves, no focus change then)
    
    this.buildUi();
  }
  
  setGame(game) {
    this.game = game;
    this.populate();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const table = this.dom.spawn(this.element, "TABLE", { "on-input": () => this.gameDetailsModal.dirty() });
    
    /* Subjective priority.
     * (and it matters ergonmically, because I'm going to hit this form thousands of times to populate the database).
     *   - gameid first, and it's immutable so whatever.
     *   - name: First mutable field. Feels right, since this form doesn't have like a title banner. Also the initial value is usually wrong.
     *   - pubtime, author: Typically revealed before you press start.
     *   - genre: Not always known before play, but doesn't take long.
     *   - rating, flags: Typically can't be determined until after a bit of play.
     *   - flags are inconvenient to edit with a keyboard, so they form a nice breaking point. Visually too; they're a horizontal line.
     *   - platform, path: Should never need edited. In v2 we even locked them behind a "edit everything" button. Do that here too?
     */
    this.spawnReadonly(table, "gameid");
    this.spawnText(table, "name");
    this.spawnText(table, "pubtime");
    this.spawnDatalist(table, "author", () => this.dbService.suggestAuthors());
    this.spawnDatalist(table, "genre", () => this.dbService.suggestGenres());
    this.spawnNumber(table, "rating");
    this.spawnFlags(table);
    this.spawnDatalist(table, "platform", () => this.dbService.suggestPlatforms());
    this.spawnText(table, "path");
  }
  
  spawnReadonly(table, k) {
    const tdv = this.spawnRow(table, k);
    return this.dom.spawn(tdv, "INPUT", { type: "text", name: k, disabled: "disabled" });
  }
  
  spawnText(table, k) {
    const tdv = this.spawnRow(table, k);
    return this.dom.spawn(tdv, "INPUT", { type: "text", name: k });
  }
  
  spawnDatalist(table, k, fetch) {
    const tdv = this.spawnRow(table, k);
    const id = `GameHeaderForm-${this.discriminator}-${k}`;
    const datalist = this.dom.spawn(tdv, "DATALIST", { id });
    fetch().then(options => {
      datalist.innerHTML = "";
      for (const option of options) {
        this.dom.spawn(datalist, "OPTION", { value: option });
      }
    });
    return this.dom.spawn(tdv, "INPUT", { type: "text", name: k, list: id });
  }
  
  spawnNumber(table, k) {
    const tdv = this.spawnRow(table, k);
    return this.dom.spawn(tdv, "INPUT", { type: "number", name: k });
  }
  
  spawnFlags(table) {
    const tdv = this.spawnRow(table, "flags");
    this.searchFlagsUi = this.dom.spawnController(tdv, SearchFlagsUi);
    this.searchFlagsUi.setMode("on/off");
    this.searchFlagsUi.onChange = () => this.gameDetailsModal.dirty();
  }
  
  spawnRow(table, k) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["key"], k);
    return this.dom.spawn(tr, "TD", ["value"]);
  }
  
  populate() {
    for (const input of this.element.querySelectorAll("td.value > input")) {
      if (!input.name) continue;
      input.value = this.game[input.name] || "";
    }
    this.searchFlagsUi.setValue(this.game.flags);
    if (this.autofocus) {
      this.autofocus = false;
      const autofocus = this.element.querySelector("input[name='name']");
      autofocus.select();
      autofocus.focus();
    }
  }
  
  readModelFromUi() {
    const game = {};
    for (const input of this.element.querySelectorAll("td.value > input")) {
      if (!input.name) continue;
      game[input.name] = input.value;
    }
    game.rating = +game.rating;
    game.flags = this.searchFlagsUi.encodeFlags();
    return game;
  }
  
  setSaveInProgress(sip) {
  }
  
  gatherChanges() {
    const input = this.readModelFromUi();
    if (this.headersEquivalent(input, this.game)) return [];
    return [{ action: "header", body: input }];
  }
  
  headersEquivalent(a, b) {
    // Don't bother checking gameid, we don't let it change.
    if ((a.author || "") !== (b.author || "")) return false;
    if ((a.flags || "") !== (b.flags || "")) return false; // false positives possible here, but i've tried to format them the same way everywhere.
    if ((a.genre || "") !== (b.genre || "")) return false;
    if ((a.path || "") !== (b.path || "")) return false;
    if ((a.platform || "") !== (b.platform || "")) return false;
    if ((a.pubtime || "") !== (b.pubtime || "")) return false;
    if ((a.rating || 0) !== (b.rating || 0)) return false;
    if ((a.name || "") !== (b.name || "")) return false;
    return true;
  }
}
