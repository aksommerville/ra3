import { Injector } from "./js/Injector.js";
import { Dom } from "./js/Dom.js";
import { RootUi } from "./js/ui/RootUi.js";

window.addEventListener("load", () => {
  const injector = new Injector(window, document);
  const dom = injector.get(Dom);
  const root = dom.spawnController(document.body, RootUi);
});
