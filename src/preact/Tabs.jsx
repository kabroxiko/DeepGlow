import { useState, useEffect } from "preact/hooks";
import { Home } from "./Home.jsx";
import { Config } from "./Config.jsx";

export const TABS = Object.freeze({
  HOME: "home",
  CONFIG: "config",
});

export function getHandshakeType(tab) {
  return { type: tab === TABS.CONFIG ? "ota_client" : "state" };
}

export function useTabs() {
  // Tab state from URL hash, fallback to localStorage, then home
  const getInitialTab = () => {
    if (globalThis.location.hash === "#" + TABS.CONFIG) return TABS.CONFIG;
    if (globalThis.location.hash === "#" + TABS.HOME) return TABS.HOME;
    let saved;
    try {
      saved = localStorage.getItem("deepglow_tab");
    } catch (e) {
      console.error("Error accessing localStorage for deepglow_tab:", e);
    }
    if (saved === TABS.CONFIG || saved === TABS.HOME) return saved;
    return TABS.HOME;
  };
  const [tab, setTab] = useState(getInitialTab);

  // Update tab when hash changes
  useEffect(() => {
    const onHashChange = () => {
      if (globalThis.location.hash === "#" + TABS.CONFIG) setTab(TABS.CONFIG);
      else setTab(TABS.HOME);
    };
    globalThis.addEventListener("hashchange", onHashChange);
    return () => globalThis.removeEventListener("hashchange", onHashChange);
  }, []);

  // Persist tab to localStorage and URL hash on change
  useEffect(() => {
    try {
      if (tab === TABS.CONFIG) {
        localStorage.setItem("deepglow_tab", TABS.CONFIG);
        globalThis.location.hash = "#" + TABS.CONFIG;
      } else {
        localStorage.setItem("deepglow_tab", TABS.HOME);
        globalThis.location.hash = "#" + TABS.HOME;
      }
    } catch {}
  }, [tab]);

  return [tab, setTab];
}

export function Tabs({
  tab,
  setTab,
  state,
  presets,
  timers,
  effects,
  activePreset,
  ledBarRef,
  sendState,
  config,
  setActivePreset,
  timezones,
  sunTimes,
  showToast,
  hideToast,
  loaded,
  setConfig,
  otaProgress,
}) {
  return tab === TABS.HOME ? (
    <Home
      state={state}
      presets={presets}
      timers={timers}
      effects={effects}
      activePreset={activePreset}
      ledBarRef={ledBarRef}
      sendState={sendState}
      setTab={setTab}
      config={config}
      setActivePreset={setActivePreset}
    />
  ) : (
    <Config
      config={config}
      timezones={timezones}
      sunTimes={sunTimes}
      showToast={showToast}
      hideToast={hideToast}
      loaded={loaded}
      setTab={setTab}
      presets={presets || []}
      setConfig={setConfig}
      otaProgress={otaProgress}
    />
  );
}
