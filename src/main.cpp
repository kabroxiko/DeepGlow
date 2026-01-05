
// ...existing code...
#include <Arduino.h>
// ...existing code...
#ifdef DEBUG_SERIAL
void debugPrintln(const char* msg) { Serial.println(msg); }
void debugPrintln(const String& msg) { Serial.println(msg); }
void debugPrintln(int val) { Serial.println(val); }
void debugPrint(const char* msg) { Serial.print(msg); }
void debugPrint(const String& msg) { Serial.print(msg); }
void debugPrint(int val) { Serial.print(val); }
#else
void debugPrintln(const char*) {}
void debugPrintln(const String&) {}
void debugPrintln(int) {}
void debugPrint(const char*) {}
void debugPrint(const String&) {}
void debugPrint(int) {}
#endif
/*
 * Standalone Aquarium LED Controller
 * ESP32/ESP8266 Fish-Safe LED Controller with Scheduling
 * 
 * Features:
 * - 6 Custom aquarium effects
 * - Fish-safe transitions and brightness limits
 * - NTP time sync with sunrise/sunset calculation
 * - Advanced scheduling system with boot recovery
 * - Modern web interface with WebSocket updates
 * - Preset management
 */

#include <Arduino.h>
#include <FastLED.h>
#ifdef ESP8266
    #include <ESP8266WiFi.h>
    #include <LittleFS.h>
    #define FILESYSTEM LittleFS
#else
    #include <WiFi.h>
    #include <SPIFFS.h>
    #define FILESYSTEM SPIFFS
#endif

#include "config.h"
#include "effects.h"
#include "scheduler.h"
#include "transition.h"
#include "webserver.h"
#include "captive_portal.h"

// Global objects
Configuration config;
Scheduler scheduler(&config);
TransitionEngine transition;
WebServerManager webServer(&config, &scheduler);

// LED array
CRGB* leds = nullptr;
AquariumEffects* effects = nullptr;

// Timing
uint32_t lastStateSave = 0;
uint32_t lastUpdate = 0;

// Function declarations
void setupWiFi();
void setupFilesystem();
void setupLEDs();
void applyPreset(uint8_t presetId);
void setPower(bool power);
void setBrightness(uint8_t brightness);
void setEffect(EffectMode effect, const EffectParams& params);
void updateLEDs();
void checkSchedule();

void setup() {
    #ifdef DEBUG_SERIAL
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println("=================================");
    Serial.println("  Aquarium LED Controller v1.0  ");
    Serial.println("=================================");
    #endif
    
    // Initialize filesystem
    setupFilesystem();
    debugPrintln("[DEBUG] Filesystem initialized");

    // Load configuration
    debugPrintln("[DEBUG] Attempting to load config...");
    if (!config.load()) {
        debugPrintln("[DEBUG] Config load failed, creating default configuration");
        config.setDefaults();
        config.save();
    } else {
        debugPrintln("[DEBUG] Config loaded successfully");
        debugPrint("[DEBUG] Loaded SSID: ");
        debugPrintln(config.network.ssid.c_str());
        debugPrint("[DEBUG] Loaded Password: ");
        debugPrintln(config.network.password.c_str());
    }

    // Load presets
    debugPrintln("[DEBUG] About to load presets");
    if (!config.loadPresets()) {
        debugPrintln("Creating default presets");
        config.setDefaultPresets();
        config.savePresets();
    }
    debugPrintln("[DEBUG] Presets loaded");

    // Load last state
    config.loadState();
    debugPrintln("[DEBUG] State loaded");

    // Initialize LEDs
    setupLEDs();
    debugPrintln("[DEBUG] LEDs initialized");

    // Connect to WiFi
    setupWiFi();
    debugPrintln("[DEBUG] WiFi setup complete");

    // Setup web server callbacks (moved up)
    webServer.onPowerChange(setPower);
    webServer.onBrightnessChange(setBrightness);
    webServer.onEffectChange(setEffect);
    webServer.onPresetApply(applyPreset);
    webServer.onConfigChange([]() {
        debugPrintln("Configuration updated, recalculating sun times");
        scheduler.calculateSunTimes();
    });

    // Start web server (moved up)
    webServer.begin();
    debugPrintln("[DEBUG] Web server started");

    // Initialize scheduler
    scheduler.begin();
    debugPrintln("[DEBUG] Scheduler initialized");

    // Wait for NTP time sync
    debugPrintln("Waiting for time sync...");
    for (int i = 0; i < 30; i++) {
        scheduler.update();
        if (scheduler.isTimeValid()) {
            debugPrintln("Time synchronized!");
            break;
        }
        delay(1000);
    }
    debugPrintln("[DEBUG] NTP sync done");

    // Check if we should apply a scheduled preset on boot
    int8_t bootPreset = scheduler.getBootPreset();
    if (bootPreset >= 0 && bootPreset < MAX_PRESETS) {
        #ifdef DEBUG_SERIAL
        debugPrint("Applying boot preset: ");
        debugPrintln(bootPreset);
        #endif
        applyPreset(bootPreset);
        #ifdef DEBUG_SERIAL
        debugPrintln("[DEBUG] Boot preset applied");
        #endif
    } else {
        // Apply last saved state
        #ifdef DEBUG_SERIAL
        debugPrintln("Restoring last state");
        #endif
        setEffect(config.state.effect, config.state.params);
        setBrightness(config.state.brightness);
        setPower(config.state.power);
        #ifdef DEBUG_SERIAL
        debugPrintln("[DEBUG] Last state restored");
        #endif
    }

    #ifdef DEBUG_SERIAL
    Serial.println();
    Serial.println("System ready!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("=================================");
    #endif
}

void loop() {
    // Update all systems
    scheduler.update();
    webServer.update();
    transition.update();
    
    // Check schedule every loop (scheduler has internal rate limiting)
    checkSchedule();
    
    // Update LEDs at target frame rate
    EVERY_N_MILLISECONDS(1000 / FRAMES_PER_SECOND) {
        updateLEDs();
    }
    
    // Save state periodically
    if (millis() - lastStateSave > STATE_SAVE_INTERVAL) {
        config.saveState();
        lastStateSave = millis();
    }

    // Captive portal DNS handler (if in AP mode)
    if (WiFi.getMode() == WIFI_AP) {
        handleCaptivePortalDns();
    }
}

void setupFilesystem() {
    if (!FILESYSTEM.begin()) {
        Serial.println("Failed to mount filesystem!");
        Serial.println("Formatting...");
        FILESYSTEM.format();
        if (!FILESYSTEM.begin()) {
            Serial.println("Filesystem mount failed after format!");
            return;
        }
    }
    Serial.println("Filesystem mounted");
}

void setupWiFi() {
    Serial.print("Connecting to WiFi");
    debugPrint("[DEBUG] WiFi credentials: SSID=");
    debugPrint(config.network.ssid.c_str());
    debugPrint(", Password=");
    debugPrintln(config.network.password.c_str());
    
    // Set hostname
    #ifdef ESP8266
        WiFi.hostname(config.network.hostname);
    #else
        WiFi.setHostname(config.network.hostname.c_str());
    #endif
    
    // Connect to WiFi
    if (config.network.ssid.length() > 0) {
        debugPrintln("[DEBUG] Attempting WiFi.begin() with loaded credentials...");
        WiFi.begin(config.network.ssid.c_str(), config.network.password.c_str());
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println();
            Serial.print("Connected! IP: ");
            Serial.println(WiFi.localIP());
            debugPrintln("[DEBUG] WiFi connection successful, stopping captive portal");
            stopCaptivePortal();
            return;
        } else {
            debugPrintln("[DEBUG] WiFi connection failed, starting AP mode");
        }
    } else {
        debugPrintln("[DEBUG] No SSID configured, starting AP mode");
    }
    // If connection failed or no credentials, start AP mode
    Serial.println();
    Serial.println("Starting Access Point mode");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(config.network.hostname.c_str(), config.network.apPassword.c_str());
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    // Start captive portal DNS
    startCaptivePortal(WiFi.softAPIP());
}

void setupLEDs() {
    // Allocate LED array
    leds = new CRGB[config.led.count];
    
    // Initialize FastLED
    // Note: For production, support multiple LED types
    FastLED.addLeds<WS2812B, DEFAULT_LED_PIN, GRB>(leds, config.led.count);
    FastLED.setBrightness(config.state.brightness);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);
    
    // Initialize effects engine
    effects = new AquariumEffects(leds, config.led.count);
    
    Serial.print("LEDs initialized: ");
    Serial.print(config.led.count);
    Serial.println(" pixels");
}

void applyPreset(uint8_t presetId) {
    if (presetId >= MAX_PRESETS || !config.presets[presetId].enabled) {
        Serial.println("Invalid preset ID");
        return;
    }
    
    Serial.print("Applying preset: ");
    Serial.println(config.presets[presetId].name);
    
    Preset& preset = config.presets[presetId];
    
    // Start transitions
    uint16_t transTime = config.state.transitionTime;
    
    // Ensure minimum transition time
    if (transTime < config.safety.minTransitionTime) {
        transTime = config.safety.minTransitionTime;
    }
    
    // Apply safety limits to brightness
    uint8_t safeBrightness = min(preset.brightness, config.safety.maxBrightness);
    
    // Start transitions
    transition.startTransition(safeBrightness, transTime);
    transition.startColorTransition(preset.params.color1, preset.params.color2, transTime);
    
    // Update state
    config.state.brightness = safeBrightness;
    config.state.effect = preset.effect;
    config.state.params = preset.params;
    config.state.currentPreset = presetId;
    config.state.power = true;
    config.state.inTransition = true;
    
    webServer.broadcastState();
}

void setPower(bool power) {
    config.state.power = power;
    
    if (!power) {
        // Fade to black
        transition.startTransition(0, config.safety.minTransitionTime);
    } else {
        // Restore brightness
        transition.startTransition(config.state.brightness, config.safety.minTransitionTime);
    }
    
    Serial.print("Power: ");
    Serial.println(power ? "ON" : "OFF");
    
    webServer.broadcastState();
}

void setBrightness(uint8_t brightness) {
    // Apply safety limit
    brightness = min(brightness, config.safety.maxBrightness);
    
    uint16_t transTime = config.state.transitionTime;
    if (transTime < config.safety.minTransitionTime) {
        transTime = config.safety.minTransitionTime;
    }
    
    transition.startTransition(brightness, transTime);
    config.state.brightness = brightness;
    
    Serial.print("Brightness: ");
    Serial.println(brightness);
    
    webServer.broadcastState();
}

void setEffect(EffectMode effect, const EffectParams& params) {
    config.state.effect = effect;
    config.state.params = params;
    
    // Start color transition
    uint16_t transTime = config.state.transitionTime;
    if (transTime < config.safety.minTransitionTime) {
        transTime = config.safety.minTransitionTime;
    }
    
    transition.startColorTransition(params.color1, params.color2, transTime);
    
    Serial.print("Effect changed to: ");
    Serial.println(effect);
    
    webServer.broadcastState();
}

void updateLEDs() {
    // Update transition
    transition.update();
    
    // Get current values from transition engine
    uint8_t currentBrightness = transition.getCurrentBrightness();
    EffectParams currentParams = config.state.params;
    currentParams.color1 = transition.getCurrentColor1();
    currentParams.color2 = transition.getCurrentColor2();
    
    // Apply power state
    if (!config.state.power) {
        currentBrightness = 0;
    }
    
    // Update effect
    effects->update(config.state.effect, currentParams, currentBrightness);
    
    // Apply global brightness
    FastLED.setBrightness(currentBrightness);
    
    // Show LEDs
    FastLED.show();
    
    // Update transition state
    config.state.inTransition = transition.isTransitioning();
}

void checkSchedule() {
    int8_t presetId = scheduler.checkTimers();
    
    if (presetId >= 0 && presetId < MAX_PRESETS) {
        Serial.print("Timer triggered, applying preset: ");
        Serial.println(presetId);
        applyPreset(presetId);
    }
}
