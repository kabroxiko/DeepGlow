// Aquarium LED Controller - Web Interface JavaScript

let ws = null;
let reconnectInterval = null;
let presets = [];
let timers = [];

// Base URL logic for local development and file://
let BASE_URL = "";
if (
  location.protocol === "file:" ||
  location.hostname === "localhost" ||
  location.hostname === "127.0.0.1"
) {
  BASE_URL = localStorage.getItem("BASE_URL") || "";
  if (!BASE_URL) {
    BASE_URL = prompt(
      "Enter backend base URL (e.g. http://192.168.1.100:80):",
      "",
    );
    if (BASE_URL) localStorage.setItem("BASE_URL", BASE_URL);
  }
}

// Shared slider logic for both config.js and app.js
window.steppedTransitionValue = function (val) {
  val = Number(val);
  if (val <= 59) return val; // 0–59s
  if (val <= 119) return (val - 59) * 60; // 1–60m
  if (val <= 127) return (val - 119) * 3600; // 1–8h
  return 28800;
};

window.formatTransitionTime = function (val) {
  val = Number(val);
  if (val === 0) return "0s";
  if (val < 60) return val + "s";
  if (val < 3600) return Math.round(val / 60) + "m";
  return Math.round(val / 3600) + "h";
};

// Convert #RRGGBBWW or #RRGGBB to a preview color, blending white channel
function rgbwHexToPreview(hex) {
  hex = hex.replace(/^#/, "");
  let r = 0,
    g = 0,
    b = 0,
    w = 0;
  if (hex.length === 8) {
    r = parseInt(hex.slice(0, 2), 16);
    g = parseInt(hex.slice(2, 4), 16);
    b = parseInt(hex.slice(4, 6), 16);
    w = parseInt(hex.slice(6, 8), 16);
  } else if (hex.length === 6) {
    r = parseInt(hex.slice(0, 2), 16);
    g = parseInt(hex.slice(2, 4), 16);
    b = parseInt(hex.slice(4, 6), 16);
  }
  // If only white channel, always preview as pure white
  if (w > 0 && r === 0 && g === 0 && b === 0) {
    return `rgb(255,255,255)`;
  }
  const blend = (c) => Math.round(c + (255 - c) * (w / 255));
  return `rgb(${blend(r)},${blend(g)},${blend(b)})`;
}


// Centralized WebSocket connection for all pages
// Usage: initializeWebSocket({ onMessage, onBinary, handshake })
window.initializeWebSocket = function (options = {}) {
  let wsUrl;
  if (BASE_URL) {
    wsUrl = BASE_URL.replace(/^http/, "ws") + "/ws";
  } else {
    const wsProtocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    wsUrl = `${wsProtocol}//${window.location.host}/ws`;
  }
  if (window.ws && window.ws.readyState === WebSocket.OPEN) {
    window.ws.close();
  }
  ws = new WebSocket(wsUrl);
  window.ws = ws;
  ws.binaryType = "arraybuffer";

  ws.onopen = () => {
    if (options.handshake) {
      ws.send(JSON.stringify(options.handshake));
    }
    if (reconnectInterval) {
      clearInterval(reconnectInterval);
      reconnectInterval = null;
    }
    if (window.wsHeartbeat) clearInterval(window.wsHeartbeat);
    window.wsHeartbeat = setInterval(() => {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send('{"type":"ping"}');
      }
    }, 30000);
    const statusIndicator = document.getElementById("statusIndicator");
    if (statusIndicator && statusIndicator.style)
      statusIndicator.style.color = "#00cc88";
  };

  ws.onmessage = (event) => {
    if (event.data instanceof ArrayBuffer) {
      if (typeof options.onBinary === "function") {
        options.onBinary(event.data);
      }
      return;
    }
    try {
      const data = JSON.parse(event.data);
      if (typeof options.onMessage === "function") {
        options.onMessage(data);
      }
    } catch (e) {
      console.error("[WS] Failed to parse message:", e);
    }
  };

  ws.onclose = () => {
    const statusIndicator = document.getElementById("statusIndicator");
    if (statusIndicator && statusIndicator.style)
      statusIndicator.style.color = "#ff4466";
    if (window.wsHeartbeat) {
      clearInterval(window.wsHeartbeat);
      window.wsHeartbeat = null;
    }
    if (!reconnectInterval) {
      reconnectInterval = setInterval(() => {
        console.log("[WS] Attempting to reconnect...");
        window.initializeWebSocket(options);
      }, 5000);
    }
  };

  ws.onerror = (error) => {
    console.error("[WS] Error:", error);
  };
};

// Update UI with state from server
// Stepped transform for transition time slider (even-distribution, decisecond/second/minute/hour)
function steppedTransitionValue(val) {
  // EXACTLY match Min Transition Time mapping from config.js
  val = Number(val);
  if (val <= 59) return val; // 0–59s
  if (val <= 119) return (val - 59) * 60; // 1–60m
  if (val <= 127) return (val - 119) * 3600; // 1–8h
  return 28800;
}

// Format transition time for display
function formatTransitionTime(val) {
  val = Number(val);
  if (val === 0) return "0s";
  if (val < 60) return val + "s";
  if (val < 3600) return Math.round(val / 60) + "m";
  return Math.round(val / 3600) + "h";
}
