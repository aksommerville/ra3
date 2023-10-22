#!/usr/bin/env node

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

function putLaunchers(launchers) {
  if (!launchers.length) return Promise.resolve();
  const launcher = launchers[0];
  launchers.splice(0, 1);
  return httpJson("PUT", "/api/launcher", null, null, JSON.stringify(launcher))
    .then(() => putLaunchers(launchers));
}

const launchers = JSON.parse(fs.readFileSync("launcher.json"));
console.log(`Uploading ${launchers.length} launchers...`);
putLaunchers(launchers)
  .then(() => console.log(`...done`))
  .catch(e => console.error(e));
