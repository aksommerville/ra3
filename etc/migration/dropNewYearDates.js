#!/usr/bin/env node

/* dropNewYearDates.js
 * Somehow I have a bunch of games with pubtime "*-01-01", instead of the conventional "*-00-00".
 * Really not a problem, but I want to drop the "-01-01" to keep things neat.
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
      rsp.on("end", () => {
        if (rsp.statusCode !== 200) {
          console.log(`${method} ${url} => ${rsp.statusCode} ${rsp.statusMessage}`);
          reject(rsp.statusMessage);
        } else {
          resolve(rspBody ? JSON.parse(rspBody) : "");
        }
      });
      rsp.on("error", error => reject(error));
    }).end(body);
  });
}

// First acquire the whole DB. We only need "record" detail.
httpJson("POST", "/api/query", { rating: "0..100", limit: "999999", detail: "record" }).then(games => {
  console.log(`Got ${games.length} games total.`);
  
  // As long as we're here, let's bucket and report by pubtime format.
  const undatedGames = [];
  const newYearsGames = [];
  const yearOnlyGames = [];
  const validDateGames = []; // YMD, not 1 Jan. Unusual. But my own games do it, since i know the exact release dates. ...oh huh, actually i didn't do that. whatever.
  const tooDetailedGames = []; // including time
  for (const game of games) {
         if (!game.pubtime) undatedGames.push(game);
    else if (game.pubtime.length === 4) yearOnlyGames.push(game);
    else if (game.pubtime.match(/^\d{4}-01-01$/)) newYearsGames.push(game);
    else if (game.pubtime.match(/^\d{4}-\d{2}-\d{2}$/)) validDateGames.push(game);
    else if (game.pubtime.match(/^\d{4}-\d{2}-\d{2}T.*$/)) tooDetailedGames.push(game);
    else console.log(`OTHER: ${game.gameid} ${game.name} ${JSON.stringify(game.pubtime)}`);
  }
  /**/
  console.log(`full date: ${validDateGames.length}`);
  console.log(`valid year: ${yearOnlyGames.length}`);
  console.log(`undated: ${undatedGames.length}`);
  console.log(`with time: ${tooDetailedGames.length}`);
  console.log(`-01-01: ${newYearsGames.length}`);
  /**/
  
  console.log(`Updating ${newYearsGames.length} games to remove "-01-01"...`);
  const next = () => {
    if (!newYearsGames.length) return Promise.resolve();
    const nextGame = newYearsGames[0];
    newYearsGames.splice(0, 1);
    nextGame.pubtime = nextGame.pubtime.substring(0, 4);
    console.log(`  ${nextGame.gameid} ${nextGame.name}`);
    return httpJson("PATCH", "/api/game", { detail: "id" }, null, JSON.stringify(nextGame))
      .then(() => next());
  };
  return next();
});
