import { useEffect, useRef } from "preact/hooks";
import { rgbwHexToPreview } from "./util.js";

export function Graph({ state, timers, presets, config }) {
  const brightnessGraphRef = useRef(null);
  useEffect(() => {
    if (!brightnessGraphRef.current) return;
    if (
      !config ||
      !config.transitionTimes ||
      typeof config.transitionTimes.schedule !== "number"
    )
      return;

    // Explicitly set canvas height and width to avoid Chart.js sizing bugs
    brightnessGraphRef.current.style.height = "220px";
    brightnessGraphRef.current.height = 220;
    brightnessGraphRef.current.style.width = "100%";
    brightnessGraphRef.current.width =
      brightnessGraphRef.current.offsetWidth || 600;

    function renderChart() {
      // --- Legacy math: build points for ramp transitions ---
      const minutesPerDay = 24 * 60;
      let events = Array.isArray(timers)
        ? timers
            .filter(
              (t) =>
                t.enabled &&
                typeof t.hour === "number" &&
                typeof t.minute === "number" &&
                typeof t.brightness === "number",
            )
            .map((t, idx) => ({
              time: t.hour * 60 + t.minute,
              brightness: t.brightness,
              index: idx,
            }))
            .sort((a, b) => a.time - b.time)
        : [];

      // Calculate actual time in minutes
      const now = new Date();
      const actualMinutes =
        now.getHours() * 60 + now.getMinutes() + now.getSeconds() / 60;

      // Chart.js plugin for vertical red line at actual time
      const actualTimeLinePlugin = {
        id: "actualTimeLine",
        afterDraw: (chart) => {
          const { ctx, chartArea, scales } = chart;
          if (!chartArea) return;
          const x = scales.x.getPixelForValue(actualMinutes);
          ctx.save();
          ctx.beginPath();
          ctx.setLineDash([6, 6]); // Dashed line: 6px dash, 6px gap
          ctx.moveTo(x, chartArea.top);
          ctx.lineTo(x, chartArea.bottom);
          ctx.lineWidth = 2;
          ctx.strokeStyle = "red";
          ctx.globalAlpha = 0.9;
          ctx.stroke();
          ctx.setLineDash([]); // Reset to solid for other drawing
          ctx.restore();
        },
      };

      // Build chart data
      let chartData, chartOptions, chartPlugins;
      if (events.length === 0) {
        // No timers: flat line
        const hours = Array.from({ length: 25 }, (_, h) => h);
        const flat = hours.map(() => state.brightness || 0);
        chartData = {
          labels: hours.map((h) => `${h.toString().padStart(2, "0")}:00`),
          datasets: [
            {
              label: "Brightness (%)",
              data: flat,
              borderColor: "#66ccff",
              backgroundColor: "rgba(102,204,255,0.2)",
              fill: true,
              tension: 0,
              stepped: true,
            },
          ],
        };
        chartOptions = {
          responsive: true,
          plugins: {
            legend: { display: false },
            actualTimeLine: actualTimeLinePlugin,
          },
          scales: {
            x: {
              display: true,
              title: { display: true, text: "Time (24h)" },
              min: 0,
              max: 24,
            },
            y: {
              display: true,
              title: { display: true, text: "Brightness (%)" },
              min: 0,
              max: 100,
            },
          },
        };
        chartPlugins = [actualTimeLinePlugin];
      } else {
        // Timers present: build ramp/step data
        // transitionTimes.schedule is in ms, convert to minutes (float)
        let transitionDuration = config.transitionTimes.schedule / 1000 / 60;
        // Clamp to at least 0.01 to avoid zero-length ramps, but do not force to 1 minute
        transitionDuration = Math.max(0.01, transitionDuration);
        // Round to 2 decimals for visual accuracy
        transitionDuration = Math.round(transitionDuration * 100) / 100;
        let n = events.length;
        let points = [];
        for (let i = 0; i < n; i++) {
          let prevIdx = (i - 1 + n) % n;
          let nextIdx = (i + 1) % n;
          let tCurr = events[i].time;
          let tPrev = events[prevIdx].time;
          let tNext = events[nextIdx].time;
          let bCurr = events[i].brightness;
          let bPrev = events[prevIdx].brightness;
          // Calculate ramp end: either transitionDuration after tCurr, or next event, whichever is sooner
          let rampEnd = tCurr + transitionDuration;
          if (rampEnd > minutesPerDay) rampEnd -= minutesPerDay;
          let tCurrToNext =
            tNext > tCurr ? tNext - tCurr : minutesPerDay - tCurr + tNext;
          if (transitionDuration > tCurrToNext) {
            rampEnd = tNext;
          }
          // For previous event, determine its intended ramp end and target brightness
          let prevIntendedEnd = tPrev + transitionDuration;
          if (prevIntendedEnd > minutesPerDay) prevIntendedEnd -= minutesPerDay;
          // Find the intended target for previous ramp: next event after prevIdx (wrap around)
          // prevTargetIdx should be the event after the previous event (wrap around)
          let prevTargetIdx = (prevIdx - 1 + n) % n;
          // Always use the intended target of the previous ramp for interpolation
          let bPrevTarget = events[prevTargetIdx].brightness;
          let prevRampDuration =
            prevIntendedEnd > tPrev
              ? prevIntendedEnd - tPrev
              : minutesPerDay - tPrev + prevIntendedEnd;
          let timeFromPrevToCurr =
            tCurr > tPrev ? tCurr - tPrev : minutesPerDay - tPrev + tCurr;
          let startBrightness = bPrev;
          let frac = 0;
          if (timeFromPrevToCurr < prevRampDuration && prevRampDuration > 0) {
            frac = timeFromPrevToCurr / prevRampDuration;
            startBrightness = bPrev + (bPrevTarget - bPrev) * (1 - frac);
          }
          points.push({ time: tCurr, brightness: startBrightness });
          points.push({ time: rampEnd, brightness: bCurr });
        }
        // Calculate brightness at 0:00 and 24:00 for wrap-around
        const firstEvent = events[0];
        const lastEvent = events[n - 1];
        const prevEvent = events[(n - 2 + n) % n];
        const t0 = firstEvent.time;
        const t1 = lastEvent.time;
        const bStart = prevEvent.brightness;
        const bEnd = lastEvent.brightness;
        let midnightBrightness = bEnd;
        // If last ramp ends after midnight, interpolate brightness at 0:00
        if (t1 + transitionDuration > minutesPerDay && t1 !== t0) {
          const frac = (minutesPerDay - t1) / transitionDuration;
          midnightBrightness = bStart + (bEnd - bStart) * frac;
        }
        points.push({ time: 0, brightness: midnightBrightness });
        points.push({ time: minutesPerDay, brightness: midnightBrightness });
        // Sort points by time
        points.sort((a, b) => a.time - b.time);
        // Remove duplicate times (keep last occurrence)
        let uniquePoints = [];
        let seen = new Set();
        for (let i = points.length - 1; i >= 0; i--) {
          if (!seen.has(points[i].time)) {
            uniquePoints.unshift(points[i]);
            seen.add(points[i].time);
          }
        }
        // Only plot dots at relevant points: event times and ramp ends (uniquePoints)
        // uniquePoints is already sorted and deduplicated
        // Use exact time (in minutes) for X axis, not just index
        const pointsForChart = uniquePoints.map((p) => ({
          x: p.time,
          y: Math.min(100, p.brightness),
        }));
        // For X axis labels, show HH:MM at each event/ramp point
        const timeToLabel = (minute) => {
          const h = Math.floor(minute / 60);
          return `${h.toString().padStart(2, "0")}` + "h";
        };
        const minutesArray = uniquePoints.map((p) => p.time);
        // --- Chart.js plugin for colorized background by 1-hour block ---
        // Use minutesArray for background color to match data
        const bgBlocks = minutesArray;
        const bgColors = bgBlocks.map((minute) => {
          let activeTimer = null;
          if (Array.isArray(timers)) {
            for (let i = 0; i < timers.length; i++) {
              let t = timers[i];
              if (
                !t.enabled ||
                typeof t.hour !== "number" ||
                typeof t.minute !== "number"
              )
                continue;
              let tMin = t.hour * 60 + t.minute;
              if (tMin <= minute) activeTimer = t;
            }
            if (!activeTimer) activeTimer = timers[0];
          }
          let preset =
            presets && activeTimer
              ? presets.find((p) => p.id === activeTimer.presetId)
              : null;
          if (!preset && presets && presets.length > 0) preset = presets[0];
          let primaryColor =
            preset &&
            preset.params &&
            preset.params.colors &&
            preset.params.colors[0]
              ? preset.params.colors[0]
              : null;
          let previewColor = primaryColor
            ? rgbwHexToPreview(primaryColor)
            : "rgb(0,116,217)";
          return previewColor;
        });

        const colorBgPlugin = {
          id: "colorBgByBlock",
          beforeDatasetsDraw: (chart) => {
            const { ctx, chartArea, scales, data } = chart;
            if (!chartArea) return;
            const dataset = chart.data.datasets[0];
            const points = dataset.data;
            for (let i = 0; i < bgBlocks.length - 1; i++) {
              // Use the true X value (minutes) for linear scale
              const x0 = scales.x.getPixelForValue(points[i].x);
              const x1 = scales.x.getPixelForValue(points[i + 1].x);
              const y0 = scales.y.getPixelForValue(points[i].y);
              const y1 = scales.y.getPixelForValue(points[i + 1].y);
              ctx.save();
              ctx.beginPath();
              ctx.moveTo(x0, chartArea.bottom);
              ctx.lineTo(x0, y0);
              ctx.lineTo(x1, y1);
              ctx.lineTo(x1, chartArea.bottom);
              ctx.closePath();
              const grad = ctx.createLinearGradient(
                x0,
                chartArea.bottom,
                x1,
                chartArea.bottom,
              );
              grad.addColorStop(0, bgColors[i - 1] || bgColors[i]);
              grad.addColorStop(1, bgColors[i]);
              ctx.fillStyle = grad;
              ctx.globalAlpha = 0.6;
              ctx.fill();
              ctx.restore();
            }
          },
        };

        chartData = {
          datasets: [
            {
              label: "Brightness (%)",
              data: pointsForChart,
              borderColor: "#66ccff",
              backgroundColor: "rgba(102,204,255,0.2)",
              fill: true,
              tension: 0,
              parsing: false, // Use x/y objects
            },
          ],
        };
        chartOptions = {
          responsive: true,
          plugins: {
            legend: { display: false },
            colorBgByBlock: colorBgPlugin,
          },
          elements: {
            line: { showLine: true },
            point: { radius: 4, pointStyle: "circle" },
          },
          scales: {
            x: {
              type: "linear",
              display: true,
              title: { display: true, text: "Time (24h)" },
              min: 0,
              max: 1440,
              ticks: {
                stepSize: 60,
                callback: function (value, index, ticks) {
                  // Show label at each event/ramp point and on the hour
                  if (minutesArray.includes(value)) return timeToLabel(value);
                  if (value % 60 === 0) return timeToLabel(value);
                  return "";
                },
              },
            },
            y: {
              display: true,
              min: 0,
              max: 100,
              title: {
                display: true,
                text: "Brightness (%)",
              },
            },
          },
        };
        chartPlugins = [colorBgPlugin, actualTimeLinePlugin];
      }

      // Destroy previous Chart.js instance if it exists
      if (brightnessGraphRef.current._chartInstance) {
        brightnessGraphRef.current._chartInstance.destroy();
        brightnessGraphRef.current._chartInstance = null;
      }
      // Create new Chart.js instance
      const ctx = brightnessGraphRef.current.getContext("2d");
      brightnessGraphRef.current._chartInstance = new window.Chart(ctx, {
        type: "line",
        data: chartData,
        options: chartOptions,
        plugins: chartPlugins,
      });
    }

    // If Chart.js is not loaded, load from CDN
    if (!window.Chart) {
      const script = document.createElement("script");
      script.src = "https://cdn.jsdelivr.net/npm/chart.js";
      script.async = true;
      script.onload = renderChart;
      document.body.appendChild(script);
    } else {
      renderChart();
    }

    // Add window focus event to update red timeline
    function handleWindowFocus() {
      renderChart();
    }
    window.addEventListener("focus", handleWindowFocus);

    // Resize chart on window resize to fix tall/short bug
    function handleResize() {
      if (
        brightnessGraphRef.current &&
        brightnessGraphRef.current._chartInstance
      ) {
        brightnessGraphRef.current._chartInstance.resize();
      }
    }
    window.addEventListener("resize", handleResize);

    // Cleanup on unmount
    return () => {
      window.removeEventListener("resize", handleResize);
      window.removeEventListener("focus", handleWindowFocus);
      if (
        brightnessGraphRef.current &&
        brightnessGraphRef.current._chartInstance
      ) {
        brightnessGraphRef.current._chartInstance.destroy();
        brightnessGraphRef.current._chartInstance = null;
      }
    };
  }, [timers, state.brightness, config]);

  return (
    <canvas
      ref={brightnessGraphRef}
      id="brightnessGraph"
      tabIndex={0}
      style={{
        width: "100%",
        maxWidth: "100%",
        height: "220px",
        marginTop: "16px",
        outline: "none",
      }}
    ></canvas>
  );
}
