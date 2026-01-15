
#include <array>
#include <cstddef>
#include <cstdint>
#include "bus_manager.h"
#include "effects.h"
#include "state.h"
#include "transition.h"


// Blending state: use prevParams and newParams for all effect blending
EffectParams prevParams;
EffectParams newParams;

extern BusManager busManager;
extern Configuration config;

volatile uint8_t g_effectSpeed = 1;


// Call this after initializing or reconfiguring the strip
void updatePixelCount() {
  busManager.updatePixelCount();
}

// Centralized function to show the strip regardless of type/order
void showStrip() {
  busManager.show();
}

static std::vector<EffectRegistryEntry>& _effectRegistryVec() {
  static std::vector<EffectRegistryEntry> reg;
  return reg;
}

void _registerEffect(const char* name, uint16_t (*handler)()) {
  _effectRegistryVec().push_back({name, handler});
}

const std::vector<EffectRegistryEntry>& getEffectRegistry() {
  return _effectRegistryVec();
}

// Unified pixel color setter for all LED types and color orders
void setPixelColorUnified(uint16_t i, uint8_t r, uint8_t g, uint8_t b) {
  uint32_t color = (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
  busManager.setPixelColor(i, color);
}

// WLED-inspired color_blend for 24/32-bit colors
static uint32_t color_blend(uint32_t color1, uint32_t color2, uint8_t blend) {
  const uint32_t TWO_CHANNEL_MASK = 0x00FF00FF;
  uint32_t rb1 =  color1       & TWO_CHANNEL_MASK;
  uint32_t wg1 = (color1 >> 8) & TWO_CHANNEL_MASK;
  uint32_t rb2 =  color2       & TWO_CHANNEL_MASK;
  uint32_t wg2 = (color2 >> 8) & TWO_CHANNEL_MASK;
  uint32_t rb3 = ((((rb1 << 8) | rb2) + (rb2 * blend) - (rb1 * blend)) >> 8) &  TWO_CHANNEL_MASK;
  uint32_t wg3 = ((((wg1 << 8) | wg2) + (wg2 * blend) - (wg1 * blend)))      & ~TWO_CHANNEL_MASK;
  return rb3 | wg3;
}


// Solid color effect: fills the strip with color[0] or writes to buffer
uint16_t solid_effect(uint32_t* buffer, size_t count) {
  extern std::array<uint32_t, 8> color;
  extern SystemState state;
  extern TransitionEngine transition;
  uint8_t brightness = transition.isTransitioning() ? transition.getCurrentBrightness() : state.brightness;
  auto scale = [brightness](uint8_t c) -> uint8_t { return (uint16_t(c) * brightness) / 255; };
  size_t n = count ? count : busManager.getPixelCount();
  uint32_t solidColor = color[0];
  uint8_t r = scale((solidColor >> 16) & 0xFF);
  uint8_t g = scale((solidColor >> 8) & 0xFF);
  uint8_t b = scale(solidColor & 0xFF);
  if (buffer) {
    for (size_t i = 0; i < n; ++i) {
      buffer[i] = (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
    }
  } else {
    for (size_t i = 0; i < n; ++i) {
      setPixelColorUnified(i, r, g, b);
    }
    showStrip();
  }
  return 0;
}
// Registration wrapper for effect registry
static uint16_t solid_effect_registry() { return solid_effect(nullptr, 0); }
REGISTER_EFFECT("Solid", solid_effect_registry);


// Blend effect: blends between previous and next effect using prevParams and newParams only
uint16_t blend_effect(uint32_t* buffer, size_t count, float progress) {
  size_t prevColorCount = prevParams.colors.size();
  size_t newColorCount = newParams.colors.size();
  size_t maxCount = std::max(prevColorCount, newColorCount);
  if (maxCount < 2) return 0;

  uint8_t prevSpeed = prevParams.speed > 0 ? (prevParams.speed * 254) / 100 + 1 : 1;
  uint8_t newSpeed = newParams.speed > 0 ? (newParams.speed * 254) / 100 + 1 : 1;
  uint8_t speed = prevSpeed + (newSpeed - prevSpeed) * progress;

  extern SystemState state;
  extern TransitionEngine transition;
  uint8_t brightness = transition.isTransitioning() ? transition.getCurrentBrightness() : state.brightness;
  auto scale = [brightness](uint8_t c) -> uint8_t { return (uint16_t(c) * brightness) / 255; };

  static uint32_t phaseBase = 0;
  uint32_t now = millis();
  if (progress == 0.0f) phaseBase = now;
  uint16_t phase = ((now - phaseBase) * ((speed >> 3) + 1)) >> 8;
  size_t ledCount = count ? count : busManager.getPixelCount();
  if (transition.isTransitioning() && (progress == 0.0f || progress == 1.0f || fmod(progress, 0.25f) < 0.01f)) {
    debugPrint("[blend_effect] progress: ");
    debugPrint(progress, 3);
    debugPrint(" prevParams.colors: ");
    for (size_t i = 0; i < prevColorCount; ++i) {
      char hex[10];
      snprintf(hex, sizeof(hex), "#%06X", strtoul(prevParams.colors[i].c_str() + (prevParams.colors[i][0] == '#' ? 1 : 0), nullptr, 16) & 0xFFFFFF);
      debugPrint(hex);
      debugPrint(" ");
    }
    debugPrint(" newParams.colors: ");
    for (size_t i = 0; i < newColorCount; ++i) {
      char hex[10];
      snprintf(hex, sizeof(hex), "#%06X", strtoul(newParams.colors[i].c_str() + (newParams.colors[i][0] == '#' ? 1 : 0), nullptr, 16) & 0xFFFFFF);
      debugPrint(hex);
      debugPrint(" ");
    }
    debugPrintln();
  }
  for (size_t i = 0; i < ledCount; i++) {
    float idxProgress = float(i) / float(std::max<size_t>(1, ledCount-1));
    size_t prevIdx = idxProgress * (prevColorCount - 1);
    size_t newIdx = idxProgress * (newColorCount - 1);
    uint32_t prevCol = strtoul(prevParams.colors[prevIdx].c_str() + (prevParams.colors[prevIdx][0] == '#' ? 1 : 0), nullptr, 16);
    uint32_t newCol = strtoul(newParams.colors[newIdx].c_str() + (newParams.colors[newIdx][0] == '#' ? 1 : 0), nullptr, 16);
    uint32_t blendedCol = color_blend(prevCol, newCol, uint8_t(progress * 255));
    uint8_t blend_phase = (phase + (i * 256 / std::max(1u, static_cast<unsigned int>(ledCount-1)))) % 256;
    size_t seg = (blend_phase * (maxCount - 1)) / 256;
    size_t seg_next = (seg + 1) % maxCount;
    uint8_t local_blend = (blend_phase * (maxCount - 1)) % 256;
    uint32_t segPrevCol = strtoul(prevParams.colors[seg].c_str() + (prevParams.colors[seg][0] == '#' ? 1 : 0), nullptr, 16);
    uint32_t segNewCol = strtoul(newParams.colors[seg_next].c_str() + (newParams.colors[seg_next][0] == '#' ? 1 : 0), nullptr, 16);
    uint32_t fxColor = color_blend(blendedCol, color_blend(segPrevCol, segNewCol, uint8_t(progress * 255)), local_blend);
    uint8_t r = scale((fxColor >> 16) & 0xFF);
    uint8_t g = scale((fxColor >> 8) & 0xFF);
    uint8_t b = scale(fxColor & 0xFF);
    if (i == 0) {
      debugPrint("[blend_effect] progress: ");
      debugPrint(progress, 3);
      debugPrint(" sample LED0 color: #");
      char hex[10];
      snprintf(hex, sizeof(hex), "%02X%02X%02X", r, g, b);
      debugPrintln(hex);
    }
    if (buffer) {
      buffer[i] = (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
    } else {
      setPixelColorUnified(i, r, g, b);
    }
  }
  if (!buffer) showStrip();
  return 0;
}
