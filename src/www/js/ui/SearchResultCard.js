/* SearchResultCard.js
 * One item in search results.
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";
import { GameDetailsModal } from "./GameDetailsModal.js";

export class SearchResultCard {
  static getDependencies() {
    return [HTMLElement, Dom, Comm];
  }
  constructor(element, dom, comm) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    
    this.game = null;
    
    this.buildUi();
  }
  
  setGame(game) {
    this.game = game;
    this.populate();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const body = this.dom.spawn(this.element, "DIV", ["cardBody"]);
    this.dom.spawn(body, "H3", ["name"]);
    const imageRow = this.dom.spawn(body, "DIV", ["imageRow"]);
    this.dom.spawn(imageRow, "IMG", ["thumbnail"]);
    const imageRowText = this.dom.spawn(imageRow, "DIV");
    const ratingRow = this.dom.spawn(imageRowText, "DIV", ["ratingRow"]);
    this.dom.spawn(ratingRow, "DIV", ["rating"]);
    this.dom.spawn(ratingRow, "DIV", ["banner"]);
    this.dom.spawn(imageRowText, "DIV", ["platform"]);
    this.dom.spawn(imageRowText, "DIV", ["author"]);
    this.dom.spawn(imageRowText, "DIV", ["pubtime"]);
    const buttonsRow = this.dom.spawn(imageRowText, "DIV", ["buttonsRow"]);
    this.dom.spawn(buttonsRow, "INPUT", { type: "button", value: "Details", "on-click": () => this.editDetails() });
    this.dom.spawn(buttonsRow, "INPUT", { type: "button", value: "&", "on-click": () => { this.launch(); this.editDetails(); } });
    this.dom.spawn(buttonsRow, "INPUT", { type: "button", value: "Launch", "on-click": () => this.launch() });
    this.dom.spawn(body, "DIV", ["desc"]);
  }
  
  populate() {
  
    // Add badges for lists. It's not OK generally for this code to know the set of lists, it's a (hopefully temporary) cudgel while I refine my collection.
    const header = this.element.querySelector(".name");
    header.innerHTML = "";
    this.dom.spawn(header, "SPAN", ["text"], this.game?.name || "");
    for (const { name } of this.game?.lists || []) {
      this.dom.spawn(header, "IMG", { src: this.iconUrlForListName(name), title: name });
    }
    
    this.element.querySelector(".thumbnail").src = this.composeThumbnailUrl();
    this.element.querySelector(".rating").innerText = this.game?.rating || "0";
    this.element.querySelector(".ratingRow .banner").style.backgroundColor = this.composeRatingColor(+this.game?.rating || 0);
    this.element.querySelector(".platform").innerText = this.game?.platform || "";
    this.element.querySelector(".author").innerText = this.game?.author || "";
    this.element.querySelector(".pubtime").innerText = this.game?.pubtime || "";
    this.element.querySelector(".desc").innerText = this.composeDesc();
  }
  
  iconUrlForListName(name) {
    switch (name) {
      case "Andy's Picks": return "/img/list-andy.png";
      case "Childhood": return "/img/list-child.png";
      case "Backseat Driver": return "/img/list-backseat.png";
      case "Karen": return "/img/list-karen.png";
      case "Ken": return "/img/list-ken.png";
      case "Ken & Karen": return "/img/list-knk.png";
      case "Famous": return "/img/list-famous.png";
      default: return "/img/list-other.png";
    }
  }
  
  composeThumbnailUrl() {
    if (!this.game?.blobs) return "";
    const candidates = this.game.blobs.filter(path => path.match(/-scap-.*\.png$/));
    if (candidates.length < 1) return "";
    const choice = Math.floor(Math.random() * candidates.length);
    return `/api/blob?path=${candidates[choice]}`;
  }
  
  composeRatingColor(rating) {
    if (rating < 1) return "#888";
    if (rating > 99) rating = 99;
    const scale = Math.floor((rating * 255) / 100);
    return `rgb(${255 - scale}, ${scale}, 0)`;
  }
  
  composeDesc() {
    let dst = "";
    for (const { k, v } of this.game.comments || []) {
      if (!v) continue;
      if (k !== "text") continue;
      dst += v + "\n";
    }
    return dst;
  }
  
  editDetails() {
    if (!this.game) return;
    const modal = this.dom.spawnModal(GameDetailsModal);
    modal.setupFull(this.game);
    modal.onChanged = game => this.setGame(game);
  }
  
  launch() {
    if (!this.game) return;
    this.comm.http("POST", `/api/launch?gameid=${this.game.gameid}`).then(() => {
    }).catch(error => {
      console.log("launch failed", error);
    });
  }
}
