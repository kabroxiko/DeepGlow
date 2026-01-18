#ifndef EFFECTS_H
#define EFFECTS_H

#include <Arduino.h>
#include "config.h"
#include <vector>
#include <cstdint>
#include <array>
#include <cstddef>
#include "state.h"

// Render the given effect and params into a buffer (does not update LEDs)
void renderEffectToBuffer(uint8_t effectId, const EffectParams& params, std::vector<uint32_t>& buffer, size_t ledCount, const std::array<uint32_t, 8>& colors, size_t colorCount, uint8_t brightness);

// All effect frame generators now take no parameters and use global buffer/ledCount
typedef void (*EffectFrameGen)();
extern std::vector<EffectFrameGen> effectFrameRegistry;

struct PendingTransitionState {
	uint8_t effect = 0;
	EffectParams params;
	uint8_t preset = 0;
};

// Removed EffectRegistryEntry structure and registration macros
// Utility to print all colors sent to the strip
void debugPrintStripColors(const std::vector<uint32_t>& colors, const char* tag = "strip colors");

// Centralized effect speed to delay mapping
uint32_t getEffectDelayMs(const EffectParams& params);

extern std::array<uint32_t, 8> color;
extern size_t colorCount;
extern void* strip;

#ifdef __cplusplus
extern "C" {
#endif

void updatePixelCount();

#ifdef __cplusplus
}
#endif

#endif // EFFECTS_H
