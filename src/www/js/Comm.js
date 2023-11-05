/* Comm.js
 */
 
export class Comm {
  static getDependencies() {
    return [Window];
  }
  constructor(window) {
    this.window = window;
    
    this.socket = null;
    this.socketOpen = false;
    this.webSocketListeners = [];
    this.nextWebSocketListenerId = 1;
    this.currentGameid = 0; // not really our problem, but it's convenient to capture here
    
    this.connectWebSocket();
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
        case "arraybuffer": return rsp.arrayBuffer();
        case "text": return rsp.text();
      }
      return rsp;
    });
  }
  
  // Because we often give just (method,url) and I forget how many dummy args...
  httpJson(method, url, params, headers, body) {
    return this.http(method, url, params, headers, body, "json");
  }
  
  sendWsJson(message) {
    if (!this.socketOpen) return false;
    this.socket.send(JSON.stringify(message));
    return true;
  }
  
  // ArrayBuffer or TypedArray
  sendWsBinary(message) {
    if (!this.socketOpen) return false;
    this.socket.send(message);
    return true;
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
  
  connectWebSocket() {
    if (this.socket) {
      this.socket.close();
      this.socket = null;
      this.socketOpen = false;
    }
    this.socket = new WebSocket(`ws://${this.window.location.host}/ws/menu`);
    this.socket.binaryType = "arraybuffer";
    this.socket.addEventListener("close", event => this.onWebSocketClose(event));
    this.socket.addEventListener("error", event => this.onWebSocketError(event));
    this.socket.addEventListener("message", event => this.onWebSocketMessage(event));
    this.socket.addEventListener("open", event => this.onWebSocketOpen(event));
  }
  
  /* cb("json", object) or cb("text", string) or cb("binary", ArrayBuffer)
   */
  listenWs(cb) {
    const id = this.nextWebSocketListenerId++;
    this.webSocketListeners.push({ id, cb });
    return id;
  }
  
  unlistenWs(id) {
    const p = this.webSocketListeners.findIndex(l => l.id === id);
    if (p < 0) return;
    this.webSocketListeners.splice(p, 1);
  }
  
  broadcastWs(type, packet) {
    for (const { cb } of this.webSocketListeners) cb(type, packet);
  }
  
  onWebSocketClose(event) {
    this.socket = null;
    this.socketOpen = false;
  }
  
  onWebSocketError(event) {
    this.socket = null;
    this.socketOpen = false;
  }
  
  onWebSocketMessage(event) {
    if (typeof(event.data) === "string") {
      let packet = null;
      try {
        packet = JSON.parse(event.data);
        this.processWsJsonLocal(packet);
      } catch (e) {
        this.broadcastWs("text", event.data);
        return;
      }
      this.broadcastWs("json", packet);
    } else if (event.data instanceof ArrayBuffer) {
      this.broadcastWs("binary", event.data);
    }
  }
  
  onWebSocketOpen(event) {
    if (!this.socket) return; // ???
    this.socketOpen = true;
  }
  
  processWsJsonLocal(packet) {
    switch (packet.id) {
      case "game": this.currentGameid = packet.gameid || 0; break;
    }
  }
}

Comm.singleton = true;
