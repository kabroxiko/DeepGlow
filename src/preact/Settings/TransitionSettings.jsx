import { steppedTransitionValue, formatTransitionTime } from "../util.js";

export function TransitionSettings({ config, setConfig }) {
  return (
    <section className="card">
      <h2>Transition Times</h2>
      <div className="config-grid">
        <div className="config-item">
          <label>Power On</label>
          <input
            type="range"
            min="0"
            max="127"
            className="slider-input"
            value={(() => {
              const sec = Math.round(
                Number(config?.transitionTimes?.powerOn) / 1000,
              );
              if (sec <= 59) return sec;
              if (sec < 3600) return 59 + Math.round(sec / 60);
              return 119 + Math.round(sec / 3600);
            })()}
            onInput={(e) =>
              setConfig((c) => ({
                ...c,
                transitionTimes: {
                  ...c.transitionTimes,
                  powerOn: steppedTransitionValue(e.target.value) * 1000,
                },
              }))
            }
          />
          <span>
            {formatTransitionTime(
              Math.round(Number(config?.transitionTimes?.powerOn) / 1000),
            )}
          </span>
        </div>
        <div className="config-item">
          <label>Schedule</label>
          <input
            type="range"
            min="0"
            max="127"
            className="slider-input"
            value={(() => {
              const sec = Math.round(
                Number(config?.transitionTimes?.schedule) / 1000,
              );
              if (sec <= 59) return sec;
              if (sec < 3600) return 59 + Math.round(sec / 60);
              return 119 + Math.round(sec / 3600);
            })()}
            onInput={(e) =>
              setConfig((c) => ({
                ...c,
                transitionTimes: {
                  ...c.transitionTimes,
                  schedule: steppedTransitionValue(e.target.value) * 1000,
                },
              }))
            }
          />
          <span>
            {formatTransitionTime(
              Math.round(Number(config?.transitionTimes?.schedule) / 1000),
            )}
          </span>
        </div>
        <div className="config-item">
          <label>Manual</label>
          <input
            type="range"
            min="0"
            max="127"
            className="slider-input"
            value={(() => {
              const sec = Math.round(
                Number(config?.transitionTimes?.manual) / 1000,
              );
              if (sec <= 59) return sec;
              if (sec < 3600) return 59 + Math.round(sec / 60);
              return 119 + Math.round(sec / 3600);
            })()}
            onInput={(e) =>
              setConfig((c) => ({
                ...c,
                transitionTimes: {
                  ...c.transitionTimes,
                  manual: steppedTransitionValue(e.target.value) * 1000,
                },
              }))
            }
          />
          <span>
            {formatTransitionTime(
              Math.round(Number(config?.transitionTimes?.manual) / 1000),
            )}
          </span>
        </div>
        <div className="config-item">
          <label>Effect</label>
          <input
            type="range"
            min="0"
            max="127"
            className="slider-input"
            value={(() => {
              const sec = Math.round(
                Number(config?.transitionTimes?.effect) / 1000,
              );
              if (sec <= 59) return sec;
              if (sec < 3600) return 59 + Math.round(sec / 60);
              return 119 + Math.round(sec / 3600);
            })()}
            onInput={(e) =>
              setConfig((c) => ({
                ...c,
                transitionTimes: {
                  ...c.transitionTimes,
                  effect: steppedTransitionValue(e.target.value) * 1000,
                },
              }))
            }
          />
          <span>
            {formatTransitionTime(
              Math.round(Number(config?.transitionTimes?.effect) / 1000),
            )}
          </span>
        </div>
      </div>
    </section>
  );
}
