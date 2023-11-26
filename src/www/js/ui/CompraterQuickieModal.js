/* CompraterQuickieModal.js
 * Like GameDetailsModal but designed for CompraterUi.
 * Enter a rating from the keyboard, and when you strike the second digit, we commit and dismiss.
 */
 
import { Dom } from "../Dom.js";
import { GameDetailsModal } from "./GameDetailsModal.js";
import { Comm } from "../Comm.js";
import { DbService } from "../model/DbService.js";

export class CompraterQuickieModal {
  static getDependencies() {
    return [HTMLElement, Dom, Comm, DbService];
  }
  constructor(element, dom, comm, dbService) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    this.dbService = dbService;
    
    this.onChanged = game => {};
    
    this.game = null;
    this.digit = 10; // 10 or 1
    this.rating = 0;
    
    this.buildUi();
  }
  
  setupFull(game) {
    this.game = {...game};
    this.populate();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.element.setAttribute("contentEditable", "true");
    this.element.addEventListener("keydown", e => this.onKeyDown(e));
    this.dom.spawn(this.element, "H1", ["name"]);
    const rating = this.dom.spawn(this.element, "DIV", ["rating"]);
    this.dom.spawn(rating, "SPAN", ["digit10", "cursor"], ".");
    this.dom.spawn(rating, "SPAN", ["digit1"], ".");
    this.dom.spawn(this.element, "IMG", ["thumbnail"]);
    const buttonsRow = this.dom.spawn(this.element, "DIV", ["buttonsRow"]);
    this.dom.spawn(buttonsRow, "INPUT", { type: "button", value: "Details", "on-click": () => this.openFullDetails() });
    this.dom.spawn(buttonsRow, "INPUT", { type: "button", value: "Launch", "on-click": () => this.launch() });
    this.dom.spawn(this.element, "DIV", ["hint"], "Hint: Shift-click thumbnails to open full details.");
  }
  
  populate() {
    this.element.querySelector(".name").innerText = this.game.name;
    this.element.querySelector(".rating .digit10").innerText = Math.floor((this.game.rating || 0) / 10);
    this.element.querySelector(".rating .digit1").innerText = (this.game.rating || 0) % 10;
    this.element.querySelector(".thumbnail").src = this.composeThumbnailUrl(this.game);
    this.element.focus();
  }
  
  composeThumbnailUrl(game) {
    if (!game?.blobs) return "";
    const candidates = game.blobs.filter(path => path.match(/-scap-.*\.png$/));
    if (candidates.length < 1) return "";
    const choice = Math.floor(Math.random() * candidates.length);
    return `/api/blob?path=${candidates[choice]}`;
  }
  
  onKeyDown(event) {
    event.stopPropagation();
    event.preventDefault();
    if (!this.game || (this.digit < 1)) return;
    if (!event.code.startsWith("Digit")) return;
    const v = +event.code.substring(5);
    if (isNaN(v) || (v < 0) || (v > 9)) return;
    this.rating += v * this.digit;
    this.digit /= 10;
    if (this.digit < 1) {
      this.dbService.patchGame({ gameid: this.game.gameid, rating: this.rating }, "id").then(() => {
        this.onChanged({ ...this.game, rating: this.rating });
        this.dom.dismissModalByController(this);
      }).catch(e => console.error(`Failed to update rating.`, e));
    } else if (this.digit === 1) {
      const digit10 = this.element.querySelector(".digit10");
      digit10.classList.remove("cursor");
      digit10.innerText = Math.floor(this.rating / 10);
      const digit1 = this.element.querySelector(".digit1");
      digit1.classList.add("cursor");
    }
  }
  
  openFullDetails() {
    const modal = this.dom.spawnModal(GameDetailsModal);
    modal.setupFull(this.game);
    modal.onChanged = (game) => this.setupFull(game);
  }
  
  launch() {
    this.comm.http("POST", `/api/launch?gameid=${this.game.gameid}`).then(() => {
    }).catch(error => {
      console.log("launch failed", error);
    });
  }
}
