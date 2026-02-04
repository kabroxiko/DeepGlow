// index.js - code used exclusively by index.html

// This file is intended to hold logic that is only needed for the main UI (index.html)
// and not for config.html or other pages. Move such code from app.js here.

let currentState = {};
let cachedPreset = undefined;
let clockInterval = null;
let clockSynced = false;
let localClock = null;
let effectNames = [];

// Helper to create color pickers row
function createColorPickers(colors, params) {
  const colorPickersRow = document.getElementById("colorPickersRow");
  if (!colorPickersRow) return;
  colorPickersRow.innerHTML = "";
  if (!colors || !Array.isArray(colors)) return;
  colors.forEach((color, idx) => {
    const colorDiv = document.createElement("div");
    colorDiv.className = "control-item";
    const label = document.createElement("label");
    label.textContent =
      (idx === 0
        ? "Primary"
        : idx === 1
          ? "Secondary"
          : idx === 2
            ? "Tertiary"
            : `Color ${idx + 1}`) + " Color";
    // Container for swatch and overlay input
    const swatchContainer = document.createElement("div");
    swatchContainer.style.position = "relative";
    swatchContainer.style.width = "100%";
    swatchContainer.style.height = "48px";
    // Swatch styled to look like a color input
    const swatch = document.createElement("span");
    swatch.className = "color-preview-swatch color-input-lookalike";
    swatch.style.background = rgbwHexToPreview(color);
    swatch.style.pointerEvents = "none";
    swatch.style.cursor = "default";
    // Overlay color input (fully covers swatch, visually hidden but interactable)
    const input = document.createElement("input");
    input.type = "color";
    input.className = "color-input color-picker-overlay";
    input.value = color.length === 9 ? color.slice(0, 7) : color;
    input.id = `colorPicker${idx}`;
    input.style.opacity = "0";
    input.style.position = "absolute";
    input.style.top = "0";
    input.style.left = "0";
    input.style.width = "100%";
    input.style.height = "100%";
    input.style.cursor = "pointer";
    input.style.margin = "0";
    input.style.padding = "0";
    input.style.border = "none";
    // When color is picked, update the swatch and state
    let pendingColor = null;
    input.addEventListener("input", (e) => {
      // Only update swatch visually, do not POST
      let newColors = [...colors];
      let orig = newColors[idx] || "#000000";
      let w = orig.length === 9 ? orig.slice(7, 9) : "";
      newColors[idx] = e.target.value + w;
      swatch.style.background = rgbwHexToPreview(newColors[idx]);
      pendingColor = newColors[idx];
    });
    input.addEventListener("change", (e) => {
      // Only POST when picker closes (onchange)
      let newColors = [...colors];
      let orig = newColors[idx] || "#000000";
      let w = orig.length === 9 ? orig.slice(7, 9) : "";
      newColors[idx] = e.target.value + w;
      swatch.style.background = rgbwHexToPreview(newColors[idx]);
      sendState({ params: { colors: newColors } });
      pendingColor = null;
    });
    // Also update swatch if W is changed elsewhere
    setTimeout(() => {
      swatch.style.background = rgbwHexToPreview(color);
    }, 0);
    swatchContainer.appendChild(swatch);
    swatchContainer.appendChild(input);
    label.appendChild(swatchContainer);
    colorDiv.appendChild(label);
    colorPickersRow.appendChild(colorDiv);
  });
}

// Update UI with state from server
function updateState(state) {
  const prevPreset = currentState.preset;
  currentState = state;
  // Show Quick Controls on first WebSocket message
  const quickControls = document.querySelector(".card");
  if (
    quickControls &&
    quickControls.style &&
    quickControls.style.display === "none"
  ) {
    quickControls.style.display = "";
  }

  // Update controls without triggering events
  const powerToggle = document.getElementById("powerToggle");
  if (powerToggle && powerToggle.checked !== state.power) {
    powerToggle.checked = state.power;
  }

  const brightnessSlider = document.getElementById("brightnessSlider");
  if (brightnessSlider) {
    const percent = state.brightness;
    if (brightnessSlider.value != percent) brightnessSlider.value = percent;
    const brightnessValue = document.getElementById("brightnessValue");
    if (brightnessValue) brightnessValue.textContent = percent + "%";
  }

  const effectSelect = document.getElementById("effectSelect");
  if (effectSelect && effectSelect.value != state.effect) {
    effectSelect.value = state.effect;
  }

  // Update transition slider and label from state
  if (typeof state.transitionTime !== "undefined") {
    const transitionSlider = document.getElementById("transitionSlider");
    if (transitionSlider) {
      let transitionSeconds = Math.round(Number(state.transitionTime) / 1000);
      let sliderVal = 0;
      if (transitionSeconds <= 59) sliderVal = transitionSeconds;
      else if (transitionSeconds < 3600)
        sliderVal = 59 + Math.round(transitionSeconds / 60);
      else sliderVal = 119 + Math.round(transitionSeconds / 3600);
      transitionSlider.value = sliderVal;
      const transitionValue = document.getElementById("transitionValue");
      if (transitionValue)
        transitionValue.textContent = formatTransitionTime(transitionSeconds);
    }
  }

  if (state.params) {
    const speedSlider = document.getElementById("speedSlider");
    const speedValue = document.getElementById("speedValue");
    if (speedSlider) speedSlider.value = state.params.speed;
    if (speedValue && speedSlider)
      speedValue.textContent = state.params.speed + "%";

    const intensitySlider = document.getElementById("intensitySlider");
    const intensityValue = document.getElementById("intensityValue");
    if (intensitySlider) intensitySlider.value = state.params.intensity;
    if (intensityValue)
      intensityValue.textContent = state.params.intensity + "%";

    // Dynamically create color pickers
    createColorPickers(state.params.colors, state.params);
  }

  // Synchronize clock with backend on first WS message, then advance locally
  if (state.time) {
    if (!clockSynced) {
      const currentTime = document.getElementById("currentTime");
      if (currentTime) currentTime.textContent = state.time;
      // Parse time as HH:MM:SS
      const [h, m, s] = state.time.split(":").map(Number);
      const now = new Date();
      localClock = new Date(
        now.getFullYear(),
        now.getMonth(),
        now.getDate(),
        h,
        m,
        s,
      );
      if (clockInterval) clearInterval(clockInterval);
      clockInterval = setInterval(() => {
        localClock.setSeconds(localClock.getSeconds() + 1);
        const hh = localClock.getHours().toString().padStart(2, "0");
        const mm = localClock.getMinutes().toString().padStart(2, "0");
        const ss = localClock.getSeconds().toString().padStart(2, "0");
        const currentTime = document.getElementById("currentTime");
        if (currentTime) currentTime.textContent = `${hh}:${mm}:${ss}`;
      }, 1000);
      clockSynced = true;
    }
  }

  if (state.sunrise) {
    const sunriseTime = document.getElementById("sunriseTime");
    if (sunriseTime) sunriseTime.textContent = state.sunrise;
  }

  if (state.sunset) {
    const sunsetTime = document.getElementById("sunsetTime");
    if (sunsetTime) sunsetTime.textContent = state.sunset;
  }

  // Highlight active preset
  if (state.preset !== undefined) {
    document.querySelectorAll(".preset-card").forEach((card, index) => {
      if (index === state.preset) {
        card.classList.add("active");
      } else {
        card.classList.remove("active");
      }
    });
  }

  // Only redraw preset cards if preset changed
  if (state.preset !== undefined && state.preset !== cachedPreset) {
    displayPresets();
  } else {
    // Just update active class
    document.querySelectorAll(".preset-card").forEach((card, index) => {
      if (index === state.preset) {
        card.classList.add("active");
      } else {
        card.classList.remove("active");
      }
    });
  }
}

// Setup event listeners
function setupEventListeners() {
  // Power toggle
  const powerToggle = document.getElementById("powerToggle");
  // Transition time slider (stepped)
  const transitionSlider = document.getElementById("transitionSlider");
  const transitionValue = document.getElementById("transitionValue");
  if (powerToggle) {
    powerToggle.addEventListener("change", (e) => {
      sendState({ power: e.target.checked });
    });
  }
  // Brightness slider (percent 0–100%)
  const brightnessSlider = document.getElementById("brightnessSlider");
  if (brightnessSlider) {
    brightnessSlider.min = 0;
    brightnessSlider.max = 100;
    brightnessSlider.step = 1;
    brightnessSlider.addEventListener("input", (e) => {
      const percent = parseInt(e.target.value);
      const brightnessValue = document.getElementById("brightnessValue");
      if (brightnessValue) brightnessValue.textContent = percent + "%";
    });
    brightnessSlider.addEventListener("change", (e) => {
      const percent = parseInt(e.target.value);
      sendState({ brightness: percent });
    });
  }
  if (transitionSlider && transitionValue) {
    transitionSlider.min = 0;
    transitionSlider.max = 127;
    transitionSlider.step = 1;
    // Set initial value
    let initial = transitionSlider.value;
    let seconds = steppedTransitionValue(initial);
    transitionSlider.value = initial;
    transitionValue.textContent = formatTransitionTime(seconds);
    transitionSlider.addEventListener("input", (e) => {
      let seconds = steppedTransitionValue(e.target.value);
      transitionSlider.value = e.target.value;
      transitionValue.textContent = formatTransitionTime(seconds);
    });
    transitionSlider.addEventListener("change", (e) => {
      let seconds = steppedTransitionValue(e.target.value);
      transitionSlider.value = e.target.value;
      transitionValue.textContent = formatTransitionTime(seconds);
      sendState({ transitionTime: seconds * 1000 });
    });
    // On load, set slider to match state (EXACTLY as in config.js)
    if (typeof currentState.transitionTime !== "undefined") {
      const ms = Number(currentState.transitionTime);
      let sec = Math.round(ms / 1000);
      let sliderVal = 0;
      if (sec <= 59) sliderVal = sec;
      else if (sec < 3600) sliderVal = 59 + Math.round(sec / 60);
      else sliderVal = 119 + Math.round(sec / 3600);
      transitionSlider.value = sliderVal;
      transitionValue.textContent = formatTransitionTime(sec);
    }
  }

  // Effect selector
  const effectSelect = document.getElementById("effectSelect");
  if (effectSelect) {
    effectSelect.addEventListener("change", (e) => {
      sendState({ effect: parseInt(e.target.value) });
    });
  }
  // Speed and intensity slider event listeners are now handled exclusively by setupSlider below to avoid duplicate POSTs.
  // Unified slider setup
  function setupSlider({
    id,
    valueId,
    min = 0,
    max = 100,
    step = 1,
    initial,
    format = (v) => v,
    onInput,
    onChange,
  }) {
    const slider = document.getElementById(id);
    const valueEl = valueId ? document.getElementById(valueId) : null;
    if (!slider) return;
    slider.min = min;
    slider.max = max;
    slider.step = step;
    if (typeof initial !== "undefined") slider.value = initial;
    if (valueEl && typeof initial !== "undefined")
      valueEl.textContent = format(initial);
    if (onInput) {
      slider.addEventListener("input", (e) => {
        if (valueEl) valueEl.textContent = format(e.target.value);
        onInput(e);
      });
    }
    if (onChange) {
      slider.addEventListener("change", (e) => {
        if (valueEl) valueEl.textContent = format(e.target.value);
        onChange(e);
      });
    }
  }

  // Brightness slider
  setupSlider({
    id: "brightnessSlider",
    valueId: "brightnessValue",
    min: 0,
    max: 100,
    step: 1,
    initial: currentState.brightness,
    format: (v) => v + "%",
    onInput: () => {},
    onChange: (e) => {
      sendState({ brightness: parseInt(e.target.value) });
    },
  });

  // Transition time slider
  setupSlider({
    id: "transitionSlider",
    valueId: "transitionValue",
    min: 0,
    max: 127,
    step: 1,
    initial: (() => {
      if (typeof currentState.transitionTime !== "undefined") {
        const ms = Number(currentState.transitionTime);
        let sec = Math.round(ms / 1000);
        if (sec <= 59) return sec;
        else if (sec < 3600) return 59 + Math.round(sec / 60);
        else return 119 + Math.round(sec / 3600);
      }
      return 0;
    })(),
    format: (v) => formatTransitionTime(steppedTransitionValue(v)),
    onInput: (e) => {},
    onChange: (e) => {
      let seconds = steppedTransitionValue(e.target.value);
      sendState({ transitionTime: seconds * 1000 });
    },
  });

  // Speed slider
  setupSlider({
    id: "speedSlider",
    valueId: "speedValue",
    min: 0,
    max: 100,
    step: 1,
    initial: currentState.params && currentState.params.speed,
    format: (v) => v + "%",
    onInput: (() => {
      let speedTimeout;
      return (e) => {
        clearTimeout(speedTimeout);
        speedTimeout = setTimeout(() => {
          sendState({ params: { speed: parseInt(e.target.value) } });
        }, 300);
      };
    })(),
    onChange: null,
  });

  // Intensity slider
  setupSlider({
    id: "intensitySlider",
    valueId: "intensityValue",
    min: 0,
    max: 100,
    step: 1,
    initial: currentState.params && currentState.params.intensity,
    format: (v) => v + "%",
    onInput: (() => {
      let intensityTimeout;
      return (e) => {
        clearTimeout(intensityTimeout);
        intensityTimeout = setTimeout(() => {
          sendState({ params: { intensity: parseInt(e.target.value) } });
        }, 100);
      };
    })(),
    onChange: null,
  });
  // No-op: color picker visibility is now handled dynamically in createColorPickers
  // Patch updateState to maintain compatibility, but do nothing
  const origUpdateState = window.updateState;
  window.updateState = function (state) {
    origUpdateState(state);
  };
}

// Send state update to server
function sendState(updates) {
  fetch(BASE_URL + "/api/state", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify(updates),
  })
    .then(async (response) => {
      const text = await response.text();
      if (!text) return {};
      try {
        return JSON.parse(text);
      } catch {
        return {};
      }
    })
    .then((data) => {
      // Treat empty response or missing success property as success
      if (data && typeof data.success !== "undefined" && !data.success) {
        console.error("Failed to update state");
      }
    })
    .catch((error) => console.error("Error:", error));
}

// Load and display presets
// Home page (index.html) only: loadPresets and loadTimers for independent schedule/preset display
function loadPresets() {
  fetch(BASE_URL + "/api/presets")
    .then(async (response) => {
      const text = await response.text();
      if (!text) return {};
      try {
        return JSON.parse(text);
      } catch {
        return {};
      }
    })
    .then((data) => {
      presets = Array.isArray(data) ? data : data.presets || [];
      if (typeof displayPresets === "function") displayPresets();
      if (typeof renderBrightnessGraph === "function") renderBrightnessGraph();
      // If timers are already loaded, update timer table
      if (
        Array.isArray(timers) &&
        timers.length > 0 &&
        typeof displayTimers === "function"
      )
        displayTimers();
    })
    .catch((error) => console.error("Error loading presets:", error));
}

function loadTimers() {
  fetch(BASE_URL + "/api/timers")
    .then(async (response) => {
      const text = await response.text();
      if (!text) return {};
      try {
        return JSON.parse(text);
      } catch {
        return {};
      }
    })
    .then((data) => {
      timers = Array.isArray(data) ? data : data.timers || [];
      // Only display timers if presets are loaded
      if (
        Array.isArray(presets) &&
        presets.length > 0 &&
        typeof displayTimers === "function"
      )
        displayTimers();
      if (typeof renderBrightnessGraph === "function") renderBrightnessGraph();
    })
    .catch((error) => console.error("Error loading timers:", error));
}

function getPresetPreviewStyle(colors) {
  if (!colors || !Array.isArray(colors) || colors.length === 0) {
    return "background: #000000;";
  }
  const previewColors = colors.map((c) => rgbwHexToPreview(c));
  if (previewColors.length > 1) {
    return `background: linear-gradient(135deg, ${previewColors.join(", ")});`;
  } else if (previewColors.length === 1) {
    return `background: ${previewColors[0]};`;
  } else {
    return "background: #000000;";
  }
}

function displayPresets() {
  const grid = document.getElementById("presetGrid");
  if (!grid) return;
  grid.innerHTML = "";
  presets.forEach((preset) => {
    if (!preset.enabled && preset.id > 0) return;
    const card = document.createElement("div");
    card.className = "preset-card";
    // Highlight if this preset is active (by id)
    if (
      currentState.preset !== undefined &&
      presets.find((p) => p.id === currentState.preset)?.id === preset.id
    ) {
      card.classList.add("active");
    }
    const effectName = effectNames[preset.effect] || `Effect #${preset.effect}`;
    // Use helper for preview style
    let colorArr =
      preset.params && Array.isArray(preset.params.colors)
        ? preset.params.colors
        : [];
    let previewStyle = getPresetPreviewStyle(colorArr);
    card.innerHTML = `
            <div class="preset-name">${preset.name}</div>
            <div class="preset-info">Effect: ${effectName}</div>
            <div class="preset-color-preview" style="${previewStyle}"></div>
        `;
    card.addEventListener("click", () => applyPreset(preset.id));
    grid.appendChild(card);
  });
  cachedPreset = currentState.preset;
}

function applyPreset(presetId) {
  fetch(BASE_URL + "/api/preset", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify({
      id: presetId,
      apply: true,
    }),
  })
    .then(async (response) => {
      const text = await response.text();
      if (!text) return {};
      try {
        return JSON.parse(text);
      } catch {
        return {};
      }
    })
    .then((data) => {
      if (data.success) {
        // Optionally update currentState.preset to presetId for immediate UI feedback
        currentState.preset = presetId;
        displayPresets();
        console.log("Preset applied:", presetId);
      }
    })
    .catch((error) => console.error("Error applying preset:", error));
}

// Load and display timers
// Removed loadTimers (timers are part of config API)
// Render a 24h brightness graph reflecting scheduled preset brightness
function renderBrightnessGraph() {
  // Plugin to draw colored rectangles for each hour as chart background
  const hourlyBackgroundPlugin = {
    id: "hourlyBackground",
    beforeDatasetsDraw: (chart) => {
      const { ctx, chartArea, scales } = chart;
      if (!chartArea) return;
      const xAxis = scales.x;
      const meta = chart.getDatasetMeta(0);
      if (!meta || !meta.data) return;
      for (let hour = 0; hour < 24; hour++) {
        const x0 = xAxis.getPixelForValue(hour);
        const x1 = xAxis.getPixelForValue(hour + 1);
        // Find Y on the line at x0 and x1 (interpolate if needed)
        let y0 = chartArea.bottom;
        let y1 = chartArea.bottom;
        for (let j = 0; j < meta.data.length - 1; j++) {
          const p1 = meta.data[j];
          const p2 = meta.data[j + 1];
          if (p1.x <= x0 && p2.x >= x0) {
            const t = (x0 - p1.x) / (p2.x - p1.x);
            y0 = p1.y + t * (p2.y - p1.y);
            break;
          }
        }
        for (let j = 0; j < meta.data.length - 1; j++) {
          const p1 = meta.data[j];
          const p2 = meta.data[j + 1];
          if (p1.x <= x1 && p2.x >= x1) {
            const t = (x1 - p1.x) / (p2.x - p1.x);
            y1 = p1.y + t * (p2.y - p1.y);
            break;
          }
        }
        ctx.save();
        // Create a horizontal gradient between this hour and the next
        const colorStart = bgColors[hour - 1] || "rgba(0,116,217,0.08)";
        const colorEnd =
          bgColors[hour] || bgColors[hour] || "rgba(0,116,217,0.08)";
        const grad = ctx.createLinearGradient(x0, 0, x1, 0);
        grad.addColorStop(0, colorStart);
        grad.addColorStop(1, colorEnd);
        ctx.fillStyle = grad;
        ctx.globalAlpha = 0.6;
        ctx.beginPath();
        ctx.moveTo(x0, y0);
        ctx.lineTo(x1, y1);
        ctx.lineTo(x1, chartArea.bottom);
        ctx.lineTo(x0, chartArea.bottom);
        ctx.closePath();
        ctx.fill();
        ctx.restore();
      }
    },
  };
  const ctx = document.getElementById("brightnessGraph");
  if (
    !ctx ||
    !Array.isArray(timers) ||
    !Array.isArray(presets) ||
    presets.length === 0
  )
    return;

  // --- NEW: Use only calculated shift points for graph ---
  const minutesPerDay = 24 * 60;
  const events = timers
    .filter(
      (timer) =>
        timer.enabled &&
        typeof timer.hour === "number" &&
        typeof timer.minute === "number" &&
        typeof timer.brightness === "number",
    )
    .map((timer, idx) => ({
      time: timer.hour * 60 + timer.minute,
      brightness: timer.brightness,
      index: idx,
    }))
    .sort((a, b) => a.time - b.time);

  if (events.length === 0) {
    if (window.brightnessChart) {
      window.brightnessChart.destroy();
      window.brightnessChart = null;
    }
    return;
  }

  let transitionDuration;
  if (
    window.config &&
    window.config.transitionTimes &&
    typeof window.config.transitionTimes.schedule === "number"
  ) {
    transitionDuration = window.config.transitionTimes.schedule / 60000;
  } else {
    alert(
      "Missing config.transitionTimes.schedule! Brightness graph cannot show transitions correctly.",
    );
    return;
  }

  // Build key points: event times, ramp ends, and explicit start/end of day
  let n = events.length;
  let points = [];
  for (let i = 0; i < n; i++) {
    let prevIdx = (i - 1 + n) % n;
    let tCurr = events[i].time;
    let tPrev = events[prevIdx].time;
    let bCurr = events[i].brightness;
    let bPrev = events[prevIdx].brightness;
    // At event time: start ramp from bPrev to bCurr
    points.push({
      time: tCurr,
      brightness: bPrev,
    });
    // At ramp end: reach bCurr
    let rampEnd = (tCurr + transitionDuration) % minutesPerDay;
    points.push({
      time: rampEnd,
      brightness: bCurr,
    });
  }
  // --- Calculate correct brightness at 00:00 and 24:00 for wrap-around transition ---
  const firstEvent = events[0];
  const lastEvent = events[n - 1];
  const prevEvent = events[(n - 2 + n) % n];
  const t0 = firstEvent.time;
  const t1 = lastEvent.time;
  const bStart = prevEvent.brightness;
  const bEnd = lastEvent.brightness;
  let midnightBrightness = bEnd;
  // If the last ramp crosses midnight, interpolate
  if (t1 + transitionDuration > minutesPerDay && t1 !== t0) {
    const frac = (minutesPerDay - t1) / transitionDuration;
    midnightBrightness = bStart + (bEnd - bStart) * frac;
  }
  points.push({ time: 0, brightness: midnightBrightness });
  points.push({ time: minutesPerDay, brightness: midnightBrightness });
  // Sort points by time (wrap-around)
  points.sort((a, b) => a.time - b.time);
  // Remove duplicate time values (keep last occurrence)
  let uniquePoints = [];
  let seen = new Set();
  for (let i = points.length - 1; i >= 0; i--) {
    if (!seen.has(points[i].time)) {
      uniquePoints.unshift(points[i]);
      seen.add(points[i].time);
    }
  }

  // Build labels for every hour (00:00, 01:00, ..., 24:00)
  const labels = [];
  const data = [];
  const bgColors = [];
  for (let hour = 0; hour <= 24; hour++) {
    labels.push(hour.toString().padStart(2, "0") + ":00");
    let minute = hour * 60;
    // Find the two points surrounding this minute
    let prev = uniquePoints[0];
    let next = uniquePoints[uniquePoints.length - 1];
    for (let i = 0; i < uniquePoints.length; i++) {
      if (uniquePoints[i].time <= minute) prev = uniquePoints[i];
      if (uniquePoints[i].time > minute) {
        next = uniquePoints[i];
        break;
      }
    }
    // Linear interpolate if between points, else use prev brightness
    let val;
    if (next.time !== prev.time && minute > prev.time && minute < next.time) {
      let frac = (minute - prev.time) / (next.time - prev.time);
      val = prev.brightness + (next.brightness - prev.brightness) * frac;
    } else {
      val = prev.brightness;
    }
    data.push(val);
    // Find active timer for this hour (fallback to previous if missing)
    let activeTimer = null;
    for (let i = 0; i < timers.length; i++) {
      let tMin = timers[i].hour * 60 + timers[i].minute;
      if (tMin <= minute) activeTimer = timers[i];
    }
    if (!activeTimer) activeTimer = timers[0];
    // Get primary color from preset, fallback to previous preset if missing
    let preset = presets.find((p) => p.id === activeTimer.presetId);
    if (!preset && presets.length > 0) preset = presets[0];
    let primaryColor =
      preset && preset.params && preset.params.colors && preset.params.colors[0]
        ? preset.params.colors[0]
        : null;
    // Convert to preview color, fallback to blue if invalid
    let previewColor = primaryColor
      ? rgbwHexToPreview(primaryColor)
      : "rgb(0,116,217)";
    // If still black or invalid, use a visible fallback
    if (!previewColor || previewColor === "rgb(0,0,0)")
      previewColor = "rgba(0,116,217,0.15)";
    console.log(
      `[BGColor] Hour ${hour}: timer`,
      activeTimer,
      "preset",
      preset,
      "primaryColor",
      primaryColor,
      "previewColor",
      previewColor,
    );
    bgColors.push(previewColor);
  }

  let minutesNow = 0;
  if (
    currentState &&
    typeof currentState.time === "string" &&
    /^\d{2}:\d{2}:\d{2}$/.test(currentState.time)
  ) {
    const [h, m, s] = currentState.time.split(":").map(Number);
    minutesNow = h * 60 + m;
  } else {
    const now = new Date();
    minutesNow = now.getHours() * 60 + now.getMinutes();
  }
  // Use fractional hour for precise vertical line
  const currentHour = minutesNow / 60;

  // Draw chart with vertical line for current time
  const annotationPlugin = {
    id: "currentTimeLine",
    afterDraw: (chart) => {
      if (
        typeof currentHour !== "number" ||
        currentHour < 0 ||
        currentHour > 24
      )
        return;
      const ctx = chart.ctx;
      const xAxis = chart.scales.x;
      const yAxis = chart.scales.y;
      const x = xAxis.getPixelForValue(currentHour);
      ctx.save();
      ctx.beginPath();
      ctx.moveTo(x, yAxis.top);
      ctx.lineTo(x, yAxis.bottom);
      ctx.lineWidth = 2;
      ctx.strokeStyle = "#FF4136";
      ctx.setLineDash([4, 4]);
      ctx.stroke();
      ctx.setLineDash([]);
      ctx.restore();
    },
  };

  if (window.brightnessChart) {
    window.brightnessChart.data.labels = labels;
    window.brightnessChart.data.datasets[0].data = data;
    window.brightnessChart.options.plugins.currentTimeLine = annotationPlugin;
    window.brightnessChart.update();
  } else {
    window.brightnessChart = new Chart(ctx, {
      type: "line",
      data: {
        labels: labels,
        datasets: [
          {
            label: "Brightness (%)",
            data: data,
            borderColor: "#0074D9",
            backgroundColor: "rgba(0,0,0,0)", // transparent, handled by plugin
            fill: true,
            pointRadius: 0,
            borderWidth: 2,
            tension: 0, // No curve, straight lines
          },
        ],
      },
      options: {
        responsive: true,
        plugins: {
          legend: { display: false },
          title: { display: false },
          currentTimeLine: annotationPlugin,
        },
        scales: {
          x: {
            title: { display: true, text: "Time (24h)" },
            ticks: { autoSkip: false, maxTicksLimit: 25 },
          },
          y: {
            title: { display: true, text: "Brightness (%)" },
            min: 0,
            max: 100,
          },
        },
      },
      plugins: [hourlyBackgroundPlugin, annotationPlugin],
    });
  }
}

function displayTimers() {
  const tableContainer = document.getElementById("scheduleTable");
  if (!tableContainer) return;
  tableContainer.innerHTML = "";

  // Elegant table markup
  const table = document.createElement("table");
  table.className = "schedule-table-inner";
  const thead = document.createElement("thead");
  thead.innerHTML = `
        <tr>
            <th>Time</th>
            <th>Preset</th>
            <th>Brightness</th>
            <th>Status</th>
        </tr>
    `;
  table.appendChild(thead);
  const tbody = document.createElement("tbody");

  const effectNames = [
    "Solid",
    "Ripple",
    "Wave",
    "Sunrise",
    "Shimmer",
    "Deep Ocean",
    "Moonlight",
  ];

  // Use server time for schedule highlight
  let activePresetId = null;
  let nowMinutes = 0;
  if (
    currentState &&
    typeof currentState.time === "string" &&
    /^\d{2}:\d{2}:\d{2}$/.test(currentState.time)
  ) {
    const [h, m, s] = currentState.time.split(":").map(Number);
    nowMinutes = h * 60 + m;
  } else {
    let now = new Date();
    nowMinutes = now.getHours() * 60 + now.getMinutes();
  }
  let lastTimerIdx = -1;
  let lastTimerTime = -1;
  timers.forEach((timer, idx) => {
    if (
      !timer.enabled ||
      typeof timer.hour !== "number" ||
      typeof timer.minute !== "number"
    )
      return;
    let timerTime = timer.hour * 60 + timer.minute;
    if (timerTime <= nowMinutes && timerTime > lastTimerTime) {
      lastTimerTime = timerTime;
      lastTimerIdx = idx;
    }
  });
  if (
    lastTimerIdx >= 0 &&
    timers[lastTimerIdx] &&
    typeof timers[lastTimerIdx].presetId === "number"
  ) {
    activePresetId = timers[lastTimerIdx].presetId;
  }

  timers.forEach((timer, index) => {
    if (!timer.enabled && !timer.name && !timer.hour && !timer.minute) return;
    const name = timer.name || `Timer ${index + 1}`;
    let timeStr = "--:--";
    if (typeof timer.hour === "number" && typeof timer.minute === "number") {
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
      // Only find by id property (robust)
      let found = presets.find(
        (p) => p.id === timer.presetId || p.presetId === timer.presetId,
      );
      if (found && found.name) {
        presetStr = found.name;
      } else {
        presetStr = `Preset ${timer.presetId}`;
      }
    }
    const statusStr = timer.enabled
      ? '<span class="timer-enabled">Enabled</span>'
      : '<span class="timer-disabled">Disabled</span>';
    const brightStr =
      typeof timer.brightness === "number" ? `${timer.brightness}%` : "--";
    const row = document.createElement("tr");
    row.innerHTML = `
            <td>${timeStr}</td>
            <td>${presetStr}</td>
            <td>${brightStr}</td>
            <td>${statusStr}</td>
        `;
    // Highlight only the timer row that matches the current time (lastTimerIdx)
    if (index === lastTimerIdx) {
      row.classList.add("active-timer-row");
    }
    // Timer editing disabled: no editTimer UI present
    tbody.appendChild(row);
  });
  table.appendChild(tbody);
  tableContainer.appendChild(table);
}

// Toast notifications
function showToast(message, duration = 3000) {
  const toast = document.createElement("div");
  toast.className = "toast";
  toast.textContent = message;
  document.body.appendChild(toast);

  setTimeout(() => {
    toast.classList.add("show");
  }, 100);

  setTimeout(() => {
    toast.classList.remove("show");
    setTimeout(() => {
      document.body.removeChild(toast);
    }, 300);
  }, duration);
}

// Initialize on page load
document.addEventListener("DOMContentLoaded", () => {
  // Hide Quick Controls until first WebSocket message
  const quickControls = document.querySelector(".card");
  if (quickControls && quickControls.style)
    quickControls.style.display = "none";
  // Only connect WebSocket if not on config.html
  if (!window.location.pathname.endsWith("config.html")) {
    initializeWebSocket();
  }
  setupEventListeners();

  // Only load effects, presets, timers, and config on the home page (index.html)
  if (
    window.location.pathname.endsWith("index.html") ||
    window.location.pathname === "/" ||
    window.location.pathname === "/index.html"
  ) {
    // Fetch config first
    fetch(BASE_URL + "/api/config")
      .then((r) => r.json())
      .then((cfg) => {
        window.config = cfg;
        // Now load effects, presets, and timers
        loadEffects().then(() => {
          if (typeof loadPresets === "function") loadPresets();
          if (typeof loadTimers === "function") loadTimers();
        });
        // Fetch and display version
        fetch(BASE_URL + "/api/version")
          .then((r) => r.json())
          .then((data) => {
            if (data && data.version) {
              const vEl = document.getElementById("versionString");
              if (vEl) vEl.textContent = "Version: " + data.version;
            }
          })
          .catch(() => {
            const vEl = document.getElementById("versionString");
            if (vEl) vEl.textContent = "Version: (unavailable)";
          });
      })
      .catch(() => {
        alert(
          "Failed to load config from backend. Brightness graph and transitions may not work correctly.",
        );
      });
  }

  // Load all available effects from backend
  function loadEffects() {
    return fetch(BASE_URL + "/api/effects")
      .then(async (response) => {
        const text = await response.text();
        if (!text) return [];
        try {
          return JSON.parse(text);
        } catch {
          return [];
        }
      })
      .then((data) => {
        let effects = data && data.effects ? data.effects : [];
        effectNames = effects.map((e) => e.name);
        // Populate effectSelect dropdown
        const effectSelect = document.getElementById("effectSelect");
        if (effectSelect) {
          effectSelect.innerHTML = "";
          effects.forEach((e) => {
            const opt = document.createElement("option");
            opt.value = e.id;
            opt.textContent = e.name;
            effectSelect.appendChild(opt);
          });
        }
      })
      .catch((error) => console.error("Error loading effects:", error));
  }
});
