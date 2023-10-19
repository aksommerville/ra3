/* DbLauncherModal.js
 * Like all the Db*Modal, we are designed for the literal database view. Not the pretty way.
 */
 
import { Dom } from "../Dom.js";

export class DbLauncherModal {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.onSave = (record) => Promise.resolve();
    
    this.record = null;
    
    this.buildUi();
  }
  
  static reprRecordForList(record) {
    return JSON.stringify(record);//TODO
  }
  
  setupNew() {
    this.record = {
      id: 0,
      name: "",
      platform: "",
      suffixes: "",
      cmd: "",
      desc: "",
    };
    this.populateUi();
  }
  
  setupEdit(record) {
    this.record = record;
    this.populateUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const errorMessage = this.dom.spawn(this.element, "DIV", ["errorMessage", "hidden"]);
    const form = this.dom.spawn(this.element, "FORM", { action: "#", method: "", "on-submit": e => this.onSubmit(e) });
    const table = this.dom.spawn(form, "TABLE");
    this.spawnStringRow(table, "id").disabled = true;
    const firstInput = this.spawnStringRow(table, "name");
    this.spawnStringRow(table, "platform"); // TODO datalist?
    this.spawnStringRow(table, "suffixes");
    this.spawnStringRow(table, "cmd");
    this.spawnStringRow(table, "desc");
    this.dom.spawn(form, "INPUT", { type: "submit", value: "Save" });
  }
  
  spawnStringRow(table, k) {
    const tr = this.dom.spawn(table, "TR", { "data-k": k });
    this.dom.spawn(tr, "TD", ["key"], k);
    const trv = this.dom.spawn(tr, "TD", ["value"]);
    const input = this.dom.spawn(trv, "INPUT", { type: "text", name: k });
    return input;
  }
  
  populateUi() {
    for (const k of Object.keys(this.record)) {
      const input = this.element.querySelector(`input[name='${k}']`);
      if (input) {
        input.value = this.record[k];
      } else {
        console.log(`DbLauncherModal: Unexpected field '${k}'`);
      }
    }
    const firstInput = this.element.querySelector("input[name='name']");
    firstInput.select();
    firstInput.focus();
  }
  
  onSubmit(e) {
    e.preventDefault();
    const v = Array.from(this.element.querySelector("FORM")).reduce((a, v) => {
      if (v.name) a[v.name] = v.value;
      return a;
    }, {
      id: this.record.id,
    });
    v.id = +v.id;
    this.onSave(v).then(() => this.dom.dismissModalByController(this))
    .catch(e => this.showError(e));
  }
  
  showError(e) {
    const container = this.element.querySelector(".errorMessage");
    container.innerText = e?.message || JSON.stringify(e);
    container.classList.remove("hidden");
  }
}
