# Preact Migration

This folder contains the new Preact-based UI for the Aquarium LED Controller. All HTML, JS, and CSS are being migrated to Preact components.

- `App.jsx`: Main UI (was index.html)
- `Config.jsx`: Configuration page (was config.html)
- `WifiSetup.jsx`: WiFi setup page (was wifi.html)
- `index.js`: SPA entry point and router
- `style.css`: Styles (imported from original, will be refactored)

## Migration Steps

- Move HTML structure to JSX in each component
- Refactor JS logic into Preact hooks/components
- Update build scripts to use Preact
- Remove old HTML/JS after migration
