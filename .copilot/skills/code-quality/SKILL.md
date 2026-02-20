---
name: code-quality
description: A protocol for performing a deep code quality analysis using static analysis tools and formatting. It covers C++ (cppcheck) and React/JavaScript (ESLint).
license: MIT
metadata:
  author: GitHub Copilot
  version: "2.0"
---

## Agentic Code Analysis and Quality Protocol

As an AI assistant, your goal is to perform a comprehensive code quality check using industry-standard tools. When asked to "review the code," "check for bugs," or "improve code quality," follow this protocol.

### Step 1: Identify File Type and Scope

1.  Determine the language of the file(s) to be reviewed:
    *   C++ (`.cpp`, `.h`)
    *   React/JavaScript/CSS (`.jsx`, `.js`, `.scss`)
2.  Ask the user if you should apply formatting changes directly or just provide suggestions.

### Step 2: Run Static Analysis

This is the core of the code review. Run the appropriate tool to find bugs, vulnerabilities, and code smells.

#### For C++ Firmware (`src/` directory):

1.  **Configure C++ Standard:** First, ensure `cppcheck` is set to use the C++11 standard.
    *   Read the `platformio.ini` file.
    *   Check if the relevant build environment (e.g., `[env:esp32d_debug]`) contains the line `check_flags = cppcheck --std=c++11`.
    *   If this line is missing or incorrect, inform the user: "To ensure the C++ analysis uses the correct standard, I need to configure `cppcheck` for C++11 in your `platformio.ini` file. May I add the necessary `check_flags`?"
    *   If the user agrees, add or modify the `check_flags` for the appropriate environment(s).
2.  **Run Static Analysis:** Execute the PlatformIO `check` command.
    ```bash
    platformio check
    ```
3.  Analyze the output for any reported defects, vulnerabilities, or performance issues.

#### For React/JavaScript Frontend (`web/` directory):

1.  **Check for ESLint setup:**
    *   Look for an `.eslintrc.*` file in the root or `web/` directory.
    *   Check `web/package.json` for a `lint` script.
2.  **If ESLint is not configured:**
    *   Inform the user that ESLint is not set up.
    *   Ask for permission to set it up: "I've noticed ESLint isn't configured for the frontend code. It's a powerful tool for finding bugs. Would you like me to set it up for you?"
    *   If yes, proceed to install `eslint` and a standard configuration (e.g., `eslint-plugin-react`). Create a basic `.eslintrc.js` and add a `lint` script to `package.json`.
3.  **If ESLint is configured:**
    *   Execute the linting command.
        ```bash
        npm run lint --prefix web
        ```
    *   Analyze the output for any errors or warnings.

### Step 3: Apply Automatic Formatting

After the deep analysis, apply standard code formatting for consistency.

#### For C++ Firmware:
```bash
platformio run --target format
```

#### For React/JavaScript Frontend:
```bash
npm run format --prefix web
```
*(Note: If the `format` script doesn't exist, offer to set it up with Prettier.)*

### Step 4: Report Findings and Suggestions

1.  **Summarize Static Analysis Results:** Report the critical findings from `cppcheck` or `ESLint`. For each issue, provide the file, line number, and a description of the problem.
2.  **Summarize Formatting:** Mention that the code has been automatically formatted for consistency.
3.  **Offer to Fix:** Ask the user if they would like you to attempt to fix the issues found by the static analysis tools.
