/* DbUi.js
 */
 
import { Dom } from "../Dom.js";
import { DbTableUi } from "./DbTableUi.js";
 
export class DbUi {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.tableControllers = [];
    
    this.buildUi();
  }
  
  setup(path) {
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.tableControllers = [];
    for (const table of ["launcher", "list", "game", "comment", "play", "blob"]) {
      const controller = this.dom.spawnController(this.element, DbTableUi);
      controller.setTable(table);
      this.tableControllers.push(controller);
    }
  }
}
