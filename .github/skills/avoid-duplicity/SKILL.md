---
name: avoid-duplicity
description: A protocol for detecting and eliminating code duplication across the C++ firmware and the React/Preact web UI. It guides the agent to extract shared logic into helpers, utilities, and reusable components.
license: MIT
metadata:
  author: GitHub Copilot
  version: "1.0"
---

## Agentic Code Deduplication Protocol

As an AI assistant, your goal is to identify and eliminate code duplication across the DeepGlow project. When asked to "remove duplication," "refactor repeated code," or "DRY up the codebase," follow this protocol.

---

### Step 1: Scan for Duplication Hotspots

#### C++ Firmware (`src/`)

Look for these common duplication patterns:

1. **`#ifdef ARDUINO` / `#else` / `#endif` blocks:**
   - Duplicated logic for Arduino vs ESP-IDF paths in the same function.
   - Action: Extract the diverging parts into a thin abstraction (e.g., a logging wrapper in `debug.h`, a filesystem helper, or a time helper).

2. **Repeated logging calls mixing `Serial.printf` and `ESP_LOGI`:**
   - `debug.h` already provides `debugPrintln`/`debugPrint` wrappers — prefer them.
   - Replace any remaining raw `Serial.printf` or inconsistent `ESP_LOGI` scattered outside `debug.h` with the centralized helpers.

3. **Repeated JSON serialization/deserialization boilerplate:**
   - Patterns like `DynamicJsonDocument` init, `deserializeJson`, and error checking appear in `config.cpp`, `ota.cpp`, and `webserver.cpp`.
   - Action: Extract a shared `parseJson(const std::string &, JsonDocument &)` helper in `config.h`/`config.cpp` or a new `util.h`.

4. **Repeated HTTP body-reading/response-sending snippets in `webserver.cpp`:**
   - `readBody`, `setCors`, and the "send JSON error" pattern are already helpers — ensure every handler uses them, with no inline duplicates.

5. **Duplicate `extern` declarations across `.cpp` files:**
   - If the same `extern` variable is declared in more than one `.cpp`, consolidate the declaration in the relevant `.h` header and include it.

#### React/Preact Web UI (`web/`)

Look for these patterns:

1. **Repeated `fetch` + `showToast` error-handling:**
   - Multiple components (e.g., `DeviceActions.jsx`, `Config.jsx`, settings panels) each wrap `fetch` with the same try/catch/toast pattern.
   - Action: Create or extend `web/util.js` with an `apiFetch(url, options, showToast)` helper that centralizes the fetch, error catching, and toast notification.

2. **Repeated `getBaseUrl()` + `/api/...` URL construction:**
   - Every component builds API URLs inline. Centralise all endpoint paths into a dedicated `web/api.js` (or extend `baseUrl.js`) exporting named functions like `apiUrl(path)`.

3. **Duplicate state-shape initialisers:**
   - If multiple components initialise the same shape (e.g., LED config objects, timer objects), extract a factory function or a `defaults.js` module in `web/`.

4. **Copy-pasted modal confirmation flows:**
   - The confirm → action → toast pattern appears in `DeviceActions.jsx` and elsewhere. The shared `Modal` component already exists — ensure all confirmations use it with consistent props and no inline reimplementation.

5. **Duplicate `presets`/`timers` list-rendering logic:**
   - If the same list map+render appears in more than one component, extract it into a shared sub-component.

---

### Step 2: Prioritise and Plan Refactors

For each duplication found, assess:
- **Risk:** Does touching this file risk breaking firmware or UI behaviour?
- **Gain:** How many occurrences are squashed?

Prioritise high-gain, low-risk extractions first (utilities, helpers). Save structural refactors (component splits, header reorganisation) for last.

---

### Step 3: Apply Refactors

1. **Create or extend the shared helper** before removing the duplicates.
2. **Replace all call sites** with the new helper. Use `grep_search` to find every occurrence.
3. **Do not change observable behaviour** — only restructure.
4. **Keep one implementation** — delete the duplicates once all call sites are updated.

#### C++ Conventions
- New shared utilities belong in `src/util.h` / `src/util.cpp` (create if missing) or in the most relevant existing header.
- Prefer `inline` or `static` functions in headers for simple one-liners.
- Use `#pragma once` on any new header.

#### Web UI Conventions
- New shared JS/JSX utilities belong in `web/util.js` (already exists) or `web/api.js` for API-specific helpers.
- New shared components belong under `web/` (or a `web/components/` subfolder if the count grows).
- Export named functions/components; avoid default exports for consistency.

---

### Step 4: Validate

#### Firmware
1. Build the affected environments:
   ```bash
   source .venv/bin/activate && pio run -e esp32d_debug
   source .venv/bin/activate && pio run -e esp32c6_debug
   ```
2. Confirm zero new errors or warnings introduced.

#### Web UI
1. Run the linter:
   ```bash
   npm run lint
   ```
2. Run the dev server and manually verify affected pages:
   ```bash
   npm run dev
   ```

---

### Step 5: Summarise

List every duplication resolved:
- **What** was duplicated (function, pattern, snippet).
- **Where** it lived (files/line ranges).
- **What** now replaces it (new helper / shared component name).
- Any follow-up opportunities spotted but intentionally deferred.
