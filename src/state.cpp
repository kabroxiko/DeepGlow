#include <NeoPixelBus.h> // Added for NeoPixelBus migration
#include "state.h"
#include "effects.h"
#include "transition.h"
#include "webserver.h"
#include "display.h"

SystemState state;


// Global user-selected colors (always reflect last user action, up to 8 colors)
uint32_t color[8] = {0x0000FF, 0x00FFFF}; // Default Blue, Default Cyan

extern Configuration config;
extern Scheduler scheduler;
extern TransitionEngine transition;
extern WebServerManager webServer;
extern void* strip;
extern int8_t lastScheduledPreset;

void applyPreset(uint8_t presetId, bool setManualOverride) {
	if (presetId >= config.getPresetCount() || !config.presets[presetId].enabled) {
		debugPrintln("Invalid preset ID");
		return;
	}
	Preset& preset = config.presets[presetId];
	uint8_t timerBrightnessPercent = 100;
	for (size_t i = 0; i < config.timers.size(); i++) {
		if (config.timers[i].presetId == presetId && config.timers[i].enabled) {
			timerBrightnessPercent = config.timers[i].brightness;
			break;
		}
	}
	uint8_t brightnessValue = (uint8_t)((timerBrightnessPercent / 100.0) * 255);
	uint8_t safeBrightness = min(brightnessValue, config.safety.maxBrightness);
	uint32_t transTime = state.transitionTime;
	if (transTime < config.safety.minTransitionTime) {
		transTime = config.safety.minTransitionTime;
	}
	transition.startTransition(safeBrightness, transTime);

	// Set color[] to preset colors, then update effect and params
	size_t n = preset.params.colors.size();
	for (size_t i = 0; i < 8; ++i) {
		if (i < n) {
			const String& hex = preset.params.colors[i];
			color[i] = (uint32_t)strtoul(hex.c_str() + (hex[0] == '#' ? 1 : 0), nullptr, 16);
		} else {
			color[i] = (i == 0) ? 0x0000FF : 0x00FFFF;
		}
	}
	state.effect = preset.effect;
	state.params = preset.params;
	state.params.colors.clear();
	for (size_t i = 0; i < n; ++i) {
		char hex[10];
		snprintf(hex, sizeof(hex), "#%06X", color[i] & 0xFFFFFF);
		state.params.colors.push_back(String(hex));
	}
	state.currentPreset = presetId;
	state.power = true;
	state.inTransition = true;
	webServer.broadcastState();
	// No need to reset userColor1/2 here
	setEffect(state.effect, state.params);
}

void setPower(bool power) {
	if (state.power == power) {
		return;
	}
	state.power = power;
	digitalWrite(config.led.relayPin, power ? (config.led.relayActiveHigh ? HIGH : LOW) : (config.led.relayActiveHigh ? LOW : HIGH));
	uint8_t targetBrightness = power ? state.brightness : 0;
	uint32_t transTime = state.transitionTime;
	if (transTime < config.safety.minTransitionTime) {
		transTime = config.safety.minTransitionTime;
	}
	if (power) {
		transition.forceCurrentBrightness(state.brightness);
	}
	if (transition.getCurrentBrightness() != targetBrightness || !transition.isTransitioning()) {
		transition.startTransition(targetBrightness, transTime);
	}
	webServer.broadcastState();

}

void setBrightness(uint8_t brightness) {
	brightness = min(brightness, config.safety.maxBrightness);
	uint32_t transTime = state.transitionTime;
	if (transTime < config.safety.minTransitionTime) {
		transTime = config.safety.minTransitionTime;
	}
	uint8_t current = transition.getCurrentBrightness();
	if (brightness != current) {
		if (!transition.isTransitioning()) {
			transition.forceCurrentBrightness(state.brightness);
		}
		transition.startTransition(brightness, transTime);
		webServer.broadcastState();
	}
}



void setEffect(uint8_t effect, const EffectParams& params) {
	// Always use color[0/1] for color1/color2
	state.effect = effect;
	state.params = params;
	state.params.colors.clear();
	for (size_t i = 0; i < 8; ++i) {
		char hex[10];
		snprintf(hex, sizeof(hex), "#%06X", color[i] & 0xFFFFFF);
		state.params.colors.push_back(String(hex));
	}
	if (!strip) return;
	const auto& reg = getEffectRegistry();
	if (effect < reg.size() && reg[effect].handler) {
		reg[effect].handler();
	}
}

// Call this when user changes color from UI/API
void setUserColor(const uint32_t newColor[2]) {
	// Accept up to 8 colors from newColor
	for (size_t i = 0; i < 8; ++i) {
		color[i] = newColor[i];
	}
	state.params.colors.clear();
	for (size_t i = 0; i < 8; ++i) {
		char hex[10];
		snprintf(hex, sizeof(hex), "#%06X", color[i] & 0xFFFFFF);
		state.params.colors.push_back(String(hex));
	}
	setEffect(state.effect, state.params);
}

void updateLEDs() {
	if (!strip) return;
		if (!state.power) {
			if (config.led.type.equalsIgnoreCase("SK6812")) {
				auto* s = (NeoPixelBus<NeoRgbwFeature, NeoEsp32Rmt0Sk6812Method>*)strip;
				RgbwColor off(0, 0, 0, 0);
				for (uint16_t i = 0; i < s->PixelCount(); i++) {
					s->SetPixelColor(i, off);
				}
				s->Show();
			} else {
				auto* s = (NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt0Ws2812xMethod>*)strip;
				RgbColor off(0, 0, 0);
				for (uint16_t i = 0; i < s->PixelCount(); i++) {
					s->SetPixelColor(i, off);
				}
				s->Show();
			}
		state.inTransition = false;
		state.brightness = 0;
		digitalWrite(config.led.relayPin, config.led.relayActiveHigh ? LOW : HIGH);
		return;
	}
	if (state.effect == 1) {
		blend_effect();
	}
	uint8_t currentBrightness = transition.getCurrentBrightness();
	uint8_t prevBrightness = state.brightness;
	// NeoPixelBus does not support setBrightness directly; scale color values instead
	// Implement brightness scaling logic here if needed
	state.inTransition = false;
	state.brightness = currentBrightness;
	digitalWrite(config.led.relayPin, config.led.relayActiveHigh ? HIGH : LOW);
}
