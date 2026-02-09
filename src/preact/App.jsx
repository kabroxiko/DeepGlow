import { useState, useEffect, useRef } from "preact/hooks";
import { getBaseUrl } from "./baseUrl.js";
import { initializeWebSocket } from "./websocket.js";
import { Toast, useToast } from "./Toast.jsx";
import { Home } from "./Home.jsx";
import { Config } from "./Config.jsx";

export function App() {
  // OTA progress state (shared for Config)
  const [otaProgress, setOtaProgress] = useState(-1); // -1: hidden, 0-100: progress
  // Home state (move up so it's available for all hooks)
  const [state, setState] = useState({});
  // Ticking clock synced to firmware time (state.time)
  const [liveClock, setLiveClock] = useState(() => {
    // Start with local time if nothing else
    const now = new Date();
    return now.toLocaleTimeString([], { hour12: false });
  });
  const lastSyncRef = useRef({ date: new Date(), fwTime: null });
  // When state.time changes, sync the clock
  useEffect(() => {
    if (state.time && /^\d{2}:\d{2}:\d{2}$/.test(state.time)) {
      // Parse state.time as HH:MM:SS
      const [h, m, s] = state.time.split(":").map(Number);
      const now = new Date();
      const syncDate = new Date(
        now.getFullYear(),
        now.getMonth(),
        now.getDate(),
        h,
        m,
        s,
      );
      lastSyncRef.current = { date: now, fwTime: syncDate };
      setLiveClock(syncDate.toLocaleTimeString([], { hour12: false }));
    }
  }, [state.time]);
  // Tick every second, adding to last synced firmware time
  useEffect(() => {
    const interval = setInterval(() => {
      const { date, fwTime } = lastSyncRef.current;
      if (fwTime) {
        const now = new Date();
        const elapsed = Math.floor((now - date) / 1000);
        const tickDate = new Date(fwTime.getTime() + elapsed * 1000);
        setLiveClock(tickDate.toLocaleTimeString([], { hour12: false }));
      } else {
        // Fallback: tick local time
        const now = new Date();
        setLiveClock(now.toLocaleTimeString([], { hour12: false }));
      }
    }, 1000);
    return () => clearInterval(interval);
  }, []);
  // Toast state
  const [toast, setToast] = useToast();

  // Toast helper (App-level, always use this)
  function showToast(message, type = "info", duration = 4000) {
    // Defensive: If message is a toast-like object, flatten
    if (
      typeof message === "object" &&
      message !== null &&
      Object.prototype.hasOwnProperty.call(message, "message") &&
      Object.prototype.hasOwnProperty.call(message, "type") &&
      Object.prototype.hasOwnProperty.call(message, "visible")
    ) {
      message = message.message || JSON.stringify(message);
    }
    let msg = message;
    if (typeof msg === "object" && msg !== null) {
      msg = msg.message || JSON.stringify(msg);
    }
    if (!msg || String(msg).trim() === "") return;
    setToast({ message: msg, type, visible: true });
    setTimeout(
      () => setToast((t) => ({ ...t, visible: false, message: "" })),
      duration,
    );
  }
  // Tab state from URL hash, fallback to localStorage, then home
  const getInitialTab = () => {
    if (window.location.hash === "#config") return "config";
    if (window.location.hash === "#home") return "home";
    try {
      const saved = localStorage.getItem("deepglow_tab");
      if (saved === "config" || saved === "home") return saved;
    } catch {}
    return "home";
  };
  const [tab, setTab] = useState(getInitialTab);
  // Update tab when hash changes
  useEffect(() => {
    const onHashChange = () => {
      if (window.location.hash === "#config") setTab("config");
      else setTab("home");
    };
    window.addEventListener("hashchange", onHashChange);
    return () => window.removeEventListener("hashchange", onHashChange);
  }, []);
  // Persist tab to localStorage and URL hash on change
  useEffect(() => {
    try {
      if (tab === "config") {
        localStorage.setItem("deepglow_tab", "config");
        window.location.hash = "#config";
      } else {
        localStorage.setItem("deepglow_tab", "home");
        window.location.hash = "#home";
      }
    } catch {}
  }, [tab]);
  // Shared WebSocket connection
  const wsRef = useRef(null);
  const [wsReady, setWsReady] = useState(false);
  const [wsError, setWsError] = useState(null);

  // Create the WebSocket connection only once
  useEffect(() => {
    wsRef.current = initializeWebSocket({
      handshake: { type: tab === "config" ? "ota_client" : "state" },
      onMessage: (data) => {
        setState((prev) => ({ ...prev, ...data }));
        if (data.preset !== undefined) setActivePreset(data.preset);
        if ("sunrise" in data) {
          setSunTimes((st) => ({
            sunrise: data.sunrise,
            sunset: st.sunset,
          }));
        }
        if (data.type === "ota_status") {
          if (typeof data.progress === "number") {
            setOtaProgress(data.progress);
            if (data.progress >= 100) {
              setTimeout(() => setOtaProgress(-1), 2000);
            }
          }
          if (data.status === "success") {
            showToast("OTA update successful! Device will reboot.", "success");
            setTimeout(() => window.location.reload(), 7000);
          } else if (data.status === "error") {
            showToast("OTA update failed: " + data.message, "error");
          }
        }
      },
      onBinary: (buffer) => {
        const canvas = ledBarRef.current;
        if (!canvas) return;
        const arr = new Uint8Array(buffer);
        const ledCount = Math.floor(arr.length / 4);
        const w = canvas.width;
        const h = canvas.height;
        const ctx = canvas.getContext("2d");
        ctx.clearRect(0, 0, w, h);
        if (ledCount === 0) return;
        const ledWidth = w / ledCount;
        for (let i = 0; i < ledCount; ++i) {
          let r1 = arr[i * 4];
          let g1 = arr[i * 4 + 1];
          let b1 = arr[i * 4 + 2];
          let wch1 = arr[i * 4 + 3];
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
          let x0 = i * ledWidth;
          let x1 = (i + 1) * ledWidth;
          let grad = ctx.createLinearGradient(x0, 0, x1, 0);
          grad.addColorStop(0, `rgb(${r1},${g1},${b1})`);
          grad.addColorStop(1, `rgb(${r2},${g2},${b2})`);
          ctx.fillStyle = grad;
          ctx.fillRect(x0, 0, Math.ceil(ledWidth), h);
        }
        ctx.strokeStyle = "#444";
        ctx.lineWidth = 1;
        ctx.strokeRect(0, 0, w, h);
      },
    });
    return () => {
      if (wsRef.current) wsRef.current.close();
    };
  }, []);

  // Only send handshake/subscription message on tab change
  useEffect(() => {
    if (wsRef.current && wsRef.current.readyState === 1) {
      wsRef.current.send(
        JSON.stringify({ type: tab === "config" ? "ota_client" : "state" }),
      );
    }
  }, [tab]);
  // Home state
  const [presets, setPresets] = useState([]);
  const [timers, setTimers] = useState([]);
  const [effects, setEffects] = useState([]);
  const [activePreset, setActivePreset] = useState(null);
  const ledBarRef = useRef(null);
  // Config state (moved from Config.jsx)
  const [config, setConfig] = useState(null);
  const [timezones, setTimezones] = useState([]);
  const [sunTimes, setSunTimes] = useState({ sunrise: "", sunset: "" });
  const [loaded, setLoaded] = useState({
    timezones: false,
    presets: false,
    config: false,
  });

  // Toast helper
  function showToast(message, type = "info", duration = 4000) {
    console.log("showToast called with:", { message, type, duration });
    // Defensive: If message is a toast-like object, log a stack trace ONCE
    if (
      typeof message === "object" &&
      message !== null &&
      Object.prototype.hasOwnProperty.call(message, "message") &&
      Object.prototype.hasOwnProperty.call(message, "type") &&
      Object.prototype.hasOwnProperty.call(message, "visible")
    ) {
      if (!window.__toastStackWarned) {
        window.__toastStackWarned = true;
        console.warn(
          "showToast called with a toast-like object! This is a bug. Stack trace:",
          message,
        );
        console.trace();
      }
      // Flatten to string
      message = message.message || JSON.stringify(message);
    }
    let msg = message;
    if (typeof msg === "object" && msg !== null) {
      msg = msg.message || JSON.stringify(msg);
    }
    if (!msg || String(msg).trim() === "") return;
    setToast({ message: msg, type, visible: true });
    setTimeout(
      () => setToast((t) => ({ ...t, visible: false, message: "" })),
      duration,
    );
  }

  // Fetch shared data (presets, timers, effects, version, config, timezones) in parallel
  useEffect(() => {
    Promise.all([
      fetch(getBaseUrl() + "/api/presets")
        .then((r) => r.json())
        .then((data) => (Array.isArray(data) ? data : data.presets))
        .catch(() => []),
      fetch(getBaseUrl() + "/api/effects")
        .then((r) => r.json())
        .then((data) => data.effects)
        .catch(() => []),
      fetch(getBaseUrl() + "/api/version")
        .then((r) => r.json())
        .catch(() => {}),
      fetch(getBaseUrl() + "/api/timezones")
        .then((resp) => resp.json())
        .catch(() => []),
      fetch(getBaseUrl() + "/api/config")
        .then(async (response) => {
          const text = await response.text();
          if (!text) return {};
          try {
            return JSON.parse(text);
          } catch {
            return {};
          }
        })
        .catch(() => {}),
    ]).then(([presets, effects, version, timezones, config]) => {
      setPresets(presets);
      setEffects(effects);
      setConfig(config);
      setTimezones(timezones);
      setTimers(config?.timers);
      setLoaded({ presets: true, timezones: true, config: true });
      // Set version string if available
      if (version && version.version) {
        const vEl = document.getElementById("versionString");
        if (vEl) vEl.textContent = "Version: " + version.version;
      }
    });
  }, []);

  // Tab UI
  // ...existing code...

  // Defensive normalization: if toast.message is an object (unexpected),
  // convert to a display string and prefer any nested `type`.
  let _displayMessage = toast.message;
  let _displayType = toast.type;
  if (typeof _displayMessage === "object" && _displayMessage !== null) {
    // prefer nested .message if available
    if (typeof _displayMessage.message === "string") {
      _displayMessage = _displayMessage.message;
    } else {
      try {
        _displayMessage = JSON.stringify(_displayMessage);
      } catch (e) {
        _displayMessage = String(_displayMessage);
      }
    }
    // prefer nested type if present
    if (_displayMessage && typeof toast.message.type === "string") {
      _displayType = toast.message.type;
    }
    // fix the state so subsequent renders are correct
    setToast((t) => ({ ...t, message: _displayMessage, type: _displayType }));
  }

  return (
    <div>
      <Toast
        message={_displayMessage}
        type={_displayType}
        visible={toast.visible}
      />
      {tab === "home" ? (
        <Home
          wsError={wsError}
          wsReady={wsReady}
          state={state}
          presets={presets}
          timers={timers}
          effects={effects}
          activePreset={activePreset}
          ledBarRef={ledBarRef}
          sunTimes={sunTimes}
          toast={toast}
          showToast={showToast}
          sendState={sendState}
          applyPreset={applyPreset}
          ScheduleTable={ScheduleTable}
          setTab={setTab}
          config={config}
          liveClock={liveClock}
        />
      ) : (
        <Config
          config={config}
          timezones={timezones}
          sunTimes={sunTimes}
          toast={toast}
          showToast={showToast}
          loaded={loaded}
          setTab={setTab}
          presets={presets || []}
          liveClock={liveClock}
          setConfig={setConfig}
          otaProgress={otaProgress}
        />
      )}
    </div>
  );

  // Event handlers for controls
  function sendState(updates) {
    fetch(getBaseUrl() + "/api/state", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(updates),
    });
  }

  function applyPreset(presetId) {
    fetch(getBaseUrl() + "/api/preset", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ id: presetId, apply: true }),
    }).then(() => setActivePreset(presetId));
  }

  // Schedule table
  function ScheduleTable() {
    let nowMinutes = 0;
    if (state.time && /^\d{2}:\d{2}:\d{2}$/.test(state.time)) {
      const [h, m] = state.time.split(":").map(Number);
      nowMinutes = h * 60 + m;
    } else {
      const now = new Date();
      nowMinutes = now.getHours() * 60 + now.getMinutes();
    }
    let lastTimerIdx = -1;
    let lastTimerTime = -1;
    timers.forEach((timer, idx) => {
      if (!timer.enabled) return;
      let timerTime = timer.hour * 60 + timer.minute;
      if (timerTime <= nowMinutes && timerTime > lastTimerTime) {
        lastTimerTime = timerTime;
        lastTimerIdx = idx;
      }
    });
    return (
      <table className="schedule-table-inner">
        <thead>
          <tr>
            <th>Time</th>
            <th>Preset</th>
            <th>Brightness</th>
            <th>Status</th>
          </tr>
        </thead>
        <tbody>
          {timers.map((timer, index) => {
            if (!timer.enabled && !timer.name && !timer.hour && !timer.minute)
              return null;
            const name = timer.name;
            let timeStr = "--:--";
            if (
              typeof timer.hour === "number" &&
              typeof timer.minute === "number"
            ) {
              timeStr = `${timer.hour.toString().padStart(2, "0")}:${timer.minute.toString().padStart(2, "0")}`;
            } else if (timer.time) {
              const d = new Date(timer.time);
              if (!isNaN(d.getTime())) {
                timeStr = d.toLocaleTimeString([], {
                  hour: "2-digit",
                  minute: "2-digit",
                });
              }
            }
            let presetStr = "--";
            if (
              typeof timer.presetId === "number" &&
              Array.isArray(presets) &&
              presets.length > 0
            ) {
              let found = presets.find((p) => p.id === timer.presetId);
              if (found && found.name) {
                presetStr = found.name;
              } else {
                presetStr = `Preset ${timer.presetId}`;
              }
            }
            const statusStr = timer.enabled ? (
              <span className="timer-enabled">Enabled</span>
            ) : (
              <span className="timer-disabled">Disabled</span>
            );
            const brightStr =
              typeof timer.brightness === "number"
                ? `${timer.brightness}%`
                : "--";
            return (
              <tr
                key={index}
                className={index === lastTimerIdx ? "active-timer-row" : ""}
              >
                <td>{timeStr}</td>
                <td>{presetStr}</td>
                <td>{brightStr}</td>
                <td>{statusStr}</td>
              </tr>
            );
          })}
        </tbody>
      </table>
    );
  }
}
