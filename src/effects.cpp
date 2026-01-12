// Ensure access to SystemState and state
#include "effects.h"
#include "state.h"
extern WS2812FX* strip;

// Ported WLED mode_blends: Blends random colors across palette
uint16_t custom_blend_fx(void) {
  if (!strip) return 0;
  WS2812FX::Segment* seg = strip->getSegment();
  uint16_t seg_len = seg->stop - seg->start + 1;
  uint16_t pixelLen = seg_len > 255 ? 255 : seg_len;
  static std::vector<uint32_t> pixels(256, 0);
  if (pixels.size() < pixelLen) pixels.resize(pixelLen, 0);

  // Map intensity (0-255) to blend speed (10-128) using state.params.intensity
  extern SystemState state;
  uint8_t blendSpeed = map(state.params.intensity, 0, 255, 10, 128);
  // Shift based on time and speed
  unsigned long now = millis();
  unsigned shift = (now * ((seg->speed >> 3) + 1)) >> 8;

  // Palette cycling: use color1/color2 as palette endpoints
  uint32_t palette[4] = { seg->colors[0], seg->colors[1], seg->colors[0], seg->colors[1] };
  auto paletteColor = [&](uint8_t idx) -> uint32_t {
    // Simple 2-color palette cycling
    float t = (sin((idx / 255.0 + shift / 64.0) * 3.14159 * 2) + 1.0) * 0.5;
    return strip->color_blend(palette[0], palette[1], (uint8_t)(t * 255));
  };

  for (unsigned i = 0; i < pixelLen; i++) {
    uint32_t target = paletteColor((i + 1) * 16 + shift);
    pixels[i] = strip->color_blend(pixels[i], target, blendSpeed);
  }

  unsigned offset = 0;
  for (unsigned i = 0; i < seg_len; i++) {
    strip->setPixelColor(seg->start + i, pixels[offset++]);
    if (offset >= pixelLen) offset = 0;
  }

  return seg->speed;
}
