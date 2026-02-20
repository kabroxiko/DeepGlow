---
name: firmware-development
description: A protocol for the agent to follow when modifying the C++ firmware. It covers analyzing the request, making code changes, and ensuring API consistency with the web UI.
license: MIT
metadata:
  author: GitHub Copilot
  version: "2.0"
---

## Agentic Firmware Development Protocol

As an AI assistant, your task is to modify the C++ firmware located in the `src/` directory. Follow this protocol to ensure changes are implemented correctly and safely.

### Step 1: Understand the Request & The Frontend Contract

1.  **Clarify the Goal:** What specific changes are needed for the firmware?
    *   Is it adding a new effect?
    *   Is it changing a configuration option?
    *   Is it fixing a bug?
2.  **Check the Frontend Contract:** If the change affects the web UI, you **must** understand the frontend's needs.
    *   **Read the relevant React components in `web/`:** Check what WebSocket messages the UI sends and what data it expects to receive.
    *   **If the change will break the UI, you must create a plan to update both.** Use the `project-overview` skill to formulate a multi-step plan.

### Step 2: Modify the Firmware Code

1.  Identify the relevant C++ files in the `src/` directory that need to be modified.
    *   For new effects, edit `effects.h` and `effects.cpp`.
    *   For persistent settings, edit `config.h` and `config.cpp`.
    *   For real-time state, edit `state.h` and `state.cpp`.
    *   For API communication, edit `webserver.cpp`.
2.  Apply the necessary code changes.
3.  Follow existing coding style and be mindful of the memory constraints of embedded devices.

### Step 3: Verify the Build

1.  After making changes, it is crucial to ensure the firmware still compiles.
2.  Execute a build command. You can use a specific environment or the default.
    ```bash
    platformio run -e esp32d_debug
    ```
3.  Analyze the output for any compiler errors or warnings. If there are errors, you must fix them before proceeding.

### Step 4: Inform the User of Completion

1.  Once the code is modified and verified to build successfully, your task is complete.
2.  Summarize the changes you made to the firmware.
3.  Advise the user that they can now upload the new firmware to their device using the command:
    `platformio run -e esp32d_debug --target upload`
