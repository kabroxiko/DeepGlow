#ifndef EFFECTS_H
#define EFFECTS_H

#include <Arduino.h>
#include "config.h"
#include <vector>

struct EffectRegistryEntry {
	const char* name;
	uint16_t (*handler)();
};

// Registry accessor and registration function (must be declared before macro)
const std::vector<EffectRegistryEntry>& getEffectRegistry();

// Registration macro (uses static object constructor)
#define REGISTER_EFFECT(NAME, FUNC) \
	namespace { \
	struct _AutoReg_##FUNC { \
		_AutoReg_##FUNC() { \
			_registerEffect(NAME, FUNC); \
		} \
	}; \
	static _AutoReg_##FUNC _autoReg_##FUNC; \
	}

void _registerEffect(const char* name, uint16_t (*handler)());

// Solid color effect
// Render solid effect to buffer if provided, else to LEDs
uint16_t solid_effect(uint32_t* buffer = nullptr, size_t count = 0);
// Render blend effect with WLED-style transition blending
uint16_t blend_effect(uint32_t* buffer, size_t count, const EffectParams& prevParams, const EffectParams& newParams, float progress);
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
