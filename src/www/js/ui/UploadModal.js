/* UploadModal.js
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";
import { DbService } from "../model/DbService.js";
import { GameDetailsModal } from "./GameDetailsModal.js";

export class UploadModal {
  static getDependencies() {
    return [HTMLElement, Dom, Comm, DbService, Window];
  }
  constructor(element, dom, comm, dbService, window) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    this.dbService = dbService;
    this.window = window;
    
    this.onSaved = () => {};
    
    this.inputFileName = "";
    this.serial = null; // ArrayBuffer
    this.tableName = ""; // "game"|"blob"
    this.verificationRequired = false; // While true, the submit button does a dry run instead. After text fields change.
  }
  
  setupGame() {
    this.tableName = "game";
    this.buildUi();
  }
  
  setupBlob() {
    this.tableName = "blob";
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": e => this.onSubmit(e) });
    this.dom.spawn(form, "INPUT", { type: "file", "on-change": e => this.onFileInputChange(e) });
    const table = this.dom.spawn(form, "TABLE");
    switch (this.tableName) {
    
      case "game": {
          this.spawnRowImmutable(table, "path");
          this.spawnRowImmutable(table, "name");
          this.spawnRowMutable(table, "basename");
          this.spawnRowMutable(table, "platform");
        } break;
        
      case "blob": {
          //TODO
        } break;
        
    }
    this.dom.spawn(form, "INPUT", { type: "submit", value: "Upload", disabled: "disabled" });
  }
  
  spawnRow(table, key) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["key"], key);
    return this.dom.spawn(tr, "TD", ["value"], { "data-key": key });
  }
  
  spawnRowImmutable(table, key) {
    const tdv = this.spawnRow(table, key);
  }
  
  spawnRowMutable(table, key) {
    const tdv = this.spawnRow(table, key);
    const input = this.dom.spawn(tdv, "INPUT", { name: key, type: "text", "on-input": () => this.onTextChange() });
  }
  
  depopulate() {
    const clearText = k => {
      const element = this.element.querySelector(`*[data-key='${k}']`);
      if (element) element.innerText = "";
    };
    const clearInput = k => {
      const element = this.element.querySelector(`*[data-key='${k}'] > input`);
      if (element) element.value = "";
    };
    // Now clear fields for all types, it's safe if they don't exist...
    clearText("path");
    clearText("name");
    clearInput("basename");
    clearInput("platform");
    const submit = this.element.querySelector("input[type='submit']");
    submit.disabled = true;
    submit.value = "Upload";
  }
  
  populateGame(rsp) {
    this.element.querySelector("*[data-key='path']").innerText = rsp.path || "";
    this.element.querySelector("*[data-key='name']").innerText = rsp.name || "";
    if (rsp.path) this.element.querySelector("*[data-key='basename'] > input").value = rsp.path.replace(/^.*\//, "");
    else this.element.querySelector("*[data-key='basename'] > input").value = this.inputFileName;
    this.element.querySelector("*[data-key='platform'] > input").value = rsp.platform || "";
    const submit = this.element.querySelector("input[type='submit']");
    submit.disabled = false;
    submit.value = "Upload";
    this.verificationRequired = false;
  }
  
  onFileInputChange(e) {
    this.depopulate();
    const files = e?.target?.files;
    if (files?.length !== 1) return;
    const file = files[0];
    const fileReader = new this.window.FileReader();
    fileReader.addEventListener("load", () => this.onFileLoaded(file.name, fileReader.result));
    fileReader.readAsArrayBuffer(file);
  }
  
  onTextChange() {
    switch (this.tableName) {
      case "game": {
          const submit = this.element.querySelector("input[type='submit']");
          submit.disabled = false;
          submit.value = "Verify";
          this.verificationRequired = true;
        } break;
    }
  }
  
  onFileLoaded(name, serial) {
    if (!name || !serial) return;
    this.inputFileName = name;
    this.serial = serial;
    switch (this.tableName) {
      case "game": this.onFileLoaded_game(); break;
      case "blob": this.onFileLoaded_blob(); break;
    }
  }
  
  onFileLoaded_game() {
    const query = {
      "dry-run": 1,
      size: this.serial.byteLength,
      name: this.inputFileName,
    };
    const nameFromForm = this.element.querySelector("*[data-key='basename'] > input")?.value;
    if (nameFromForm) query.name = nameFromForm;
    const platformFromForm = this.element.querySelector("*[data-key='platform'] > input")?.value;
    if (platformFromForm) query.platform = platformFromForm;
    this.comm.http("PUT", "/api/game/file", query, null, null, "json").then(rsp => {
      this.populateGame(rsp);
    }).catch(e => {
      console.log(`File upload dry run failed.`, e);
    });
  }
  
  onFileLoaded_blob() {
    if (!this.imageService.isPng(this.serial)) {
      console.log(`This is not a PNG file. Blobs can be anything, but we should ask for confirmation.`, this.serial);//TODO
    } else {
      this.element.querySelector("input[value='Upload']").disabled = false;
    }
  }
  
  onUpload() {
    if (this.verificationRequired) {
      return this.onFileLoaded(this.inputFileName, this.serial);
    } else if (!this.inputFileName || !this.serial) {
      return;
    } else switch (this.tableName) {
    
      case "game": {
          const query = {
            // We've passed the dry run, so there will definitely be a basename and platform in the form.
            name: this.element.querySelector("*[data-key='basename'] > input").value,
            platform: this.element.querySelector("*[data-key='platform'] > input").value,
          };
          this.comm.http("PUT", "/api/game/file", query, null, this.serial, "json").then(rsp => {
            this.openGameDetailsAndCloseSelf(rsp);
          }).catch(e => {
            console.log(`File upload failed.`, e);
          });
        } break;
        
      case "blob": {
          console.log(`TODO UploadModal.onUpload (blob)`);
        } break;
        
    }
  }
  
  onSubmit(e) {
    e.preventDefault();
    e.stopPropagation();
    this.onUpload();
  }
  
  openGameDetailsAndCloseSelf(rsp) {
    const details = this.dom.spawnModal(GameDetailsModal);
    details.setupGameid(rsp.gameid);
    this.onSaved();
    this.dom.dismissModalByController(this);
  }
}
