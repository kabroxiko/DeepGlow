#pragma once
#include <cstdint>


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

// Pack r, g, b, w components into a RRGGBBWW uint32_t
inline uint32_t pack_rgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
  return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)w;
}

// Unpack a packed RRGGBBWW uint32_t into r, g, b, w components
inline void unpack_rgbw(uint32_t color, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &w) {
  r = (color >> 24) & 0xFF;
  g = (color >> 16) & 0xFF;
  b = (color >> 8) & 0xFF;
  w = color & 0xFF;
}

// Scale r, g, b, w by brightness (0-255)
inline void scale_rgbw_brightness(uint8_t in_r, uint8_t in_g, uint8_t in_b, uint8_t in_w, uint8_t brightness, uint8_t &out_r, uint8_t &out_g, uint8_t &out_b, uint8_t &out_w) {
  out_r = (uint16_t)in_r * brightness / 255;
  out_g = (uint16_t)in_g * brightness / 255;
  out_b = (uint16_t)in_b * brightness / 255;
  out_w = (uint16_t)in_w * brightness / 255;
}

// Blends two 32-bit RGBW colors by a fractional amount (0.0-1.0), applies brightness, and returns r,g,b,w
// Expects color in RRGGBBWW order (WW in LSB, RR in MSB)
inline void blend_rgbw_brightness(uint32_t c0, uint32_t c1, float frac, uint8_t brightness, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& w) {
  r = (uint8_t)((((c0 >> 24) & 0xFF) * (1.0f - frac) + ((c1 >> 24) & 0xFF) * frac) * brightness / 255);
  g = (uint8_t)((((c0 >> 16) & 0xFF) * (1.0f - frac) + ((c1 >> 16) & 0xFF) * frac) * brightness / 255);
  b = (uint8_t)((((c0 >> 8) & 0xFF) * (1.0f - frac) + ((c1 >> 8) & 0xFF) * frac) * brightness / 255);
  w = (uint8_t)((( (c0 & 0xFF) * (1.0f - frac) + (c1 & 0xFF) * frac) * brightness / 255));
}
