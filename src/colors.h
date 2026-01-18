#pragma once
#include <cstdint>

// Blends two 32-bit RGBW colors by a fractional amount (0.0-1.0), applies brightness, and returns r,g,b,w
inline void blend_rgbw_brightness(uint32_t c0, uint32_t c1, float frac, uint8_t brightness, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& w) {
  r = (uint8_t)((((c0 >> 16) & 0xFF) * (1.0f - frac) + ((c1 >> 16) & 0xFF) * frac) * brightness / 255);
  g = (uint8_t)((((c0 >> 8) & 0xFF) * (1.0f - frac) + ((c1 >> 8) & 0xFF) * frac) * brightness / 255);
  b = (uint8_t)((( (c0 & 0xFF) * (1.0f - frac) + (c1 & 0xFF) * frac) * brightness / 255));
  w = (uint8_t)((((c0 >> 24) & 0xFF) * (1.0f - frac) + ((c1 >> 24) & 0xFF) * frac) * brightness / 255);
}

// Blends two 24-bit RGB colors by a fractional amount (0.0-1.0), applies brightness, and returns r,g,b
inline void blend_rgb_brightness(uint32_t c1, uint32_t c2, float frac, uint8_t brightness, uint8_t& r, uint8_t& g, uint8_t& b) {
  r = (uint8_t)((((c1 >> 16) & 0xFF) * (1.0f - frac) + ((c2 >> 16) & 0xFF) * frac) * brightness / 255);
  g = (uint8_t)((((c1 >> 8) & 0xFF) * (1.0f - frac) + ((c2 >> 8) & 0xFF) * frac) * brightness / 255);
  b = (uint8_t)((( (c1 & 0xFF) * (1.0f - frac) + (c2 & 0xFF) * frac) * brightness / 255));
}

// Packs 8-bit r, g, b into 24-bit uint32_t
inline uint32_t pack_rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

// Unpacks 24-bit uint32_t color into r, g, b
inline void unpack_rgb(uint32_t color, uint8_t& r, uint8_t& g, uint8_t& b) {
  r = (color >> 16) & 0xFF;
  g = (color >> 8) & 0xFF;
  b = color & 0xFF;
}

// Scales 24-bit RGB color by brightness (0-255)
inline void scale_rgb_brightness(uint32_t color, uint8_t brightness, uint8_t& r, uint8_t& g, uint8_t& b) {
  r = (uint8_t)(((color >> 16) & 0xFF) * brightness / 255);
  g = (uint8_t)(((color >> 8) & 0xFF) * brightness / 255);
  b = (uint8_t)((color & 0xFF) * brightness / 255);
}
