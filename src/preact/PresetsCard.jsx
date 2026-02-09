import { rgbwHexToPreview } from "./util.js";

export function PresetsCard({ presets, effects, activePreset, applyPreset }) {
  function PresetGrid() {
    return (
      <div className="preset-grid">
        {presets.map((preset) => {
          const colors =
            Array.isArray(preset.params?.colors) &&
            preset.params.colors.length > 0
              ? preset.params.colors
              : ["#000000"];
          const stops = colors.map((c) => rgbwHexToPreview(c));
          let gradient;
          if (stops.length === 1) {
            gradient = stops[0];
          } else {
            const pctStep = 100 / (stops.length - 1);
            gradient = `linear-gradient(135deg, ${stops.map((c, i) => `${c} ${i * pctStep}%`).join(", ")})`;
          }
          return (
            <div
              className={
                "preset-card" + (activePreset === preset.id ? " active" : "")
              }
              key={preset.id}
              onClick={() => applyPreset(preset.id)}
            >
              <div className="preset-name">{preset.name}</div>
              <div className="preset-info">
                Effect: {effects[preset.effect]?.name}
              </div>
              <div
                className="preset-color-preview"
                style={{ background: gradient }}
              ></div>
            </div>
          );
        })}
      </div>
    );
  }

  return (
    <section className="card">
      <h2>Presets</h2>
      <PresetGrid />
    </section>
  );
}
