import { h } from "preact";
import { getBaseUrl } from "../baseUrl.js";

export function FirmwareUpdate({
  config,
  showToast,
  otaProgress,
  loaded,
  setConfig,
  otaFileName,
  setOtaFileName,
  otaInputRef,
  localOtaProgress,
  setLocalOtaProgress,
}) {
  // Helper to reset file input
  const resetFileInput = () => {
    if (otaInputRef.current) {
      otaInputRef.current.value = "";
    }
    setOtaFileName("");
  };
  return (
    <section class="card system-card">
      <h2>Firmware Update (OTA)</h2>
      <form
        class="ota-form"
        style={{
          width: "100%",
          display: "flex",
          flexDirection: "column",
          gap: 16,
        }}
        onSubmit={async (e) => {
          e.preventDefault();
          const otaFile = otaInputRef.current?.files[0];
          if (!otaFile) {
            showToast("Please select a firmware file.", "error");
            return;
          }
          try {
            setLocalOtaProgress(0);
            const xhr = new XMLHttpRequest();
            xhr.open("POST", getBaseUrl() + "/ota", true);
            xhr.setRequestHeader("Accept", "application/json");
            xhr.upload.onprogress = function (evt) {
              if (evt.lengthComputable) {
                const percent = Math.round((evt.loaded / evt.total) * 100);
                setLocalOtaProgress(percent);
              }
            };
            xhr.onload = function () {
              if (xhr.status === 200) {
                showToast("Firmware uploaded! Rebooting...", "success");
                setTimeout(() => window.location.reload(), 7000);
                resetFileInput();
              } else {
                showToast(
                  "OTA failed: " + (xhr.responseText || xhr.statusText),
                  "error",
                );
                setLocalOtaProgress(-1);
                resetFileInput();
              }
            };
            xhr.onerror = function () {
              showToast("OTA upload error.", "error");
              setLocalOtaProgress(-1);
              resetFileInput();
            };
            xhr.send(otaFile);
          } catch (err) {
            showToast("OTA error: " + err, "error");
            setLocalOtaProgress(-1);
            resetFileInput();
          }
        }}
      >
        <div
          style={{
            display: "flex",
            gap: 10,
            alignItems: "center",
            flexWrap: "wrap",
          }}
        >
          {/* Flex container ensures vertical alignment */}
          <input
            ref={otaInputRef}
            type="file"
            name="otaFile"
            accept=".bin,.gz,.zip"
            required
            style={{ display: "none" }}
            id="otaFileInput"
            onChange={(e) => {
              const file = e.target.files && e.target.files[0];
              setOtaFileName(file ? file.name : "");
            }}
          />
          <label
            htmlFor="otaFileInput"
            class="btn btn-primary"
            style={{
              margin: 0,
              display: "flex",
              alignItems: "center",
              height: 40,
            }}
            onClick={() => {
              resetFileInput();
            }}
          >
            Choose File
          </label>
          <button
            type="submit"
            class="btn btn-primary"
            style={{
              margin: 0,
              height: 40,
              display: "flex",
              alignItems: "center",
            }}
          >
            Upload
          </button>
          <span
            class="ota-file-name"
            id="otaFileName"
            style={{
              minWidth: 120,
              fontSize: "0.97em",
              marginLeft: 8,
              alignSelf: "center",
              height: 40,
              display: "flex",
              alignItems: "center",
            }}
          >
            {otaFileName || "No file chosen"}
          </span>
        </div>
        <button
          type="button"
          class="btn btn-info"
          style={{ width: "100%", marginTop: 0 }}
          onClick={async () => {
            try {
              const resp = await fetch(getBaseUrl() + "/api/update", {
                method: "POST",
              });
              const result = await resp.json();
              if (result && result.success) {
                showToast("Installing update... Device will reboot.", "info");
              } else {
                showToast(
                  result && result.message
                    ? result.message
                    : "No update found.",
                  "info",
                );
              }
            } catch (e) {
              showToast("Update check failed!", "error");
            }
          }}
        >
          Check for Updates
        </button>
      </form>
      {/* OTA Progress Bar */}
      {(localOtaProgress >= 0 || otaProgress >= 0) && (
        <div style={{ width: "100%", marginTop: "1em" }}>
          <div
            style={{
              height: "12px",
              background: "#eee",
              borderRadius: "6px",
              overflow: "hidden",
              boxShadow: "0 1px 2px #aaa inset",
            }}
          >
            <div
              style={{
                width: `${localOtaProgress >= 0 ? localOtaProgress : otaProgress}%`,
                height: "100%",
                background:
                  (localOtaProgress >= 0 ? localOtaProgress : otaProgress) < 100
                    ? "#66ccff"
                    : "#4caf50",
                transition: "width 0.2s",
              }}
            />
          </div>
          <div
            style={{ fontSize: "0.9em", marginTop: "2px", textAlign: "right" }}
          >
            {(localOtaProgress >= 0 ? localOtaProgress : otaProgress) < 100
              ? `Uploading... ${localOtaProgress >= 0 ? localOtaProgress : otaProgress}%`
              : "Update complete! Rebooting..."}
          </div>
        </div>
      )}
    </section>
  );
}
