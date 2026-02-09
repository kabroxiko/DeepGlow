// Preact entry point for SPA routing
import { h, render } from "preact";
import { App } from "./App.jsx";
import { Config } from "./Config.jsx";
import { WifiSetup } from "./WifiSetup.jsx";

function getPage() {
  const path = window.location.pathname;
  // Always show WifiSetup for /wifi or /wifi.html
  if (path === "/wifi" || path.endsWith("wifi.html")) return WifiSetup;
  return App;
}

const Page = getPage();
render(h(Page, {}), document.body);
