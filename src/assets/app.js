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

// WebSocket Connection
function initializeWebSocket() {
  let wsUrl;
  if (BASE_URL) {
    // Convert BASE_URL to ws(s)://
    wsUrl = BASE_URL.replace(/^http/, "ws") + "/ws";
  } else {
    const wsProtocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    wsUrl = `${wsProtocol}//${window.location.host}/ws`;
  }
  ws = new WebSocket(wsUrl);

  // Accept binary blobs for live LED data
  ws.binaryType = "arraybuffer";

  ws.onopen = () => {
    console.log("[WS] Connected");
    const statusIndicator = document.getElementById("statusIndicator");
    if (statusIndicator && statusIndicator.style)
      statusIndicator.style.color = "#00cc88";
    if (reconnectInterval) {
      clearInterval(reconnectInterval);
      reconnectInterval = null;
    }
    // Start heartbeat
    if (window.wsHeartbeat) clearInterval(window.wsHeartbeat);
    window.wsHeartbeat = setInterval(() => {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send('{"type":"ping"}');
      }
    }, 30000); // 30 seconds
  };

  ws.onmessage = (event) => {
    // If message is binary, treat as LED data
    if (event.data instanceof ArrayBuffer) {
      updateLedBarFromBlob(event.data);
      return;
    }
    // Otherwise, treat as JSON
    try {
      const data = JSON.parse(event.data);
      updateState(data);
    } catch (e) {
      console.error("[WS] Failed to parse message:", e);
    }
  };

  // Update the floating LED bar from binary blob (RGBW)
  function updateLedBarFromBlob(buffer) {
    const canvas = document.getElementById("ledBarCanvas");
    if (!canvas) return;
    const container = document.getElementById("ledBarContainer");
    let needResize = false;
    let width = canvas.width;
    if (container) {
      const style = getComputedStyle(container);
      const newWidth =
        container.clientWidth -
        parseFloat(style.paddingLeft) -
        parseFloat(style.paddingRight);
      if (canvas.width !== newWidth) {
        canvas.width = newWidth;
        needResize = true;
        width = newWidth;
      }
    }
    const ctx = canvas.getContext("2d");
    if (!ctx) return;
    // Buffer is [R,G,B,W,R,G,B,W,...] for each LED
    // Reuse a single Uint8Array for performance
    if (
      !window._ledBarArr ||
      window._ledBarArr.buffer.byteLength !== buffer.byteLength
    ) {
      window._ledBarArr = new Uint8Array(buffer);
    } else {
      window._ledBarArr.set(new Uint8Array(buffer));
    }
    const arr = window._ledBarArr;
    const ledCount = Math.floor(arr.length / 4);
    const w = canvas.width;
    const h = canvas.height;
    // Only clear if needed
    if (needResize || ledCount === 0) ctx.clearRect(0, 0, w, h);
    if (ledCount === 0) return;
    const ledWidth = w / ledCount;
    // Batch drawing with requestAnimationFrame for smoothness
    if (window._ledBarDrawReq) cancelAnimationFrame(window._ledBarDrawReq);
    window._ledBarDrawReq = requestAnimationFrame(() => {
      ctx.clearRect(0, 0, w, h);
      for (let i = 0; i < ledCount; ++i) {
        let r1 = arr[i * 4];
        let g1 = arr[i * 4 + 1];
        let b1 = arr[i * 4 + 2];
        let wch1 = arr[i * 4 + 3];
        // If any white channel, force pure white
        if (wch1 > 0) {
          r1 = 255;
          g1 = 255;
          b1 = 255;
        }
        let r2 = r1,
          g2 = g1,
          b2 = b1;
        if (i < ledCount - 1) {
          r2 = arr[(i + 1) * 4];
          g2 = arr[(i + 1) * 4 + 1];
          b2 = arr[(i + 1) * 4 + 2];
          let wch2 = arr[(i + 1) * 4 + 3];
          if (wch2 > 0) {
            r2 = 255;
            g2 = 255;
            b2 = 255;
          }
        }
        // Create gradient between r1,g1,b1 and r2,g2,b2
        let x0 = i * ledWidth;
        let x1 = (i + 1) * ledWidth;
        let grad = ctx.createLinearGradient(x0, 0, x1, 0);
        grad.addColorStop(0, `rgb(${r1},${g1},${b1})`);
        grad.addColorStop(1, `rgb(${r2},${g2},${b2})`);
        ctx.fillStyle = grad;
        ctx.fillRect(x0, 0, Math.ceil(ledWidth), h);
      }
      // Optional: draw border
      ctx.strokeStyle = "#444";
      ctx.lineWidth = 1;
      ctx.strokeRect(0, 0, w, h);
    });
    // Responsive: redraw on resize
    if (!window._ledBarResizeHandler) {
      window._ledBarResizeHandler = () => {
        if (window._lastLedBarBuffer)
          updateLedBarFromBlob(window._lastLedBarBuffer);
      };
      window.addEventListener("resize", window._ledBarResizeHandler);
    }
    window._lastLedBarBuffer = buffer;
  }

  ws.onclose = () => {
    console.log("[WS] Disconnected");
    const statusIndicator = document.getElementById("statusIndicator");
    if (statusIndicator && statusIndicator.style)
      statusIndicator.style.color = "#ff4466";
    // Stop heartbeat
    if (window.wsHeartbeat) {
      clearInterval(window.wsHeartbeat);
      window.wsHeartbeat = null;
    }
    // Attempt to reconnect
    if (!reconnectInterval) {
      reconnectInterval = setInterval(() => {
        console.log("[WS] Attempting to reconnect...");
        initializeWebSocket();
      }, 5000);
    }
  };

  ws.onerror = (error) => {
    console.error("[WS] Error:", error);
  };
}

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
