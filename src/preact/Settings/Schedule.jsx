import { h } from "preact";

export function Schedule({ config, setConfig, presets, sunTimes }) {
  return (
    <section class="card">
      <h2>Schedule</h2>
      <div class="schedule-table">
        <table>
          <thead>
            <tr>
              <th>Enabled</th>
              <th>Type</th>
              <th>Time</th>
              <th>Preset</th>
              <th>Brightness (%)</th>
              <th>Actions</th>
            </tr>
          </thead>
          <tbody>
            {Array.isArray(config?.timers) && config.timers.length > 0
              ? config.timers.map((timer, idx) => (
                  <tr key={idx}>
                    {/* Enabled checkbox */}
                    <td>
                      <input
                        type="checkbox"
                        checked={!!timer.enabled}
                        onInput={(e) => {
                          setConfig((c) => {
                            const timers = [...c.timers];
                            timers[idx] = {
                              ...timers[idx],
                              enabled: e.target.checked,
                            };
                            return { ...c, timers };
                          });
                        }}
                      />
                    </td>
                    {/* Type radio group */}
                    <td>
                      {["Regular", "Sunrise", "Sunset"].map(
                        (label, val) => (
                          <label style={{ marginRight: "0.5em" }} key={val}>
                            <input
                              type="radio"
                              name={`type_${idx}`}
                              value={val}
                              checked={timer.type === val}
                              onInput={(e) => {
                                setConfig((c) => {
                                  const timers = [...c.timers];
                                  // Prevent multiple Sunrise/Sunset
                                  if (val === 1 || val === 2) {
                                    timers.forEach((t, i) => {
                                      if (i !== idx && t.type === val)
                                        t.type = 0;
                                    });
                                  }
                                  timers[idx] = {
                                    ...timers[idx],
                                    type: val,
                                  };
                                  return { ...c, timers };
                                });
                              }}
                            />
                            {label}
                          </label>
                        ),
                      )}
                    </td>
                    {/* Time input or sunrise/sunset label */}
                    <td>
                      {timer.type === 1 ? (
                        <span>{sunTimes.sunrise || ""}</span>
                      ) : timer.type === 2 ? (
                        <span>{sunTimes.sunset || ""}</span>
                      ) : (
                        <input
                          type="time"
                          value={`${String(timer.hour).padStart(2, "0")}:${String(timer.minute).padStart(2, "0")}`}
                          onInput={(e) => {
                            const [h, m] = e.target.value
                              .split(":")
                              .map(Number);
                            setConfig((c) => {
                              const timers = [...c.timers];
                              timers[idx] = {
                                ...timers[idx],
                                hour: h || 0,
                                minute: m || 0,
                              };
                              return { ...c, timers };
                            });
                          }}
                        />
                      )}
                    </td>
                    {/* Preset select */}
                    <td>
                      <select
                        value={timer.presetId}
                        onInput={(e) => {
                          setConfig((c) => {
                            const timers = [...c.timers];
                            timers[idx] = {
                              ...timers[idx],
                              presetId: parseInt(e.target.value),
                            };
                            return { ...c, timers };
                          });
                        }}
                      >
                        {presets.length > 0 ? (
                          presets.map((preset) => (
                            <option value={preset.id} key={preset.id}>
                              {preset.name || `Preset ${preset.id}`}
                            </option>
                          ))
                        ) : (
                          <option value={0}>No presets available</option>
                        )}
                      </select>
                    </td>
                    {/* Brightness input */}
                    <td>
                      <input
                        type="number"
                        min="0"
                        max="100"
                        value={timer.brightness}
                        style={{ width: "60px" }}
                        onInput={(e) => {
                          const val = Math.max(
                            0,
                            Math.min(100, parseInt(e.target.value) || 0),
                          );
                          setConfig((c) => {
                            const timers = [...c.timers];
                            timers[idx] = {
                              ...timers[idx],
                              brightness: val,
                            };
                            return { ...c, timers };
                          });
                        }}
                      />
                    </td>
                    {/* Actions (Delete button) */}
                    <td>
                      <button
                        class="btn btn-danger"
                        onClick={(e) => {
                          e.preventDefault();
                          setConfig((c) => {
                            const timers = [...c.timers];
                            timers.splice(idx, 1);
                            return { ...c, timers };
                          });
                        }}
                      >
                        Delete
                      </button>
                    </td>
                  </tr>
                ))
              : null}
          </tbody>
        </table>
        <button
          class="btn btn-secondary"
          style={{ marginTop: "1em" }}
          onClick={(e) => {
            e.preventDefault();
            setConfig((c) => {
              const timers = Array.isArray(c.timers) ? [...c.timers] : [];
              timers.push({
                enabled: true,
                type: 0,
                hour: 12,
                minute: 0,
                presetId: presets.length > 0 ? presets[0].id : 0,
                brightness: 80,
              });
              return { ...c, timers };
            });
          }}
        >
          Add Timer
        </button>
      </div>
    </section>
  );
}
