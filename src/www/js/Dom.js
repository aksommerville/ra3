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
    
    // We listen for the Escape key, to dismiss modals.
    this.keyDownListener = event => this.onKeyDown(event);
    this.window.addEventListener("keydown", this.keyDownListener);
    
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
  
  spawnModal(clazz, overrides) {
    const container = this.requireModalBlotter();
    const modal = this.spawn(container, "DIV", ["modal"]);
    const controller = this.spawnController(modal, clazz, overrides);
    return controller;
  }
  
  dismissAllModals() {
    for (const container of this.document.querySelectorAll(".modalContainer")) {
      container.remove();
    }
    this.document.querySelector(".modalBlotter")?.remove();
  }
  
  dismissModalByController(controller) {
    const modal = this.document.querySelector(`.modalContainer > .modal > .${controller.constructor.name}`);
    if (!modal) return false;
    const container = modal.parentNode.parentNode;
    container.remove();
    this.adjustModalBlotterPosition();
  }
  
  dismissTopModal() {
    const containers = Array.from(this.document.querySelectorAll(".modalContainer"));
    if (!containers.length) return false;
    containers[containers.length - 1].remove();
    this.adjustModalBlotterPosition();
    return true;
  }
  
  findModalByControllerClass(cls) {
    const modal = this.document.querySelector(`.modalContainer > .modal > .${cls.name}`);
    if (!modal) return null;
    return modal.__ra_controller;
  }
  
  requireModalBlotter() {
    let blotter = this.document.querySelector(".modalBlotter");
    if (blotter) {
      this.document.body.insertBefore(blotter, null); // move to top
    } else {
      blotter = this.spawn(this.document.body, "DIV", ["modalBlotter"], { "on-click": () => this.dismissTopModal() });
    }
    const container = this.spawn(this.document.body, "DIV", ["modalContainer"]);
    return container;
  }
  
  adjustModalBlotterPosition() {
    const blotter = this.document.querySelector(".modalBlotter");
    if (!blotter) return;
    const containers = Array.from(this.document.querySelectorAll(".modalContainer"));
    if (containers.length > 0) {
      this.document.body.insertBefore(blotter, containers[containers.length - 1]);
    } else {
      blotter.remove();
    }
  }
  
  onKeyDown(e) {
    if (e.code !== "Escape") return;
    if (this.dismissTopModal()) {
      e.stopPropagation();
      e.preventDefault();
      return;
    }
  }
  
  onMutation(e) {
    for (const event of e) {
      for (const node of event.removedNodes) {
        this.notifyRemovalDeep(node);
      }
    }
  }
  
  notifyRemovalDeep(parent) {
    if (parent.childNodes) {
      for (const child of parent.childNodes) this.notifyRemovalDeep(child);
    }
    if (parent.__ra_controller) {
      const controller = parent.__ra_controller;
      parent.__ra_controller = null;
      if (controller.onRemoveFromDom) controller.onRemoveFromDom();
    }
  }
}

Dom.singleton = true;
