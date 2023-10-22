/* LoHiUi.js
 * Two input fields representing low and high edges of a range of integers.
 * Zero is always a legal value if unset.
 */
 
import { Dom } from "../Dom.js";

export class LoHiUi {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.onChange = () => {};
    this.loLimit = 0;
    this.hiLimit = 0xffff;
    
    this.buildUi();
  }
  
  setRange(lo, hi, caption) {
    this.loLimit = lo;
    this.hiLimit = hi;
    this.element.querySelector(".caption").innerText = caption || "";
  }
  
  getRangeAsString() {
    const lo = this.element.querySelector("input[name='lo']").value;
    const hi = this.element.querySelector("input[name='hi']").value;
    if (!lo && !hi) return "";
    return `${lo}..${hi}`;
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "SPAN", ["caption"], "Value:");
    this.dom.spawn(this.element, "INPUT", { type: "number", name: "lo", "on-input": () => this.onChange() });
    this.dom.spawn(this.element, "SPAN", "thru");
    this.dom.spawn(this.element, "INPUT", { type: "number", name: "hi", "on-input": () => this.onChange() });
  }
}
