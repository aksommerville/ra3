/* ScreencapModal.js
 * Displays a screencap and permit user to save as a blob.
 */
 
import { Dom } from "../Dom.js";
import { DbService } from "../model/DbService.js";
import { ImageService } from "../model/ImageService.js";
import { Comm } from "../Comm.js";

export class ScreencapModal {
  static getDependencies() {
    return [HTMLElement, Dom, DbService, ImageService, Comm];
  }
  constructor(element, dom, dbService, imageService, comm) {
    this.element = element;
    this.dom = dom;
    this.dbService = dbService;
    this.imageService = imageService;
    this.comm = comm;
    
    this.binary = null;
    this.gameid = 0;
    
    this.buildUi();
  }
  
  setupPng(binary) {
    this.element.querySelector("input[value='Save']").disabled = false;
    this.binary = binary;
    this.gameid = this.comm.currentGameid; // important to capture now, in case the game ends before we save
    this.imageService.decodePng(binary)
      .then(image => this.setImage(image))
      .catch(error => console.error(`Failed to decode image`, { error, binary }));
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", ["imageContainer"]);
    const buttonsRow = this.dom.spawn(this.element, "DIV", ["buttonsRow"]);
    this.dom.spawn(buttonsRow, "INPUT", { type: "button", value: "Save", "on-click": () => this.onSave(), disabled: "disabled" });
  }
  
  setImage(image) {
    const container = this.element.querySelector(".imageContainer");
    container.innerHTML = "";
    container.appendChild(image);
  }
  
  onSave() {
    if (!this.binary) return;
    if (!this.gameid) return;
    this.element.querySelector("input[value='Save']").disabled = true;
    this.comm.http("PUT", "/api/blob", { gameid: this.gameid, type: "scap", sfx: ".png" }, null, this.binary.buffer, "text")
      .then(rsp => {
        console.log(`Saved blob: ${rsp}`);
      })
      .catch(error => {
        console.error(`Failed to save blob.`, error);
        this.element.querySelector("input[value='Save']").disabled = false;
      });
  }
}
