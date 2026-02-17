import { LiveClock } from "./LiveClock.jsx";
import { setTab } from "./App.jsx";

/**
 * StatusBar component for Home and Config pages.
 * Props:
 * - indicatorColor: CSS color for the status indicator (default: var(--success))
 * - indicatorId: id for the indicator span (optional)
 * - onButtonClick: function to call when the button is clicked
 * - buttonLabel: aria-label/title for the button (e.g., "Configuration", "Back to Main")
 * - buttonIcon: JSX for the icon (svg or similar)
 * - buttonClass: className for the button (optional)
 * - showIndicator: whether to show the status indicator (default: true)
 * - children: additional elements (optional, e.g., extra status text)
 */
export function StatusBar({
  indicatorColor = "var(--success)",
  indicatorId = "statusIndicator",
  buttonLabel,
  buttonSvgPath,
  buttonClass = "",
  showIndicator = true,
  children,
  buttonSvgViewBox = "0 0 640 640",
  buttonSvgWidth = 24,
  buttonSvgHeight = 24,
  buttonSvgAriaLabel = buttonLabel,
  buttonSvgClass = "icon-glow-hover",
  buttonSvgFill = "#66ccff",
  buttonTab,
  setTab,
}) {
  const handleButtonClick = () => {
    if (buttonTab && typeof setTab === "function") {
      setTab(buttonTab);
    }
  };
  return (
    <div className="status-bar">
      {showIndicator && (
        <span
          className="status-indicator"
          id={indicatorId}
          style={{ color: indicatorColor }}
        >
          ●
        </span>
      )}
      <LiveClock />
      {children}
      <button
        type="button"
        className={buttonClass}
        title={buttonLabel}
        aria-label={buttonLabel}
        onClick={handleButtonClick}
      >
        <svg
          className={buttonSvgClass}
          aria-label={buttonSvgAriaLabel}
          xmlns="http://www.w3.org/2000/svg"
          viewBox={buttonSvgViewBox}
          width={buttonSvgWidth}
          height={buttonSvgHeight}
        >
          <path fill={buttonSvgFill} d={buttonSvgPath} />
        </svg>
      </button>
    </div>
  );
}
