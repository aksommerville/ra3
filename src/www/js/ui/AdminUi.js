/* AdminUi.js
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";
 
export class AdminUi {
  static getDependencies() {
    return [HTMLElement, Dom, Comm];
  }
  constructor(element, dom, comm) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "Auto Screencap", "on-click": () => this.onAutoScreencap() });
  }
  
  onAutoScreencap() {
    console.log(`Generating screencaps...`);
    this.comm.httpJson("POST", "/api/autoscreencap").then(rsp => {
      console.log(`...success`, rsp);
    }).catch(error => {
      console.log(`...failure`, error);
    });
  }
}

AdminUi.TAB_LABEL = "Admin";
