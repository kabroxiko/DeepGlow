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
#define DEFAULT_MIN_TRANSITION      100  // deciseconds (10s)
#define DEFAULT_MAX_BRIGHTNESS      200

class AquariumControllerUsermod : public Usermod {
  private:
    // Configuration
    uint16_t minTransitionTime = DEFAULT_MIN_TRANSITION; // deciseconds
    uint8_t maxBrightness = DEFAULT_MAX_BRIGHTNESS;

    // Constants
    static const char _name[];
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
      // minTransitionTime is now in deciseconds (tenths of a second)
      uint16_t minTransition = minTransitionTime; // already in deciseconds
      if (minTransitionTime == 1) minTransition = 100; // 1 means 10s, not 0.1s (legacy fallback)
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
      enforceSafeTransition();
    }

    void addToJsonInfo(JsonObject& root) {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray aquariumInfo = user.createNestedArray(F("Aquarium Mode"));
      aquariumInfo.add(F("Safety: Active"));
      aquariumInfo.add(String(F("Max Bri: ")) + String(maxBrightness));
    }

    void addToJsonState(JsonObject& root) {
      JsonObject aquarium = root.createNestedObject(F("aquarium"));
      aquarium[F("maxBrightness")] = maxBrightness;
    }

    void readFromJsonState(JsonObject& root) {
      JsonObject aquarium = root[F("aquarium")];
      if (!aquarium.isNull()) {
        if (aquarium.containsKey(F("maxBrightness"))) {
          maxBrightness = aquarium[F("maxBrightness")];
        }
      }
    }

    void addToConfig(JsonObject& root) {
      JsonObject top = root.createNestedObject(F("AquariumController"));
      JsonObject safety = top.createNestedObject(F("safety"));
      safety[F("minTransitionTime")] = minTransitionTime;
      safety[F("maxBrightness")] = maxBrightness;
    }

    bool readFromConfig(JsonObject& root) {
      JsonObject top = root[F("AquariumController")];

      if (top.isNull()) {
        DEBUG_PRINTLN(F("Aquarium Controller: No config found, using defaults"));
        return false;
      }

      bool configComplete = true;

      JsonObject safety = top[F("safety")];
      if (!safety.isNull()) {
        // Accept both legacy (seconds) and new (deciseconds) config
        if (safety.containsKey(F("minTransitionTime"))) {
          uint32_t val = safety[F("minTransitionTime")];
          // If value is less than 50, assume legacy seconds, convert to deciseconds
          if (val < 50) {
            minTransitionTime = val * 10;
          } else {
            minTransitionTime = val;
          }
        } else {
          minTransitionTime = DEFAULT_MIN_TRANSITION;
        }
        configComplete &= getJsonValue(safety[F("maxBrightness")], maxBrightness, DEFAULT_MAX_BRIGHTNESS);
      }

      return configComplete;
    }

    uint16_t getId() {
      return USERMOD_ID_AQUARIUM_CONTROLLER;
    }
};
