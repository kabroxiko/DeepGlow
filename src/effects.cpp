#include "effects.h"

AquariumEffects::AquariumEffects(CRGB* leds, uint16_t numLeds) {
    _leds = leds;
    _numLeds = numLeds;
}

void AquariumEffects::update(EffectMode mode, const EffectParams& params, uint8_t brightness) {
    switch (mode) {
        case MODE_SOLID:
            setSolid(params.color1);
            break;
        case MODE_AQUARIUM_RIPPLE:
            aquariumRipple(params, brightness);
            break;
        case MODE_AQUARIUM_GENTLE_WAVE:
            aquariumGentleWave(params, brightness);
            break;
        case MODE_AQUARIUM_SUNRISE:
            aquariumSunrise(params, brightness);
            break;
        case MODE_AQUARIUM_CORAL_SHIMMER:
            aquariumCoralShimmer(params, brightness);
            break;
        case MODE_AQUARIUM_DEEP_OCEAN:
            aquariumDeepOcean(params, brightness);
            break;
        case MODE_AQUARIUM_MOONLIGHT:
            aquariumMoonlight(params, brightness);
            break;
        default:
            setSolid(params.color1);
            break;
    }
}

void AquariumEffects::setSolid(uint32_t color) {
    CRGB rgb = colorFromUint32(color);
    fill_solid(_leds, _numLeds, rgb);
}

void AquariumEffects::clear() {
    fill_solid(_leds, _numLeds, CRGB::Black);
}

// Effect 1: Aquarium Ripple - Simulates light ripples through water surface
void AquariumEffects::aquariumRipple(const EffectParams& params, uint8_t brightness) {
    uint8_t speed = map(params.speed, 0, 255, 20, 150);
    uint8_t intensity = params.intensity;
    
    CRGB color1 = colorFromUint32(params.color1);
    CRGB color2 = colorFromUint32(params.color2);
    
    uint32_t time = millis() / (256 - speed);
    
    for (uint16_t i = 0; i < _numLeds; i++) {
        // Two sine waves at different speeds combining
        uint8_t wave1 = sine8(time + (i * 8));
        uint8_t wave2 = sine8(time * 2 + (i * 12));
        uint8_t combined = (wave1 + wave2) / 2;
        
        // Blend between two colors based on wave
        CRGB finalColor = blendColors(color1, color2, combined);
        
        // Apply intensity
        finalColor.nscale8(intensity);
        
        _leds[i] = finalColor;
    }
}

// Effect 2: Gentle Wave - Smooth flowing underwater waves
void AquariumEffects::aquariumGentleWave(const EffectParams& params, uint8_t brightness) {
    uint8_t speed = map(params.speed, 0, 255, 10, 100);
    uint8_t intensity = params.intensity;
    
    CRGB color1 = colorFromUint32(params.color1);
    CRGB color2 = colorFromUint32(params.color2);
    
    uint32_t time = millis() / (300 - speed);
    
    for (uint16_t i = 0; i < _numLeds; i++) {
        // Single smooth sine wave across the strip
        uint8_t wave = sine8(time + (i * 255 / _numLeds));
        
        // Blend colors based on wave position
        CRGB finalColor = blendColors(color1, color2, wave);
        
        // Apply intensity
        finalColor.nscale8(intensity);
        
        _leds[i] = finalColor;
    }
}

// Effect 3: Sunrise Simulation - Multi-phase color transition
void AquariumEffects::aquariumSunrise(const EffectParams& params, uint8_t brightness) {
    uint8_t speed = map(params.speed, 0, 255, 5, 50);
    
    // Auto-progress through phases
    uint32_t time = millis() / (500 - (speed * 2));
    _effectPhase = (time % 256);
    
    CRGB currentColor;
    
    if (_effectPhase < 85) {
        // Phase 1: Orange to Yellow (dawn)
        uint8_t progress = map(_effectPhase, 0, 84, 0, 255);
        currentColor = blendColors(CRGB(255, 100, 0), CRGB(255, 200, 0), progress);
    } else if (_effectPhase < 170) {
        // Phase 2: Yellow to White (morning)
        uint8_t progress = map(_effectPhase, 85, 169, 0, 255);
        currentColor = blendColors(CRGB(255, 200, 0), CRGB(255, 255, 200), progress);
    } else {
        // Phase 3: White to Daylight Blue (day)
        uint8_t progress = map(_effectPhase, 170, 255, 0, 255);
        currentColor = blendColors(CRGB(255, 255, 200), CRGB(135, 206, 235), progress);
    }
    
    // Apply to all LEDs
    fill_solid(_leds, _numLeds, currentColor);
}

// Effect 4: Coral Shimmer - Subtle twinkling effect
void AquariumEffects::aquariumCoralShimmer(const EffectParams& params, uint8_t brightness) {
    uint8_t speed = map(params.speed, 0, 255, 50, 200);
    
    CRGB color1 = colorFromUint32(params.color1);
    
    uint32_t time = millis() / (256 - speed);
    
    for (uint16_t i = 0; i < _numLeds; i++) {
        // Random variation per LED with gentle twinkling
        uint8_t shimmer = sine8(time + (i * 23) + random8());
        
        // Keep in 191-255 range for subtle effect
        shimmer = map(shimmer, 0, 255, 191, 255);
        
        CRGB finalColor = color1;
        finalColor.nscale8(shimmer);
        
        _leds[i] = finalColor;
    }
}

// Effect 5: Deep Ocean - Very slow dark blue pulsing
void AquariumEffects::aquariumDeepOcean(const EffectParams& params, uint8_t brightness) {
    uint8_t speed = map(params.speed, 0, 255, 5, 40);
    uint8_t intensity = map(params.intensity, 0, 255, 0, 80);
    
    // Ultra-slow pulse
    uint32_t time = millis() / 500;
    uint8_t pulse = sine8(time * speed / 10);
    
    // Dark blue color
    CRGB deepBlue = CRGB(0, 0, intensity);
    deepBlue.nscale8(pulse);
    
    fill_solid(_leds, _numLeds, deepBlue);
}

// Effect 6: Moonlight - Ultra-dim blue lighting for night
void AquariumEffects::aquariumMoonlight(const EffectParams& params, uint8_t brightness) {
    uint8_t speed = map(params.speed, 0, 255, 10, 60);
    
    // Very subtle pulse (239-255 range)
    uint32_t time = millis() / 800;
    uint8_t pulse = sine8(time * speed / 10);
    pulse = map(pulse, 0, 255, 239, 255);
    
    // Moonlight color (very dim blue)
    CRGB moonColor = colorFromUint32(params.color1);
    moonColor.nscale8(pulse);
    
    fill_solid(_leds, _numLeds, moonColor);
}

// Helper Functions
CRGB AquariumEffects::colorFromUint32(uint32_t color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    return CRGB(r, g, b);
}

CRGB AquariumEffects::blendColors(CRGB color1, CRGB color2, uint8_t amount) {
    return blend(color1, color2, amount);
}

uint8_t AquariumEffects::sine8(uint8_t theta) {
    return sin8(theta);
}

uint8_t AquariumEffects::cos8(uint8_t theta) {
    return cos8(theta);
}
