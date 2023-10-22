#!/usr/bin/env node

// I stupidly forgot to map genre in the big migration.
// Updated populate-ra3.js blindly, so we shouldn't need this one again.
 
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

function update(games) {
  if (games.length < 1) return Promise.resolve();
  const game = games[0];
  games.splice(0, 1);
  return httpJson("PATCH", "/api/game", { detail: "id" }, null, JSON.stringify({
    gameid: game.id,
    genre: game.genre,
  })).then(() => update(games));
}
  
console.log("Updating...");
update(odbGames.filter(g => g.genre)).then(() => console.log("OK updated"));
