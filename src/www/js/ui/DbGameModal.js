/* DbGameModal.js
 * Like all the Db*Modal, we are designed for the literal database view. Not the pretty way.
 */
 
import { Dom } from "../Dom.js";

export class DbGameModal {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.onSave = (record) => {};
    
    this.record = null;
    
    this.buildUi();
  }
  
  static reprRecordForList(record) {
    return JSON.stringify(record);//TODO
  }
  
  setupNew() {
    this.record = {
      //TODO
    };
    this.populateUi();
  }
  
  setupEdit(record) {
    this.record = record;
    this.populateUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
  }
  
  populateUi() {
    //TODO
  }
}
