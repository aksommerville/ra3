/* ListsUi.js
 */
 
import { Dom } from "../Dom.js";
import { DbService } from "../model/DbService.js";
import { QuickieSearchUi } from "./QuickieSearchUi.js";
import { GameDetailsModal } from "./GameDetailsModal.js";

export class ListsUi {
  static getDependencies() {
    return [HTMLElement, Dom, DbService, Window];
  }
  constructor(element, dom, dbService, window) {
    this.element = element;
    this.dom = dom;
    this.dbService = dbService;
    this.window = window;
    
    this.selectedList = null;
    this.quickieSearchUi = null;
    this.saveTimeout = null;
    
    this.buildUi();
    
    this.refreshAll();
  }
  
  refreshAll() {
    this.dbService.fetchRecords("list")
      .then(lists => this.rebuildListsList(lists))
      .catch(error => this.window.console.error(`Failed to fetch lists`, error));
  }
  
  onRemoveFromDom() {
    if (this.saveTimeout) {
      this.window.clearTimeout(this.saveTimeout);
      this.saveTimeout = null;
    }
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const listsList = this.dom.spawn(this.element, "UL", ["listsList"]);
    const vertPacker = this.dom.spawn(this.element, "DIV", ["vertPacker"]);
    this.buildHeaderForm(this.dom.spawn(vertPacker, "FORM", ["header"], {
      "on-submit": e => this.onSubmitHeader(e),
      "on-input": e => this.onHeaderInput(e),
    }));
    this.buildPayloadForm(this.dom.spawn(vertPacker, "DIV", ["payload"]));
  }
  
  buildHeaderForm(parent) {
    this.dom.spawn(parent, "INPUT", ["hidden"], { type: "submit" });
    const table = this.dom.spawn(parent, "TABLE");
    this.spawnRow(table, "listid", { disabled: "disabled" });
    this.spawnRow(table, "name", { });
    this.spawnRow(table, "desc", { });
    this.spawnRow(table, "sorted", { type: "checkbox" });
  }
  
  spawnRow(table, k, attr) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["key"], k);
    const tdv = this.dom.spawn(tr, "TD", ["value"]);
    attr = {
      name: k,
      type: "text",
      ...attr,
    };
    const input = this.dom.spawn(tdv, "INPUT", attr);
  }
  
  buildPayloadForm(parent) {
    const left = this.dom.spawn(parent, "DIV");
    this.dom.spawn(left, "H3", "Games in List");
    this.dom.spawn(left, "UL", ["gamesPresent"]);
    const right = this.dom.spawn(parent, "DIV");
    this.dom.spawn(right, "H3", "Add Games");
    this.quickieSearchUi = this.dom.spawnController(right, QuickieSearchUi);
    this.quickieSearchUi.onClickGame = (game) => this.onAddGame(game);
  }
  
  rebuildListsList(lists) {
    const listsList = this.element.querySelector(".listsList");
    listsList.innerHTML = "";
    for (const list of lists) {
      const li = this.dom.spawn(listsList, "LI", list.name || list.listid, { "on-click": () => this.selectList(list, li) });
      if (list.listid === this.selectedList?.listid) {
        li.classList.add("selected");
      }
    }
  }
  
  selectList(list, li) {
    for (const element of this.element.querySelectorAll(".listsList > li.selected")) {
      element.classList.remove("selected");
    }
    li.classList.add("selected");
    this.selectedList = list;
    this.populateHeaderAndPayload();
  }
  
  populateHeaderAndPayload() {
    for (const input of this.element.querySelectorAll(".header input")) {
      if (input.type === "checkbox") input.checked = this.selectedList[input.name];
      else input.value = this.selectedList[input.name] || "";
    }
    const gamesList = this.element.querySelector(".gamesPresent");
    gamesList.innerHTML = "";
    for (const game of this.selectedList.games || []) {
      const li = this.dom.spawn(gamesList, "LI");
      this.dom.spawn(li, "INPUT", { type: "button", value: "X", "on-click": () => this.onRemoveGame(game) });
      this.dom.spawn(li, "DIV", `${game.gameid}: ${game.name}`, { "on-click": () => {
        const modal = this.dom.spawnModal(GameDetailsModal);
        modal.setupGameid(game.gameid);
      }});
    }
  }
  
  allowEdit(allow) {
    this.element.querySelector("input[name='name']").disabled = !allow;
    this.element.querySelector("input[name='desc']").disabled = !allow;
    this.element.querySelector("input[name='sorted']").disabled = !allow;
  }
  
  saveLater() {
    if (this.saveTimeout) this.window.clearTimeout(this.saveTimeout);
    this.saveTimeout = this.window.setTimeout(() => {
      this.saveTimeout = null;
      this.saveNow();
    }, 2000);
  }
  
  saveNow() {
    if (this.saveTimeout) {
      this.window.clearTimeout(this.saveTimeout);
      this.saveTimeout = null;
    }
    if (!this.selectedList) return;
    const update = {
      listid: this.selectedList.listid,
      name: this.element.querySelector("input[name='name']").value,
      desc: this.element.querySelector("input[name='desc']").value,
      sorted: this.element.querySelector("input[name='sorted']").checked,
      games: this.selectedList.games,
    };
    this.dbService.updateRecord("list", update).then(() => this.refreshAll());
  }
  
  onSubmitHeader(event) {
    event.preventDefault();
    this.saveNow();
  }
  
  onHeaderInput() {
    this.saveLater();
  }
  
  onAddGame(game) {
    if (!this.selectedList) return;
    if (this.selectedList.games.find(g => g.gameid === game.gameid)) return;
    this.allowEdit(false);
    this.dbService.addToList(game.gameid, this.selectedList.listid).then(() => {
      this.selectedList.games.push(game);
      this.populateHeaderAndPayload();
      this.allowEdit(true);
    }).catch(error => {
      this.window.console.error(error);
      this.allowEdit(true);
    });
  }
  
  onRemoveGame(game) {
    this.allowEdit(false);
    this.dbService.removeFromList(game.gameid, this.selectedList.listid).then(() => {
      const p = this.selectedList.games.findIndex(g => g.gameid === game.gameid);
      if (p >= 0) {
        this.selectedList.games.splice(p, 1);
        this.populateHeaderAndPlayload();
      }
      this.allowEdit(true);
    }).catch(error => {
      this.window.console.error(error);
      this.allowEdit(true);
    });
  }
}

ListsUi.TAB_LABEL = "Lists";
