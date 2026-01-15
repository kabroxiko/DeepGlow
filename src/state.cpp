#include "bus_manager.h"
#include "state.h"
#include "effects.h"
#include "transition.h"
#include "webserver.h"
#include "display.h"
#include "debug.h"

// Needed for effect speed control in updateLEDs
extern volatile uint8_t g_effectSpeed;

SystemState state;

extern BusManager busManager;

// Global user-selected colors (fixed size)
#include <array>
std::array<uint32_t, 8> color = {0x0000FF, 0x00FFFF, 0x00FFFF, 0x00FFFF, 0x00FFFF, 0x00FFFF, 0x00FFFF, 0x00FFFF};
size_t colorCount = 2;

extern Configuration config;
extern Scheduler scheduler;
extern TransitionEngine transition;
extern WebServerManager webServer;
extern void* strip;
extern int8_t lastScheduledPreset;

// Setup prevParams/newParams for effect blending
extern EffectParams prevParams;
extern EffectParams newParams;

// WLED-style transition buffer
#include <vector>
static std::vector<uint32_t> previousFrame;
static EffectParams transitionPrevParams;
struct PendingTransitionState {
  uint8_t effect = 0;
  EffectParams params;
  uint8_t preset = 0;
};
static PendingTransitionState pendingTransition;

void applyPreset(uint8_t presetId, bool setManualOverride) {
	// Find preset by id
	auto it = std::find_if(config.presets.begin(), config.presets.end(), [presetId](const Preset& p) { return p.id == presetId; });
	if (it == config.presets.end() || !it->enabled) {
		debugPrintln("Invalid preset ID");
		return;
	}
	Preset& preset = *it;
	// Debug: log preset application
	debugPrint("[applyPreset] presetId:"); debugPrint((int)presetId);
	debugPrint(" effect:"); debugPrint((int)preset.effect);
	debugPrint(" colors:");
	for (size_t i = 0; i < preset.params.colors.size(); ++i) {
		debugPrint(" "); debugPrint(preset.params.colors[i]);
	}
	debugPrintln("");
	uint8_t timerBrightnessPercent = 100;
	for (size_t i = 0; i < config.timers.size(); i++) {
		if (config.timers[i].presetId == presetId && config.timers[i].enabled) {
			timerBrightnessPercent = config.timers[i].brightness;
			break;
		}
	}
	uint8_t brightnessValue = (uint8_t)((timerBrightnessPercent / 100.0) * 255);
	uint8_t safeBrightness = min(brightnessValue, config.safety.maxBrightness);

	// --- WLED-style transition logic ---
	// 1. Setup transition using the current state as 'previous', do NOT touch color[] or state yet
	uint8_t previousBrightness = transition.getCurrentBrightness(); // Cache previous brightness at start
	bool doTransition = (state.effect >= 0);
	uint32_t transTime = state.transitionTime;
	if (transTime < config.safety.minTransitionTime) {
		transTime = config.safety.minTransitionTime;
	}

	size_t count = busManager.getPixelCount();
	previousFrame.resize(count);
	bool colorsChanged = false;
	debugPrint("[applyPreset] branch: ");
	debugPrint(" effect index: ");
	debugPrintln((int)preset.effect);
	debugPrint("[applyPreset] transition duration (ms): ");
	debugPrintln((int)transTime);
	if (doTransition && state.effect >= 0) {
		debugPrintln("transition from previous effect");
		// Log current state.params.colors and preset.params.colors before assignment
		debugPrint("[applyPreset] state.params.colors (before): ");
		for (size_t i = 0; i < state.params.colors.size(); ++i) {
			debugPrint(state.params.colors[i]); debugPrint(" ");
		}
		debugPrintln("");
		debugPrint("[applyPreset] preset.params.colors: ");
		for (size_t i = 0; i < preset.params.colors.size(); ++i) {
			debugPrint(preset.params.colors[i]); debugPrint(" ");
		}
		debugPrintln("");

		// Capture previous and new effect params BEFORE any preset changes
		prevParams = state.params;
		newParams = preset.params;
		// Log prevParams.colors and newParams.colors after assignment
		debugPrint("[applyPreset] prevParams.colors: ");
		for (size_t i = 0; i < prevParams.colors.size(); ++i) {
			char hex[10];
			snprintf(hex, sizeof(hex), "#%06X", strtoul(prevParams.colors[i].c_str() + (prevParams.colors[i][0] == '#' ? 1 : 0), nullptr, 16) & 0xFFFFFF);
			debugPrint(hex); debugPrint(" ");
		}
		debugPrintln("");
		debugPrint("[applyPreset] newParams.colors: ");
		for (size_t i = 0; i < newParams.colors.size(); ++i) {
			char hex[10];
			snprintf(hex, sizeof(hex), "#%06X", strtoul(newParams.colors[i].c_str() + (newParams.colors[i][0] == '#' ? 1 : 0), nullptr, 16) & 0xFFFFFF);
			debugPrint(hex); debugPrint(" ");
		}
		debugPrintln("");
		if (state.effect == 1 || (state.effect == preset.effect && prevParams.colors.size() == newParams.colors.size())) {
			blend_effect(previousFrame.data(), count, 0.0f);
		} else {
			solid_effect(previousFrame.data(), count);
		}
		debugPrint("[applyPreset] forceCurrentBrightness (prev): ");
		debugPrintln((int)previousBrightness);
		transition.forceCurrentBrightness(previousBrightness);
	} else {
		if (!state.power || state.brightness == 0) {
			debugPrintln("fade in from black (LEDs were off)");
			std::fill(previousFrame.begin(), previousFrame.end(), 0x000000);
			debugPrintln("[applyPreset] forceCurrentBrightness: 0");
			transition.forceCurrentBrightness(0);
		} else {
			debugPrintln("blend from current frame (LEDs were on)");
			BusNeoPixel* neo = busManager.getNeoPixelBus();
			for (size_t i = 0; i < count; ++i) {
				previousFrame[i] = neo ? neo->getPixelColor(i) : 0;
			}
			debugPrint("[applyPreset] forceCurrentBrightness (on): ");
			debugPrintln((int)previousBrightness);
			transition.forceCurrentBrightness(previousBrightness);
		}
	}

	// 2. Now update color[] and all state for the new preset
	size_t n = preset.params.colors.size();
	colorCount = n > 0 ? n : 1;
	for (size_t i = 0; i < 8; ++i) {
		if (i < n) {
			const String& hex = preset.params.colors[i];
			color[i] = (uint32_t)strtoul(hex.c_str() + (hex[0] == '#' ? 1 : 0), nullptr, 16);
		} else {
			color[i] = (i == 0) ? 0x0000FF : 0x00FFFF;
		}
	}

	// Start transitions (with previous effect/params still set)
	transition.startTransition(safeBrightness, transTime);
	uint32_t newColor1 = colorCount > 0 ? color[0] : 0x0000FF;
	uint32_t newColor2 = colorCount > 1 ? color[1] : 0x00FFFF;
	transition.startColorTransition(newColor1, newColor2, transTime);

	// Store new effect/params for later commit after transition
	pendingTransition.effect = preset.effect;
	pendingTransition.params = preset.params;
	pendingTransition.params.colors.clear();
	for (size_t i = 0; i < n; ++i) {
		char hex[10];
		snprintf(hex, sizeof(hex), "#%06X", color[i] & 0xFFFFFF);
		pendingTransition.params.colors.push_back(String(hex));
	}
	pendingTransition.preset = preset.id;
	state.power = true;
	state.inTransition = true;
	state.currentPreset = preset.id;
	webServer.broadcastState();
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
		// Capture current LED state for blending
		BusNeoPixel* neo = busManager.getNeoPixelBus();
		size_t count = busManager.getPixelCount();
		previousFrame.resize(count);
		for (size_t i = 0; i < count; ++i) {
			previousFrame[i] = neo ? neo->getPixelColor(i) : 0;
		}
		transition.startTransition(brightness, transTime);
		webServer.broadcastState();
	}
}



void setEffect(uint8_t effect, const EffectParams& params) {
	state.effect = effect;
	state.params = params;
	state.params.colors.clear();
	size_t n = 0;
	for (size_t i = 0; i < 8; ++i) {
		// Only push colors that are actually set (not default)
		if (i == 0 || color[i] != 0x00FFFF) {
			char hex[10];
			snprintf(hex, sizeof(hex), "#%06X", color[i] & 0xFFFFFF);
			state.params.colors.push_back(String(hex));
			n++;
		} else {
			break;
		}
	}
	BusNeoPixel* neo = busManager.getNeoPixelBus();
	if (!neo || !neo->getStrip()) return;
	const auto& reg = getEffectRegistry();
	if (effect < reg.size() && reg[effect].handler) {
		// Update global effect speed if present in params
		   if (params.speed > 0) {
			   extern volatile uint8_t g_effectSpeed;
			   // Map UI speed (1-100) to WLED effect speed (1-255)
			   g_effectSpeed = (params.speed * 254) / 100 + 1;
		   }
		reg[effect].handler();
	}
}

// Call this when user changes color from UI/API
void setUserColor(const uint32_t* newColor, size_t count) {
	colorCount = count;
	debugPrint("[setUserColor] count: ");
	debugPrintln((int)count);
	for (size_t i = 0; i < count; ++i) {
		debugPrint("[setUserColor] newColor[");
		debugPrint((int)i);
		debugPrint("] = 0x");
		debugPrintln((unsigned int)newColor[i], HEX);
	}
	// Fill color array with new values, pad as needed
	for (size_t i = 0; i < 8; ++i) {
		if (i < count) {
			color[i] = newColor[i];
		} else {
			color[i] = (i == 0) ? 0x0000FF : 0x00FFFF;
		}
	}
	debugPrint("[setUserColor] color: ");
	for (size_t i = 0; i < 8; ++i) {
		debugPrint("["); debugPrint((int)i); debugPrint("] = 0x"); debugPrintln((unsigned int)color[i], HEX);
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
	// Cache progress at the start for consistency
	float progress = transition.getProgress();
	// Debug output only when in transition
	if (transition.isTransitioning()) {
		debugPrint("[updateLEDs] power:"); debugPrint((int)state.power);
		debugPrint(" effect:"); debugPrint((int)pendingTransition.effect);
		debugPrint(" prevEffect:"); debugPrint((int)state.prevEffect);
		debugPrint(" inTransition:"); debugPrint((int)transition.isTransitioning());
		debugPrint(" brightness:"); debugPrint((int)transition.getCurrentBrightness());
		debugPrint(" progress:"); debugPrint(progress, 3);
		debugPrintln("");
	}
    BusNeoPixel* neo = busManager.getNeoPixelBus();
    if (!neo || !neo->getStrip()) return;
    if (!state.power) {
        busManager.turnOffLEDs();
        state.inTransition = false;
        state.brightness = 0;
        digitalWrite(config.led.relayPin, config.led.relayActiveHigh ? LOW : HIGH);
        return;
    }
	size_t count = busManager.getPixelCount();
	static bool pendingCommit = false;
	const auto& reg = getEffectRegistry();
	// If a transition is running, blend and set pendingCommit
	if (transition.isTransitioning() && previousFrame.size() == count) {
		pendingCommit = true;
		debugPrint("[updateLEDs] Using transition.getProgress(): ");
		debugPrintln(progress, 3);
		uint8_t nextIdx = pendingTransition.effect;
		if (nextIdx == 1) { // 1 = blend effect
			blend_effect(nullptr, count, progress);
		} else if (nextIdx == 0) { // 0 = solid effect
			solid_effect(nullptr, count);
		} else if (nextIdx < reg.size() && reg[nextIdx].handler) {
			reg[nextIdx].handler();
		} else {
			solid_effect(nullptr, count);
		}
	} else {
		// Always commit pendingTransition after transition completes or if effect/params are out of sync
		bool needCommit = pendingCommit && !transition.isTransitioning();
		needCommit = needCommit || (state.effect != pendingTransition.effect || state.currentPreset != pendingTransition.preset);
		if (needCommit) {
			debugPrint("[updateLEDs] Committing effect: ");
			debugPrint((int)pendingTransition.effect);
			debugPrint(", preset: ");
			debugPrintln((int)pendingTransition.preset);
			state.effect = pendingTransition.effect;
			state.params = pendingTransition.params;
			state.currentPreset = pendingTransition.preset;
			setEffect(state.effect, state.params);
			// Update prevEffect/prevParams for next transition
			state.prevEffect = state.effect;
			state.prevParams = state.params;
			pendingCommit = false;
		}
		previousFrame.clear();
		if (state.effect == 1) {
			blend_effect(nullptr, 0, 1.0f);
		} else if (state.effect == 0) {
			solid_effect(nullptr, 0);
		} else if (state.effect < reg.size() && reg[state.effect].handler) {
			reg[state.effect].handler();
		} else {
			solid_effect(nullptr, 0);
		}
	}
    uint8_t currentBrightness = transition.getCurrentBrightness();
    state.inTransition = false;
    state.brightness = currentBrightness;
    digitalWrite(config.led.relayPin, config.led.relayActiveHigh ? HIGH : LOW);
}
