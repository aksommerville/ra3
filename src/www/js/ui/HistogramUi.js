/* ListsUi.js
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";

export class HistogramUi {
  static getDependencies() {
    return [HTMLElement, Dom, Comm];
  }
  constructor(element, dom, comm) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    
    this.comm.httpJson("GET", "/api/histograms")
      .then(r => this.buildUi(r))
      .catch(e => console.log(`Failed to fetch histograms`, e));
  }
  
  buildUi(rsp) {
    this.element.innerHTML = "";
    this.addGenericUi(rsp.author || [], "Author");
    this.addGenericUi(rsp.platform || [], "Platform");
    this.addGenericUi(rsp.genre || [], "Genre");
    this.addRatingUi(rsp.rating || []);
    this.addPubtimeUi(rsp.pubtime || []);
  }
  
  addGenericUi(src, k) {
    this.dom.spawn(this.element, "H2", `By ${k}`);
    const row = this.dom.spawn(this.element, "DIV", ["reportRow"]);
    for (const { v, c } of this.reduceHistogram(src, 15)) {
      const col = this.dom.spawn(row, "DIV", ["reportCol"]);
      this.dom.spawn(col, "DIV", ["count"], c);
      this.dom.spawn(col, "DIV", ["label"], v.toString() || "(unset)");
    }
  }
  
  addRatingUi(src) {
    src = [...src];
    this.dom.spawn(this.element, "H2", "By Rating");
    if ((src.length >= 1) && !src[0].v) {
      this.dom.spawn(this.element, "DIV", ["ratingCaption"], `${src[0].c} unset`);
      src.splice(0, 1);
    }
    
    let maxc = 0;
    for (const { c } of src) {
      if (c > maxc) maxc = c;
    }
    
    // Setup chart and fill with black.
    const chart = this.dom.spawn(this.element, "CANVAS", ["ratingChart"]);
    chart.width = chart.offsetWidth;
    chart.height = chart.offsetHeight;
    const ctx = chart.getContext("2d");
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, chart.width, chart.height);
    
    // Faint lines and label at each decade.
    ctx.beginPath();
    ctx.fillStyle = "#666";
    for (let v=0; v<100; v+=10) {
      const x = Math.floor((v * chart.width) / 100) + 0.5;
      ctx.moveTo(x, 0);
      ctx.lineTo(x, chart.height);
      ctx.fillText(v, x + 5, 10);
    }
    ctx.strokeStyle = "#666";
    ctx.stroke();
    
    // And finally a bar for each value >0
    const barw = Math.floor(chart.width / 100);
    const padding = 15;
    const barh = chart.height - padding;
    ctx.fillStyle = "#f80";
    for (const { v, c } of src) {
      if (!c) continue;
      const x = Math.floor((v * chart.width) / 100);
      const h = Math.max(1, Math.floor((c * barh) / maxc));
      const y = chart.height - h;
      ctx.fillRect(x, y, barw, h);
    }
  }
  
  addPubtimeUi(src) {
    //this.dom.spawn(this.element, "H2", "By Date");
    //TODO... meh
  }
  
  /* Return a new histogram with no more than (fldc) fields, and fields sorted by count.
   */
  reduceHistogram(src, fldc) {
    src = [...src];
    src.sort((a, b) => b.c - a.c);
    if (src.length > fldc) {
      let otherc = 0;
      for (let i=fldc-1; i<src.length; i++) otherc += src[i].c;
      src.splice(fldc - 1, src.length - fldc + 1);
      src.push({ v: "Other", c: otherc });
    }
    return src;
  }
}

HistogramUi.TAB_LABEL = "Histogram";
