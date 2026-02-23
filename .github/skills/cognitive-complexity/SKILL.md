---
name: cognitive-complexity
description: A protocol for detecting and reducing cognitive complexity across the C++ firmware and the React/Preact web UI. It guides the agent to flatten deep nesting, split long functions, and eliminate complex conditional chains.
license: MIT
metadata:
  author: GitHub Copilot
  version: "1.0"
---

## Agentic Cognitive Complexity Reduction Protocol

As an AI assistant, your goal is to identify and reduce cognitive complexity across the DeepGlow project. When asked to "reduce complexity," "simplify code," "flatten nesting," or "improve readability," follow this protocol.

Cognitive complexity (as defined by SonarSource) adds one unit per: `if`, `else if`, `else`, `for`, `while`, `switch case`, logical operator chain (`&&`/`||`), early `return` inside a loop, and **nesting bonus** (each level of nesting further increases cost). The goal is to keep any single function under a complexity score of ~15.

---

### Step 1: Scan for High-Complexity Hotspots

#### C++ Firmware (`src/`)

Look for these patterns:

1. **Near-identical twin functions:**
   - Pairs of functions that share 80 %+ of their body with only one or two values differing (e.g., `calculateSunriseMinutes` vs `calculateSunsetMinutes` in `scheduler.cpp`).
   - Action: Merge into a single parameterised helper and call it twice with the differing arguments.

2. **Deep nesting inside loop bodies (`effects.cpp`, `webserver.cpp`):**
   - Guard clauses (`if (!ptr) return;`) buried behind 3+ levels of indentation.
   - Action: Hoist all guard clauses to the top of the function so the happy path remains flat.

3. **Long dispatch chains inside a single handler:**
   - `if (msg.find("...") != npos) { ... } else if (...) { ... }` chains longer than 4 arms (see `wsHandler` in `webserver.cpp`).
   - Action: Extract a `handleWsMessage(mgr, req, msg)` helper and use early-return within it.

4. **Functions longer than ~80 lines:**
   - Split into clearly-named sub-functions. Name each after *what it does*, not *how* it does it.

5. **Redundant intermediate variables used only once:**
   - Local variables whose only purpose is to shorten a nested expression — inline them or rename to clarify intent.

6. **Parallel `#ifdef ARDUINO / #else` blocks with complex bodies:**
   - If both branches contain more than 5 lines of logic, consider wrapping the platform-specific part in a thin inline helper in `debug.h` or a new `platform.h`.

#### React/Preact Web UI (`web/`)

Look for these patterns:

1. **Large `useEffect` containing an inner named function:**
   - E.g., a `renderChart()` function defined inside `useEffect` in `Graph.jsx`.
   - Action: Hoist `renderChart` out of the `useEffect` (pass dependencies as parameters) so the arrow in `useEffect` becomes a one-liner call.

2. **Multi-level ternary chains:**
   - `a ? b : c ? d : e` — replace with an `if`/`else if`/`else` block or a lookup table.

3. **`&&`/`||` chains used as control flow instead of data:**
   - `condition1 && condition2 && doSomething()` — replace with an explicit `if`.

4. **Inline `switch`-like logic in JSX render:**
   - Sequences of `{ condition && <Component /> }` for mutually exclusive states — replace with a helper `renderXxx()` function that uses a plain `if/else if`.

5. **Deeply nested optional chaining (`?.`) 4+ levels deep:**
   - `a?.b?.c?.d?.e` — extract the intermediate value into a named variable with a sensible default.

6. **Event handler functions longer than ~30 lines:**
   - Extract into named handler functions defined outside the JSX return, or into custom hooks (`useXxx`).

---

### Step 2: Prioritise Refactors

For each hotspot found, assess:
- **Safety:** Does the change alter observable behaviour? (If uncertain, skip.)
- **Gain:** How much does it reduce the nesting depth or function length?
- **Scope:** Is the change contained to one file?

Prioritise: **twin-function merges** (safest + highest gain) → **guard-clause hoisting** → **handler extraction** → **JSX render helpers**.

---

### Step 3: Apply Refactors

1. **Simplify guard clauses first.** Move all early-exit conditions to the top of the function.
2. **Extract sub-functions.** Name them after their intent. Keep them in the same file (use `static` in C++, module-scope in JS).
3. **Merge near-identical twin functions.** Add a single boolean or enum parameter for the diverging behaviour.
4. **Do NOT change external APIs, function signatures visible from headers, or component props.**
5. **Preserve all comments** or transfer them to the new helper.

#### C++ Conventions
- New sub-functions: `static` free functions above the caller in the same `.cpp` file, or `inline` in the relevant `.h` if shared.
- Use `#pragma once` on any new header.
- Prefer `const` parameters and return values where possible.

#### Web UI Conventions
- New sub-functions: module-scope named functions in the same `.jsx`/`.js` file.
- Hoist pure helper functions (no hooks) above the component definition.
- Hooks (`use*`) must stay inside component/custom-hook bodies.

---

### Step 4: Validate

#### Firmware

```bash
source .venv/bin/activate && pio run -e esp32d_debug
source .venv/bin/activate && pio run -e esp32c6_debug
```

Confirm zero new errors or warnings.

#### Web UI

```bash
npm run lint
```

Confirm no new lint errors.

---

### Step 5: Summarise

List every hotspot resolved:
- **What** was complex (function name, pattern type).
- **Where** it lived (file and approximate line range).
- **What** replaced it (new helper name / technique applied).
- **Estimated complexity reduction** (lines removed / nesting levels flattened).
- Any follow-up opportunities spotted but deferred.
