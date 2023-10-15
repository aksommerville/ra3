/* Dom.js
 */
 
import { Injector } from "./Injector.js";

export class Dom {
  static getDependencies() {
    return [Injector, Window, Document];
  }
  constructor(injector, window, document) {
    this.injector = injector;
    this.window = window;
    this.document = document;
    
    this.mutationObserver = new this.window.MutationObserver(e => this.onMutation(e));
    this.mutationObserver.observe(document.body, {
      subtree: true,
      childList: true,
    });
  }
  
  /* (args) may contain:
   *  - scalar => innerText.
   *  - array => css classes.
   *  - object => HTML attributes, or ("on-*":function) for event listeners.
   * This creates plain DOM elements, no attached controller.
   */
  spawn(parent, tagName, ...args) {
    const element = this.document.createElement(tagName);
    for (const arg of args) {
      if (!arg || (typeof(arg) !== "object")) {
        element.innerText = arg;
      } else if (arg instanceof Array) {
        for (const cssClass of arg) {
          element.classList.add(cssClass);
        }
      } else {
        for (const k of Object.keys(arg)) {
          if (k.startsWith("on-")) {
            element.addEventListener(k.substring(3), arg[k]);
          } else {
            element.setAttribute(k, arg[k]);
          }
        }
      }
    }
    parent.appendChild(element);
    return element;
  }
  
  spawnController(parent, clazz, overrides) {
    const element = this.spawn(parent, this.tagNameForControllerClass(clazz));
    const controller = this.injector.get(clazz, [...overrides || [], element]);
    element.__ra_controller = controller;
    element.classList.add(clazz.name);
    return controller;
  }
  
  tagNameForControllerClass(clazz) {
    if (!clazz.getDependencies) return "DIV";
    for (const dc of clazz.getDependencies()) {
      const match = dc.name?.match(/^HTML([a-zA-Z0-9]*)Element$/);
      if (!match) continue;
      switch (match[1]) {
        case "Div": return "DIV";
        case "Canvas": return "CANVAS";
        case "Input": return "INPUT";
        // Some others like LI are weird, so don't just toUpperCase.
        // And it's OK not to match, we can pick these off as we encounter them.
      }
    }
    return "DIV";
  }
  
  //TODO modals
  
  onMutation(e) {
    for (const event of e) {
      for (const node of event.removedNodes) {
        if (node.__ra_controller) {
          const controller = node.__ra_controller;
          node.__ra_controller = null;
          if (controller.onRemoveFromDom) controller.onRemoveFromDom();
        }
      }
    }
  }
}

Dom.singleton = true;
