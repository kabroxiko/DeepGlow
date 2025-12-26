#pragma once

#include "wled.h"

#define FX_MODE_AQUARIUM_RIPPLE        218
#define FX_MODE_AQUARIUM_GENTLE_WAVE   219
#define FX_MODE_AQUARIUM_SUNRISE       220
#define FX_MODE_AQUARIUM_CORAL_SHIMMER 221
#define FX_MODE_AQUARIUM_DEEP_OCEAN    222
#define FX_MODE_AQUARIUM_MOONLIGHT     223

/*
 * Aquarium-specific LED effects
 * These effects are designed to simulate underwater environments
 */

// Water Ripple Effect - simulates light rippling through water
static uint16_t mode_aquarium_ripple(void) {
  const uint16_t seglen = SEGLEN;
  const uint32_t timer = strip.now / 50;

  for (uint16_t i = 0; i < seglen; i++) {
    uint8_t wave1 = sin8((timer + i * SEGMENT.speed / 32) & 0xFF);
    uint8_t wave2 = sin8((timer * 2 - i * SEGMENT.speed / 64 + 85) & 0xFF);
    uint8_t combined = (wave1 + wave2) / 2;

    uint32_t color = SEGMENT.color_from_palette(combined, false, false, 0);
    SEGMENT.setPixelColor(i, color_fade(color, combined));
  }

  return FRAMETIME;
}
// pal=15 means palette index 15 in gGradientPalettes (see palettes.cpp)
// Example: pal=15 -> gr65_hult_gp
static const char _data_FX_MODE_AQUARIUM_RIPPLE[] PROGMEM = "Aquarium Ripple@Speed,Intensity;,;Color;;1;pal=15";

// Gentle Wave Effect - smooth flowing waves
static uint16_t mode_aquarium_gentle_wave(void) {
  const uint16_t seglen = SEGLEN;
  const uint32_t timer = strip.now / 80;
  const uint8_t waveSpeed = SEGMENT.speed;
  const uint8_t waveIntensity = SEGMENT.intensity;

  for (uint16_t i = 0; i < seglen; i++) {
    uint8_t pos = (timer * waveSpeed / 128 + i * 255 / seglen) & 0xFF;
    uint8_t brightness = sin8(pos);
    brightness = scale8(brightness, waveIntensity);

    uint32_t color = SEGMENT.color_from_palette(pos, false, false, 0);
    SEGMENT.setPixelColor(i, color_fade(color, brightness));
  }

  return FRAMETIME;
}
// pal=15 means palette index 15 in gGradientPalettes (see palettes.cpp)
// Example: pal=15 -> gr65_hult_gp
static const char _data_FX_MODE_AQUARIUM_GENTLE_WAVE[] PROGMEM = "Aquarium Gentle Wave@Speed,Intensity;,;Color;;1;pal=15";

// Sunrise/Sunset Simulation - gradual color shift from orange to blue
static uint16_t mode_aquarium_sunrise(void) {
  const uint16_t seglen = SEGLEN;
  static uint16_t sunriseProgress = 0;

  // Increment progress slowly
  if (SEGENV.call == 0) {
    sunriseProgress = 0;
    SEGENV.aux0 = 0;
  }

  // Gradual transition over time
  const uint8_t transitionSpeed = SEGMENT.speed;
  if (strip.now - SEGENV.step > (255 - transitionSpeed) * 2) {
    SEGENV.step = strip.now;
    if (SEGENV.aux0 < 255) {
      SEGENV.aux0++;
    }
  }

  // Create gradient from warm (orange/yellow) to cool (blue/white)
  for (uint16_t i = 0; i < seglen; i++) {
    uint8_t r, g, b;

    // Interpolate between sunrise colors
    if (SEGENV.aux0 < 85) {
      // Orange to yellow phase
      r = 255;
      g = map(SEGENV.aux0, 0, 85, 100, 215);
      b = 0;
    } else if (SEGENV.aux0 < 170) {
      // Yellow to white phase
      r = 255;
      g = 255;
      b = map(SEGENV.aux0, 85, 170, 0, 180);
    } else {
      // White to daylight blue phase
      r = map(SEGENV.aux0, 170, 255, 255, 180);
      g = map(SEGENV.aux0, 170, 255, 255, 220);
      b = 255;
    }

    // Apply intensity
    uint8_t brightness = SEGMENT.intensity;
    r = scale8(r, brightness);
    g = scale8(g, brightness);
    b = scale8(b, brightness);

    SEGMENT.setPixelColor(i, RGBW32(r, g, b, 0));
  }

  return FRAMETIME;
}
// pal=13 means palette index 13 in gGradientPalettes (see palettes.cpp)
// Example: pal=13 -> Sunset_Real_gp ("Sunset")
static const char _data_FX_MODE_AQUARIUM_SUNRISE[] PROGMEM = "Aquarium Sunrise@Speed,Brightness;;;1;pal=13";

// Coral Shimmer - gentle twinkling effect for reef tanks
static uint16_t mode_aquarium_coral_shimmer(void) {
  const uint16_t seglen = SEGLEN;
  const uint32_t timer = strip.now / 100;

  for (uint16_t i = 0; i < seglen; i++) {
    // Create subtle random shimmer
    uint8_t shimmer = sin8(timer + i * 13) / 4 + 191; // Range 191-255

    // Get base color from palette
    uint32_t color = SEGMENT.color_from_palette(i * 255 / seglen, false, false, 0);

    // Apply shimmer effect
    uint8_t r = scale8(R(color), shimmer);
    uint8_t g = scale8(G(color), shimmer);
    uint8_t b = scale8(B(color), shimmer);

    SEGMENT.setPixelColor(i, RGBW32(r, g, b, 0));
  }

  return FRAMETIME;
}
// pal=35 means palette index 35 in gGradientPalettes (see palettes.cpp)
// Example: pal=35 -> lava_gp ("Fire")
static const char _data_FX_MODE_AQUARIUM_CORAL_SHIMMER[] PROGMEM = "Coral Shimmer@Speed,Intensity;,;Color;;1;pal=35";

// Deep Ocean - slow, dark blue pulsing effect
static uint16_t mode_aquarium_deep_ocean(void) {
  const uint16_t seglen = SEGLEN;
  const uint32_t timer = strip.now / 200; // Very slow movement

  for (uint16_t i = 0; i < seglen; i++) {
    // Create slow pulse
    uint8_t pulse = sin8(timer + i * 5);

    // Deep blue color with subtle variation
    uint8_t r = 0;
    uint8_t g = scale8(16, pulse); // Very subtle green
    uint8_t b = scale8(80, pulse); // Dark blue

    // Apply intensity for brightness control
    r = scale8(r, SEGMENT.intensity);
    g = scale8(g, SEGMENT.intensity);
    b = scale8(b, SEGMENT.intensity);

    SEGMENT.setPixelColor(i, RGBW32(r, g, b, 0));
  }

  return FRAMETIME;
}
// pal=15 means palette index 15 in gGradientPalettes (see palettes.cpp)
// Example: pal=15 -> gr65_hult_gp
static const char _data_FX_MODE_AQUARIUM_DEEP_OCEAN[] PROGMEM = "Deep Ocean@Speed,Brightness;;;1;pal=15";

// Moonlight - ultra-dim blue for nighttime
static uint16_t mode_aquarium_moonlight(void) {
  const uint16_t seglen = SEGLEN;
  const uint32_t timer = strip.now / 500; // Very slow variation

  for (uint16_t i = 0; i < seglen; i++) {
    // Very subtle pulse
    uint8_t pulse = sin8(timer) / 16 + 239; // Range 239-255

    // Moonlight color - very dim blue
    uint8_t r = 10;
    uint8_t g = 10;
    uint8_t b = 32;

    // Apply pulse
    r = scale8(r, pulse);
    g = scale8(g, pulse);
    b = scale8(b, pulse);

    // Further reduce by intensity
    r = scale8(r, SEGMENT.intensity);
    g = scale8(g, SEGMENT.intensity);
    b = scale8(b, SEGMENT.intensity);

    SEGMENT.setPixelColor(i, RGBW32(r, g, b, 0));
  }

  return FRAMETIME;
}
// pal=60 means palette index 60 in gGradientPalettes (see palettes.cpp)
// Example: pal=60 -> semi_blue_gp ("Semi Blue")
static const char _data_FX_MODE_AQUARIUM_MOONLIGHT[] PROGMEM = "Moonlight@Speed,Brightness;;;1;pal=60";
