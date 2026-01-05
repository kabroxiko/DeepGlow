#ifndef EFFECTS_H
#define EFFECTS_H

#include <Arduino.h>
#include <FastLED.h>
#include "config.h"

class AquariumEffects {
public:
    AquariumEffects(CRGB* leds, uint16_t numLeds);
    
    void update(EffectMode mode, const EffectParams& params, uint8_t brightness);
    void setSolid(uint32_t color);
    void clear();
    
private:
    CRGB* _leds;
    uint16_t _numLeds;
    uint32_t _effectTimer = 0;
    uint8_t _effectPhase = 0;
    
    // Effect implementations
    void aquariumRipple(const EffectParams& params, uint8_t brightness);
    void aquariumGentleWave(const EffectParams& params, uint8_t brightness);
    void aquariumSunrise(const EffectParams& params, uint8_t brightness);
    void aquariumCoralShimmer(const EffectParams& params, uint8_t brightness);
    void aquariumDeepOcean(const EffectParams& params, uint8_t brightness);
    void aquariumMoonlight(const EffectParams& params, uint8_t brightness);
    
    // Helper functions
    CRGB colorFromUint32(uint32_t color);
    CRGB blendColors(CRGB color1, CRGB color2, uint8_t amount);
    uint8_t sine8(uint8_t theta);
    uint8_t cos8(uint8_t theta);
};

#endif
