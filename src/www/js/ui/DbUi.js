/* DbUi.js
 */
 
import { Dom } from "../Dom.js";
import { DbTableUi } from "./DbTableUi.js";
import { DbService } from "../model/DbService.js";
 
export class DbUi {
  static getDependencies() {
    return [HTMLElement, Dom, DbService];
  }
  constructor(element, dom, dbService) {
    this.element = element;
    this.dom = dom;
    this.dbService = dbService;
    
    this.tableControllers = [];
    
    this.buildUi();
  }
  
  setup(path) {
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.tableControllers = [];
    for (const table of this.dbService.getTableNames()) {
      const controller = this.dom.spawnController(this.element, DbTableUi);
      controller.setTable(table);
      this.tableControllers.push(controller);
    }
  }
}
