/* NowPlayingUi.js
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";
 
export class NowPlayingUi {
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
    //TODO WebSocket to backend, and show what's running.
    //TODO Button to pop up details for running game.
    //TODO Screencap
    //TODO Pause/Resume
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "Terminate", "on-click": () => this.onTerminate() });
  }
  
  onTerminate() {
    this.comm.http("POST", "/api/terminate");
  }
}
