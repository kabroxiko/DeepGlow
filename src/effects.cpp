#include "effects.h"
#include "state.h"
#include <vector>
// Use correct NeoPixelBus method types

#if !defined(HAS_WS2812X_TYPEDEF)
#include <NeoPixelBus.h>
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel0> NeoEsp32Rmt0Ws2812xMethod;
#define HAS_WS2812X_TYPEDEF
#endif
#if !defined(HAS_SK6812_TYPEDEF)
#include <NeoPixelBus.h>
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannel0> NeoEsp32Rmt0Sk6812Method;
#define HAS_SK6812_TYPEDEF
#endif

extern void* strip;
extern Configuration config;

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
  if (config.led.type.equalsIgnoreCase("SK6812")) {
    auto* s = (NeoPixelBus<NeoRgbwFeature, NeoEsp32Rmt0Sk6812Method>*)strip;
    if (config.led.colorOrder.equalsIgnoreCase("GRB")) {
      s->SetPixelColor(i, RgbwColor(g, r, b, 0));
    } else {
      s->SetPixelColor(i, RgbwColor(r, g, b, 0));
    }
  } else {
    if (config.led.colorOrder.equalsIgnoreCase("RGB")) {
      auto* s = (NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt0Ws2812xMethod>*)strip;
      s->SetPixelColor(i, RgbColor(r, g, b));
    } else {
      auto* s = (NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>*)strip;
      s->SetPixelColor(i, RgbColor(g, r, b));
    }
  }
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

// Solid color effect: fills the strip with color[0]
uint16_t solid_effect() {
  extern uint32_t color[8];
  if (!strip) return 0;
  extern SystemState state;
  uint8_t brightness = state.brightness;
  auto scale = [brightness](uint8_t c) -> uint8_t { return (uint16_t(c) * brightness) / 255; };
  // Use only the first color in the array
  uint32_t solidColor = color[0];
  uint8_t r = scale((solidColor >> 16) & 0xFF);
  uint8_t g = scale((solidColor >> 8) & 0xFF);
  uint8_t b = scale(solidColor & 0xFF);
  uint16_t count = 0;
  if (config.led.type.equalsIgnoreCase("SK6812")) {
    auto* s = (NeoPixelBus<NeoRgbwFeature, NeoEsp32Rmt0Sk6812Method>*)strip;
    count = s->PixelCount();
  } else if (config.led.colorOrder.equalsIgnoreCase("RGB")) {
    auto* s = (NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt0Ws2812xMethod>*)strip;
    count = s->PixelCount();
  } else {
    auto* s = (NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>*)strip;
    count = s->PixelCount();
  }
  for (uint16_t i = 0; i < count; i++) {
    setPixelColorUnified(i, r, g, b);
  }
  // Show on any strip
  if (config.led.type.equalsIgnoreCase("SK6812")) {
    auto* s = (NeoPixelBus<NeoRgbwFeature, NeoEsp32Rmt0Sk6812Method>*)strip;
    s->Show();
  } else if (config.led.colorOrder.equalsIgnoreCase("RGB")) {
    auto* s = (NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt0Ws2812xMethod>*)strip;
    s->Show();
  } else {
    auto* s = (NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>*)strip;
    s->Show();
  }
  return 0;
}
REGISTER_EFFECT("Solid", solid_effect);

// Blend effect: smoothly blend between two colors using WLED's mode_blends logic
uint16_t blend_effect() {
  extern uint32_t color[2];
  if (!strip) return 0;
  extern SystemState state;
  uint8_t brightness = state.brightness;
  auto scale = [brightness](uint8_t c) -> uint8_t { return (uint16_t(c) * brightness) / 255; };
  static uint8_t blend = 0;
  static int8_t direction = 1;
  blend += direction;
  if (blend == 0 || blend == 255) direction = -direction;

  uint16_t count = 0;
  if (config.led.type.equalsIgnoreCase("SK6812")) {
    auto* s = (NeoPixelBus<NeoRgbwFeature, NeoEsp32Rmt0Sk6812Method>*)strip;
    count = s->PixelCount();
  } else if (config.led.colorOrder.equalsIgnoreCase("RGB")) {
    auto* s = (NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt0Ws2812xMethod>*)strip;
    count = s->PixelCount();
  } else {
    auto* s = (NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>*)strip;
    count = s->PixelCount();
  }
  for (uint16_t i = 0; i < count; i++) {
    uint32_t blended = color_blend(color[0], color[1], blend);
    uint8_t r = scale((blended >> 16) & 0xFF);
    uint8_t g = scale((blended >> 8) & 0xFF);
    uint8_t b = scale(blended & 0xFF);
    setPixelColorUnified(i, r, g, b);
  }
  // Show on any strip
  if (config.led.type.equalsIgnoreCase("SK6812")) {
    auto* s = (NeoPixelBus<NeoRgbwFeature, NeoEsp32Rmt0Sk6812Method>*)strip;
    s->Show();
  } else if (config.led.colorOrder.equalsIgnoreCase("RGB")) {
    auto* s = (NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt0Ws2812xMethod>*)strip;
    s->Show();
  } else {
    auto* s = (NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>*)strip;
    s->Show();
  }
  return 0;
}
REGISTER_EFFECT("Blend", blend_effect);
