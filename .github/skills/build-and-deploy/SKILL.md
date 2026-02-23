---
name: build-and-deploy
description: Instructions for building, testing, and deploying the firmware and web interface for an IoT LED project. Use this skill to automate common development tasks.
license: MIT
metadata:
  author: GitHub Copilot
  version: "1.0"
---

## Build and Deployment Automation

This skill outlines the common tasks for building and deploying the project.

### Building the Firmware

### Supported Targets

This project supports two main firmware targets:

- **ESP32-C6 (ESP-IDF):** For modern ESP32-C6 boards using the ESP-IDF framework.
- **ESP32D (Arduino):** For classic ESP32 boards using the Arduino framework.

### Building the Firmware

*   **ESP32-C6 (ESP-IDF):**
  *   Command: `source .venv/bin/activate && platformio run -e esp32c6_debug`
  *   Example:
    ```bash
    source .venv/bin/activate && platformio run -e esp32c6_debug
    ```
  *   Use the "Build for esp32c6_debug" task for debug builds.

*   **ESP32D (Arduino):**
  *   Command: `source .venv/bin/activate && platformio run -e esp32d_debug`
  *   Example:
    ```bash
    source .venv/bin/activate && platformio run -e esp32d_debug
    ```
  *   Use the "Build for esp32d_debug" task for debug builds.

*   Always activate the Python virtual environment before building.

### Uploading Firmware

*   **ESP32D (Arduino):**
  *   Command: `source .venv/bin/activate && platformio run -e esp32d_debug -t upload`
  *   Example:
    ```bash
    source .venv/bin/activate && platformio run -e esp32d_debug -t upload
    ```
  *   Use the "Upload for esp32d_debug" task to flash the device.

*   **ESP32-C6 (ESP-IDF):**
  *   Command: `source .venv/bin/activate && platformio run -e esp32c6_debug -t upload`
  *   Example:
    ```bash
    source .venv/bin/activate && platformio run -e esp32c6_debug -t upload
    ```
  *   Use the "Upload for esp32c6_debug" task to flash the device.

### Building the Web Interface

*   **Command:** `npm run build`
*   **Action:** This command, defined in `package.json`, uses Vite to compile the React application into static assets located in the `web/dist` directory.

### Embedding Web Assets into Firmware

*   **Action:** After building the web interface, the static files must be converted into C++ headers to be included in the firmware.
*   **Command:** `python scripts/embed_assets.py`
*   **Full Workflow:**
    1.  `npm run build` (in the `web` directory)
    2.  `python scripts/embed_assets.py` (from the project root)

### Cleaning the Build Environment

*   **Command:** `platformio run --target clean`
*   **Action:** Removes compiled object files and firmware binaries, which is useful for forcing a complete rebuild.

### Uploading Firmware to Device

*   **Command:** `platformio run -e <environment_name> --target upload`
*   **Action:** Builds the firmware and uploads it to the connected device.
