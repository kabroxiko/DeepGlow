---
name: code-quality
description: A protocol for formatting and reviewing code to ensure consistency and quality across the project. It handles both C++ firmware and React/JavaScript frontend code.
license: MIT
metadata:
  author: GitHub Copilot
  version: "1.0"
---

## Agentic Code Quality Protocol

As an AI assistant, your goal is to format and review code to ensure it adheres to project standards. When asked to "review this file," "format the code," or "check my work," follow this protocol.

### Step 1: Identify File Type and Scope

1.  Determine the language of the file(s) to be reviewed:
    *   C++ (`.cpp`, `.h`)
    *   React/JavaScript/CSS (`.jsx`, `.js`, `.scss`)
2.  Ask the user if you should apply formatting changes directly or just provide suggestions.

### Step 2: Apply Automatic Formatting

Based on the file type, run the appropriate formatting command.

#### For C++ Firmware (`src/` directory):

1.  Execute the PlatformIO formatting tool, which uses `clang-format`.
    ```bash
    platformio run --target format
    ```
2.  Inform the user that you are using `clang-format` as defined by the project's configuration.

#### For React/JavaScript Frontend (`web/` directory):

1.  Execute the npm script for formatting. This typically uses a tool like `Prettier`.
    ```bash
    npm run format --prefix web
    ```
2.  Inform the user that you are using the project's defined frontend formatting tool.
    *(Note: If the `format` script doesn't exist in `package.json`, you should add it first using a tool like Prettier.)*

### Step 3: Perform a Code Review

After formatting, read the file and analyze it for common quality issues. Do not be overly critical; focus on clear improvements.

#### General Checks (All Code):

*   **Clarity:** Are variable and function names clear and descriptive?
*   **Magic Numbers:** Are there hard-coded numbers that should be named constants?
*   **Comments:** Is there complex logic that could benefit from an explanatory comment?
*   **Simplicity:** Could complex code blocks be simplified or broken into smaller functions?

#### C++ Specific Checks:

*   **Memory:** Are there obvious memory leaks (e.g., `new` without `delete`)? (Note: This is hard to do perfectly without running the code).
*   **Pointers:** Are pointers being used safely? Check for potential null pointer dereferences.

#### React/JavaScript Specific Checks:

*   **Component Size:** Are components becoming too large and could be split into smaller, reusable ones?
*   **Hook Rules:** Are React Hooks (like `useState`, `useEffect`) being used at the top level of the component?
*   **State Management:** Is state being managed appropriately? (e.g., not using component state for what should be global state).

### Step 4: Report Findings and Suggestions

1.  Summarize the formatting changes that were applied automatically.
2.  Present your code review suggestions to the user in a clear, constructive list.
3.  For each suggestion, provide a code snippet showing the *before* and *after* or explain the reasoning behind the recommended change.
4.  Ask the user if they would like you to apply any of the suggested changes.
