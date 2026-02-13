// Config page as Preact component
import { useState, useRef } from "preact/hooks";

import { getBaseUrl } from "./baseUrl.js";
import { WiFiSettings } from "./Settings/WiFiSettings.jsx";
import { LEDSettings } from "./Settings/LEDSettings.jsx";
import { RelaySettings } from "./Settings/RelaySettings.jsx";
import { SafetySettings } from "./Settings/SafetySettings.jsx";
import { TransitionSettings } from "./Settings/TransitionSettings.jsx";
import { LocationTimeSettings } from "./Settings/LocationTimeSettings.jsx";
import { FirmwareUpdate } from "./Settings/FirmwareUpdate.jsx";
import { DeviceActions } from "./Settings/DeviceActions.jsx";
import { Schedule } from "./Settings/Schedule.jsx";

export function Config({
  config,
  timezones,
  sunTimes,
  showToast,
  loaded,
  setTab,
  presets,
  setConfig,
  otaProgress,
}) {
  const [otaFileName, setOtaFileName] = useState("");
  const otaInputRef = useRef();
  // Local upload progress for manual upload only
  const [localOtaProgress, setLocalOtaProgress] = useState(-1); // -1: hidden, 0-100: progress
  return (
    <div>
      {!loaded.timezones && (
        <div
          id="loadingIndicator"
          style={{ textAlign: "center", marginTop: "3em", fontSize: "1.3em" }}
        >
          Loading configuration...
        </div>
      )}
      <div
        class="container"
        id="mainContainer"
        style={{
          display:
            loaded.timezones && loaded.presets && loaded.config ? "" : "none",
        }}
      >
        <header class="header">
          <h1>Configuration</h1>
          <div class="status-bar">
            <span class="status-indicator" id="statusIndicator">
              ●
            </span>
            <span id="currentTime"></span>
            <a
              href="#"
              className="back-to-main-link"
              title="Back to Main"
              onClick={(e) => {
                e.preventDefault();
                if (typeof setTab === "function") {
                  setTab("home");
                } else {
                  window.location.href = "index.html";
                }
              }}
            >
              {/* FontAwesome Free SVG: fa-home (solid) exact */}
              <svg
                aria-hidden="true"
                focusable="false"
                data-prefix="fas"
                data-icon="home"
                className="svg-inline--fa fa-home fa-w-18 back-to-main-icon icon-glow-hover"
                role="img"
                xmlns="http://www.w3.org/2000/svg"
                viewBox="0 0 576 512"
                width="24"
                height="24"
              >
                <path
                  fill="#66ccff"
                  d="M280.37 148.26L96 300.11V464a16 16 0 0 0 16 16l112-.3a16 16 0 0 0 16-15.7V368a16 16 0 0 1 16-16h64a16 16 0 0 1 16 16v95.7a16 16 0 0 0 16 16l112 .3a16 16 0 0 0 16-16V300L295.67 148.26a12.19 12.19 0 0 0-15.3 0zM573.32 268.35L512 220.69V56a24 24 0 0 0-24-24h-88a24 24 0 0 0-24 24v51.61L318.47 43a48 48 0 0 0-61 0L2.61 268.35a16 16 0 0 0-1.6 22.59l21.41 25.5a16 16 0 0 0 22.59 1.6L64 271.69V464a48 48 0 0 0 48 48h352a48 48 0 0 0 48-48V271.7l19 16.35a16 16 0 0 0 22.59-1.6l21.41-25.5a16 16 0 0 0-1.68-22.6z"
                />
              </svg>
            </a>
          </div>
        </header>
        <div
          style={{
            display: "flex",
            justifyContent: "flex-end",
            marginBottom: "0.5em",
          }}
        >
          {/* Download Config */}
          <button
            class="btn btn-secondary"
            style={{ marginRight: "0.5em" }}
            onClick={() => {
              const blob = new Blob([JSON.stringify(config, null, 2)], {
                type: "application/json",
              });
              const url = URL.createObjectURL(blob);
              const a = document.createElement("a");
              a.href = url;
              a.download = "config.json";
              document.body.appendChild(a);
              a.click();
              setTimeout(() => {
                document.body.removeChild(a);
                URL.revokeObjectURL(url);
                showToast("Config downloaded!", "success");
              }, 100);
            }}
          >
            Download Config
          </button>
          {/* Upload Config */}
          <label
            class="btn btn-secondary"
            style={{ marginRight: "0.5em", cursor: "pointer" }}
          >
            Upload Config
            <input
              type="file"
              accept=".json,application/json"
              style={{ display: "none" }}
              onChange={(e) => {
                const file = e.target.files && e.target.files[0];
                if (!file) return;
                const reader = new FileReader();
                reader.onload = (evt) => {
                  try {
                    const json = JSON.parse(evt.target.result);
                    fetch(getBaseUrl() + "/api/config", {
                      method: "POST",
                      headers: { "Content-Type": "application/json" },
                      body: JSON.stringify(json),
                    })
                      .then(async (response) => {
                        if (!response.ok) throw new Error("Upload failed");
                        return response.text();
                      })
                      .then(() => {
                        showToast("Config uploaded!", "success");
                        setTimeout(() => window.location.reload(), 1200);
                      })
                      .catch((err) => {
                        showToast("Error uploading config: " + err, "error");
                      });
                  } catch (err) {
                    showToast("Invalid config file: " + err.message, "error");
                  }
                };
                reader.readAsText(file);
                e.target.value = "";
              }}
            />
          </label>
          {/* Save Configuration */}
          <button
            class="btn btn-primary"
            onClick={async () => {
              try {
                const resp = await fetch(getBaseUrl() + "/api/config", {
                  method: "POST",
                  headers: { "Content-Type": "application/json" },
                  body: JSON.stringify(config),
                });
                if (!resp.ok) throw new Error("Save failed");
                showToast("Configuration saved!", "success");
              } catch (err) {
                showToast("Error saving config: " + err, "error");
              }
            }}
          >
            Save Configuration
          </button>
        </div>
        <div class="card-grid">
          <WiFiSettings config={config} setConfig={setConfig} />
          <LEDSettings config={config} setConfig={setConfig} />
          <RelaySettings config={config} setConfig={setConfig} />
          <SafetySettings config={config} setConfig={setConfig} />
          <TransitionSettings config={config} setConfig={setConfig} />
          <LocationTimeSettings
            config={config}
            setConfig={setConfig}
            timezones={timezones}
          />
          <FirmwareUpdate
            config={config}
            showToast={showToast}
            otaProgress={otaProgress}
            loaded={loaded}
            setConfig={setConfig}
            otaFileName={otaFileName}
            setOtaFileName={setOtaFileName}
            otaInputRef={otaInputRef}
            localOtaProgress={localOtaProgress}
            setLocalOtaProgress={setLocalOtaProgress}
          />
          <DeviceActions
            config={config}
            showToast={showToast}
            loaded={loaded}
            setConfig={setConfig}
          />
          </div>
          <Schedule
            config={config}
            setConfig={setConfig}
            presets={presets}
            sunTimes={sunTimes}
          />
      </div>
    </div>
  );
}
