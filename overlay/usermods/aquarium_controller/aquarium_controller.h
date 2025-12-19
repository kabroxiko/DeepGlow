#pragma once

#include "wled.h"
#include "aquarium_effects.h"

/*
 * Aquarium Controller Usermod
 *
 * Transforms WLED into a specialized aquarium lighting controller
 * with fish-safe presets, automated scheduling, and aquarium-specific effects.
 */
// Default configuration values
#define DEFAULT_MIN_TRANSITION      10  // seconds
#define DEFAULT_MAX_BRIGHTNESS      200

class AquariumControllerUsermod : public Usermod {
  private:
    // Configuration
    bool enabled = true;
    uint8_t minTransitionSeconds = DEFAULT_MIN_TRANSITION;
    uint8_t maxBrightness = DEFAULT_MAX_BRIGHTNESS;

    // Constants
    static const char _name[];
    static const char _enabled[];

    /**
     * Initialize aquarium presets
     */
    void initializePresets() {
      DEBUG_PRINTLN(F("Aquarium Controller: Effects registered, use WLED time presets for scheduling"));
    }

    /**
     * Ensure minimum transition time for fish safety
     */
    void enforceSafeTransition() {
      if (!enabled) return;

      // Ensure transition time is at least minTransitionSeconds
      uint16_t minTransition = minTransitionSeconds * 10; // Convert to tenths of seconds
      if (transitionDelay < minTransition) {
        transitionDelay = minTransition;
      }

      // Limit maximum brightness
      if (bri > maxBrightness) {
        bri = maxBrightness;
      }
    }

  public:
    void setup() {
      DEBUG_PRINTLN(F("Aquarium Controller: Starting setup"));

      strip.addEffect(FX_MODE_AQUARIUM_RIPPLE,        &mode_aquarium_ripple,        _data_FX_MODE_AQUARIUM_RIPPLE);
      strip.addEffect(FX_MODE_AQUARIUM_GENTLE_WAVE,   &mode_aquarium_gentle_wave,   _data_FX_MODE_AQUARIUM_GENTLE_WAVE);
      strip.addEffect(FX_MODE_AQUARIUM_SUNRISE,       &mode_aquarium_sunrise,       _data_FX_MODE_AQUARIUM_SUNRISE);
      strip.addEffect(FX_MODE_AQUARIUM_CORAL_SHIMMER, &mode_aquarium_coral_shimmer, _data_FX_MODE_AQUARIUM_CORAL_SHIMMER);
      strip.addEffect(FX_MODE_AQUARIUM_DEEP_OCEAN,    &mode_aquarium_deep_ocean,    _data_FX_MODE_AQUARIUM_DEEP_OCEAN);
      strip.addEffect(FX_MODE_AQUARIUM_MOONLIGHT,     &mode_aquarium_moonlight,     _data_FX_MODE_AQUARIUM_MOONLIGHT);

      DEBUG_PRINTLN(F("Aquarium Controller: Registered 6 custom effects"));

      initializePresets();
    }

    void loop() {
      if (!enabled) return;

      enforceSafeTransition();
    }

    void addToJsonInfo(JsonObject& root) {
      if (!enabled) return;

      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray aquariumInfo = user.createNestedArray(F("Aquarium Mode"));
      aquariumInfo.add(F("Safety: Active"));
      aquariumInfo.add(String(F("Max Bri: ")) + String(maxBrightness));
    }

    void addToJsonState(JsonObject& root) {
      if (!enabled) return;

      JsonObject aquarium = root.createNestedObject(F("aquarium"));
      aquarium[F("enabled")] = enabled;
      aquarium[F("maxBrightness")] = maxBrightness;
    }

    void readFromJsonState(JsonObject& root) {
      if (!enabled) return;

      JsonObject aquarium = root[F("aquarium")];
      if (!aquarium.isNull()) {
        if (aquarium.containsKey(F("maxBrightness"))) {
          maxBrightness = aquarium[F("maxBrightness")];
        }
      }
    }

    void addToConfig(JsonObject& root) {
      JsonObject top = root.createNestedObject(F("AquariumController"));
      top[F("enabled")] = enabled;

      JsonObject safety = top.createNestedObject(F("safety"));
      safety[F("minTransitionSeconds")] = minTransitionSeconds;
      safety[F("maxBrightness")] = maxBrightness;
    }

    bool readFromConfig(JsonObject& root) {
      JsonObject top = root[F("AquariumController")];

      if (top.isNull()) {
        DEBUG_PRINTLN(F("Aquarium Controller: No config found, using defaults"));
        return false;
      }

      bool configComplete = true;

      configComplete &= getJsonValue(top[F("enabled")], enabled, true);

      JsonObject safety = top[F("safety")];
      if (!safety.isNull()) {
        configComplete &= getJsonValue(safety[F("minTransitionSeconds")], minTransitionSeconds, DEFAULT_MIN_TRANSITION);
        configComplete &= getJsonValue(safety[F("maxBrightness")], maxBrightness, DEFAULT_MAX_BRIGHTNESS);
      }

      return configComplete;
    }

    uint16_t getId() {
      return USERMOD_ID_AQUARIUM_CONTROLLER;
    }
};
