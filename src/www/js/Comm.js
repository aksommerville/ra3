/* Comm.js
 */
 
export class Comm {
  static getDependencies() {
    return [Window];
  }
  constructor(window) {
    this.window = window;
  }
  
  http(method, url, params, headers, body, result) {
    const options = {
      method,
      mode: "same-origin",
      redirect: "error",
    };
    if (headers) options.headers = headers;
    if (body) options.body = body;
    return this.window.fetch(this.composeUrl(url, params), options).then(rsp => {
      if (!rsp.ok) throw rsp;
      switch (result) {
        case "response": return rsp;
        case "json": return rsp.json();
        case "arraybuffer": return rsp.body.arraybuffer();
        case "text": return rsp.body.text();
      }
      return rsp;
    });
  }
  
  // Because we often give just (method,url) and I forget how many dummy args...
  httpJson(method, url, params, headers, body) {
    return this.http(method, url, params, headers, body, "json");
  }
  
  composeUrl(url, params) {
    if (params) {
      let sep = "?";
      for (const k of Object.keys(params)) {
        url += sep + encodeURIComponent(k) + "=" + encodeURIComponent(params[k]);
        sep = "&";
      }
    }
    return url;
  }
}

Comm.singleton = true;
