/* SearchFlagsUi.js
 * Array of toggles, one for each game flag.
 */
 
import { Dom } from "../Dom.js";
import { DbService } from "../model/DbService.js";

export class SearchFlagsUi {
  static getDependencies() {
    return [HTMLElement, Dom, DbService, "discriminator"];
  }
  constructor(element, dom, dbService, discriminator) {
    this.element = element;
    this.dom = dom;
    this.dbService = dbService;
    this.discriminator = discriminator;
    
    this.onChange = () => {};
    
    this.yesFlags = 0;
    this.noFlags = 0;
    this.mode = "yes/no/none"; // "yes/no/none" or "on/off"
    
    this.buildUi();
  }
  
  setValue(flags, notflags) {
    if (flags) for (const name of flags.split(/\s+/)) {
      const bitix = DbService.FLAG_NAMES.indexOf(name);
      if (bitix < 0) continue;
      const input = this.element.querySelector(`.toggle[data-bitix='${bitix}']`);
      if (!input) continue;
      input.classList.add("on");
    }
    if (notflags) for (const name of notflags.split(/\s+/)) {
      const bitix = DbService.FLAG_NAMES.indexOf(name);
      if (bitix < 0) continue;
      const input = this.element.querySelector(`.toggle[data-bitix='${bitix}']`);
      if (!input) continue;
      input.classList.add("off");
    }
  }
  
  setMode(mode) {
    if (mode === this.mode) return;
    switch (mode) {
      case "yes/no/none":
      case "on/off":
        break;
      default: throw new Error(`Invalid mode ${JSON.stringify(mode)} for SearchFlagsUi. Expected 'yes/no/none' or 'on/off'`);
    }
    this.mode = mode;
    this.refreshToggleClasses();
  }
  
  encodeFlags() {
    return Array.from(this.element.querySelectorAll(".toggle.on")).map(e => DbService.FLAG_NAMES[+e.getAttribute("data-bitix")]).join(" ");
  }
  
  encodeNotFlags() {
    return Array.from(this.element.querySelectorAll(".toggle.off")).map(e => DbService.FLAG_NAMES[+e.getAttribute("data-bitix")]).join(" ");
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.spawnToggle( "0", "1");
    this.spawnToggle( "1", "2");
    this.spawnToggle( "2", "3");
    this.spawnToggle( "3", "4");
    this.spawnToggle( "4", "+");
    this.spawnToggle( "5", "\u2620"); // faulty (skull)
    this.spawnToggle( "6", "\u03c6"); // foreign (hiragana a)
    this.spawnToggle( "7", "\u267b"); // hack (recycling)
    this.spawnToggle( "8", "\u26cf"); // hardware (pickax)
    this.spawnToggle( "9", "?"); // review
    this.spawnToggle("10", "\u2623"); // obscene (biohazard)
    this.spawnToggle("11", "\u2605"); // favorite (star)
    this.spawnToggle("12", "\u21a9"); // seeother (bending arrow)
  }
  
  spawnToggle(bitix, display) {
    const input = this.dom.spawn(this.element, "INPUT", ["toggle"], {
      type: "button",
      "data-bitix": bitix,
      value: display,
      "on-click": () => this.toggle(bitix),
      title: DbService.FLAG_NAMES[+bitix] || "",
      tabindex: -1,
    });
  }
  
  toggle(bitix) {
    const input = this.element.querySelector(`input[data-bitix='${bitix}']`);
    const mask = 1 << +bitix;
    switch (this.mode) {
      case "yes/no/none": {
          if (this.noFlags & mask) {
            this.noFlags &= ~mask;
            input.classList.remove("off");
          } else if (this.yesFlags & mask) {
            this.yesFlags &= ~mask;
            this.noFlags |= mask;
            input.classList.remove("on");
            input.classList.add("off");
          } else {
            this.yesFlags |= mask;
            input.classList.add("on");
          }
        } break;
      case "on/off": {
          if (this.yesFlags & mask) {
            this.yesFlags &= ~mask;
            input.classList.remove("on");
          } else {
            this.yesFlags |= mask;
            input.classList.add("on");
          }
        } break;
      default: return;
    }
    this.onChange();
  }
  
  refreshToggleClasses() {
    switch (this.mode) {
      case "yes/no/none": {
          for (const input of this.element.querySelectorAll("input[data-bitix]")) {
            const mask = 1 << +input.getAttribute("data-bitix");
            if (this.noFlags & mask) {
              input.classList.remove("on");
              input.classList.add("off");
            } else if (this.yesFlags & mask) {
              input.classList.add("on");
              input.classList.remove("off");
            } else {
              input.classList.remove("on");
              input.classList.remove("off");
            }
          }
        } break;
      case "on/off": {
          for (const input of this.element.querySelectorAll("input[data-bitix]")) {
            const mask = 1 << +input.getAttribute("data-bitix");
            input.classList.remove("off");
            if (this.yesFlags & mask) {
              input.classList.add("on");
            } else {
              input.classList.remove("off");
            }
          }
        } break;
    }
  }
}
