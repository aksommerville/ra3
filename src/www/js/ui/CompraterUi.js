/* CompraterUi.js
 * Show a window of rating values, and all the games in range, to fine-tune ratings comparatively.
 */
 
import { Dom } from "../Dom.js";
import { DbService } from "../model/DbService.js";
import { GameDetailsModal } from "./GameDetailsModal.js";
import { CompraterQuickieModal } from "./CompraterQuickieModal.js";

export class CompraterUi {
  static getDependencies() {
    return [HTMLElement, Dom, DbService];
  }
  constructor(element, dom, dbService) {
    this.element = element;
    this.dom = dom;
    this.dbService = dbService;
    
    this.ratingCount = 12; // Window size, constant.
    this.rating = 50 - (this.ratingCount >> 1); // Low end of window.
    this.thumbnailCount = 10; // How many rows. Adjusts dynamically.
    this.buckets = []; // Indexed by numeric rating, sparse. Each member is an Array of Game.
    this.scroll = []; // Parallel to buckets.
    
    this.loadAll();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const yslop = 80; // Accomodate header and up/down buttons
    const thumbnailHeight = 100; // Constant per CSS.
    this.thumbnailCount = Math.max(1, Math.floor((this.element.offsetHeight - yslop) / thumbnailHeight));
    
    /* Bottom row has control panels fore and aft, and a header for each bucket in play.
     */
    const leftNav = this.dom.spawn(this.element, "DIV", ["nav"]);
    this.dom.spawn(leftNav, "INPUT", { type: "button", value: "< 1", "on-click": () => this.nav(-1) });
    this.dom.spawn(leftNav, "INPUT", { type: "button", value: "< 10", "on-click": () => this.nav(-10) });
    for (let i=0; i<this.ratingCount; i++) {
      const rating = this.rating + i;
      const column = this.dom.spawn(this.element, "DIV", ["bucketColumn"]);
      if (!i) column.classList.add("left");
      this.populateColumn(column, rating);
    }
    const rightNav = this.dom.spawn(this.element, "DIV", ["nav"]);
    this.dom.spawn(rightNav, "INPUT", { type: "button", value: "1 >", "on-click": () => this.nav(1) });
    this.dom.spawn(rightNav, "INPUT", { type: "button", value: "10 >", "on-click": () => this.nav(10) });
  }
  
  populateColumn(column, rating) {
    this.dom.spawn(column, "DIV", ["header"], rating);
    const bucket = this.buckets[rating];
    if (!bucket) return;
    const scroll = this.scroll[rating] || 0;
    
    if (bucket.length > this.thumbnailCount) {
      // Create the lower scroll button always (instead of `if (scroll) ...`), so the column doesn't move when you scroll up the first time.
      this.dom.spawn(column, "INPUT", { type: "button", value: `vvv ${scroll} vvv`, "on-click": () => this.scrollColumn(rating, -1, column) });
    }
    for (let i=0; i<this.thumbnailCount; i++) {
      const p = scroll + i;
      if (p >= bucket.length) break;
      const game = bucket[p];
      const thumbnail = this.dom.spawn(column, "DIV", ["thumbnail"], { "on-click": (event) => this.onClickGame(game, event) });
      this.dom.spawn(thumbnail, "DIV", ["name"], this.truncateName(game.name));
      const imgurl = this.composeThumbnailUrl(game);
      if (imgurl) this.dom.spawn(thumbnail, "IMG", { src: imgurl, title: game.name || "" });
    }
    if (scroll + this.thumbnailCount < bucket.length) {
      this.dom.spawn(column, "INPUT", { type: "button", value: `^^^ ${bucket.length - this.thumbnailCount - scroll} ^^^`, "on-click": () => this.scrollColumn(rating, 1, column) });
    }
  }
  
  truncateName(name) {
    if (!name) return "---";
    if (name.length > 15) return name.substring(0, 12) + "...";
    return name;
  }
  
  composeThumbnailUrl(game) {
    if (!game?.blobs) return "";
    const candidates = game.blobs.filter(path => path.match(/-scap-.*\.png$/));
    if (candidates.length < 1) return "";
    const choice = Math.floor(Math.random() * candidates.length);
    return `/api/blob?path=${candidates[choice]}`;
  }
  
  scrollColumn(rating, d, column) {
    let nv = (this.scroll[rating] || 0) + d;
    if (nv < 0) nv = 0;
    this.scroll[rating] = nv;
    column.innerHTML = "";
    this.populateColumn(column, rating);
  }
  
  nav(d) {
    // Optimization: If scrolling by one, just drop the one and request the other.
    // Any other delta, reload from scratch.
    if (d === -1) {
      this.rating--;
      delete this.buckets[this.rating + this.ratingCount];
      this.loadOne(this.rating);
    } else if (d === 1) {
      delete this.buckets[this.rating];
      this.rating++;
      this.loadOne(this.rating + this.ratingCount - 1);
    } else {
      this.rating += d;
      this.buckets = [];
      this.loadAll();
    }
  }
  
  onClickGame(game, event) {
    // It's easy enough to make CompraterQuickieModal export the same interface as GameDetailsModal to keep this clean:
    let clazz = CompraterQuickieModal;
    if (event.shiftKey) clazz = GameDetailsModal;
    const modal = this.dom.spawnModal(CompraterQuickieModal);
    modal.setupFull(game);
    modal.onChanged = (changed) => {
      this.loadOne(game.rating);
      if ((game.rating !== changed.rating) && (changed.rating >= this.rating) && (changed.rating < this.rating + this.ratingCount)) {
        this.loadOne(changed.rating);
      }
    };
  }
  
  /* Sort this array of games into (this.buckets).
   * Appends only! You'll want to clear content before calling, usually.
   */
  receiveResults(results) {
    for (const game of results) {
      const rating = game.rating || 0;
      let bucket = this.buckets[rating];
      if (!bucket) this.buckets[rating] = bucket = [];
      bucket.push(game);
    }
    this.buildUi();
  }
  
  loadOne(rating) {
    this.dbService.query({
      rating: rating,
      limit: 1000,
      detail: "blobs",
    }).then(results => {
      delete this.buckets[rating];
      this.receiveResults(results.results);
    }).catch(error => {
      console.error(`CompraterUi search failed`, error);
    });
  }
  
  loadAll() {
    this.dbService.query({
      rating: `${this.rating}..${this.rating + this.ratingCount - 1}`,
      limit: 1000,
      detail: "blobs",
    }).then(results => {
      this.buckets = [];
      this.receiveResults(results.results);
    }).catch(error => {
      console.error(`CompraterUi search failed`, error);
    });
  }
}
