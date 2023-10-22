#!/usr/bin/env node

/* desc-as-comment.js
 * ra2 had a "desc" field built in to the game header, ra3 does not.
 * I forgot about this in the first pass. We need to gather those "desc" and add them as comments.
 * The logic is now added (blindly) to populate-ra3.js, so we should only need this script once.
 */
 
const fs = require("fs");
const http = require("http");

function httpJson(method, url, query, headers, body) {
  return new Promise((resolve, reject) => {
    url = "http://localhost:6502" + url;
    if (query) url += "?" + Object.keys(query).map(k => encodeURIComponent(k) + "=" + encodeURIComponent(query[k])).join("&");
    if (!headers) headers = {};
    http.request(url, { method, headers }, (rsp) => {
      let rspBody = "";
      rsp.on("data", data => rspBody += data);
      rsp.on("end", () => resolve(rspBody ? JSON.parse(rspBody) : ""));
      rsp.on("error", error => reject(error));
    }).end(body);
  });
}

const odbGames = JSON.parse(fs.readFileSync("game.json"));
const comments = odbGames.filter(g => g.desc).map(g => ({ gameid: g.id, k: "text", v: g.desc }));
console.log(`Adding ${comments.length} comments from ra2 primary descriptions.`);

function addComments(v) {
  if (!v.length) return Promise.resolve();
  const comment = v[0];
  v.splice(0, 1);
  return httpJson("PUT", "/api/comment", null, null, JSON.stringify(comment))
    .then(() => addComments(v));
}

addComments(comments)
  .then(() => console.log(`...success`))
  .catch(e => console.error(e));
