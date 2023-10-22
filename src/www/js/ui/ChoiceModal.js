/* ChoiceModal.js
 * Generic modal for simple answers eg "Really do the thing you just told me to do?"
 */
 
import { Dom } from "../Dom.js";

export class ChoiceModal {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.resolve = null;
    this.reject = null;
  }
  
  onRemoveFromDom() {
    this.reject?.();
  }
  
  /* Returns a Promise that resolves with the selected option string, or rejects if we get cancelled.
   * You are only allowed to do this once.
   */
  setup(prompt, options) {
    if (this.resolve || this.reject) return Promise.reject("Multiple setup of ChoiceModal");
    const promise = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", ["prompt"], prompt);
    const buttonRow = this.dom.spawn(this.element, "DIV", ["buttonRow"]);
    for (const option of options) {
      this.dom.spawn(buttonRow, "INPUT", { type: "button", value: option, "on-click": () => {
        const resolve = this.resolve;
        this.resolve = null;
        this.reject = null;
        resolve?.(option);
        this.dom.dismissModalByController(this);
      }});
    }
    return promise;
  }
}
