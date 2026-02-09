import { getBaseUrl } from "./baseUrl.js";
export function initializeWebSocket(opts) {
  return createWebSocket(opts);
}
// Shared WebSocket connection logic
export function createWebSocket({ handshake, onMessage, onBinary }) {
  const BASE_URL = getBaseUrl();
  let wsUrl;
  if (BASE_URL) {
    wsUrl = BASE_URL.replace(/^http/, "ws") + "/ws";
  } else {
    const wsProtocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    wsUrl = `${wsProtocol}//${window.location.host}/ws`;
  }
  const ws = new window.WebSocket(wsUrl);
  ws.binaryType = "arraybuffer";
  ws.onopen = () => {
    if (handshake) ws.send(JSON.stringify(handshake));
  };
  ws.onmessage = (event) => {
    if (event.data instanceof ArrayBuffer && typeof onBinary === "function") {
      onBinary(event.data);
      return;
    }
    try {
      const data = JSON.parse(event.data);
      if (typeof onMessage === "function") onMessage(data);
    } catch (e) {}
  };
  ws.onclose = () => {};
  ws.onerror = () => {};
  return ws;
}
