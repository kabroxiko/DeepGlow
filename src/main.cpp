
#include <Arduino.h>
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
#include <WS2812FX.h>
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
// #include "effects.h" // Custom effects disabled, using WS2812FX native effects only
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
#include <type_traits>

// WS2812FX LED object
WS2812FX* strip = nullptr;

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
void setEffect(uint8_t effect, const EffectParams& params);
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
        debugPrintln("Configuration updated, recalculating sun times and reinitializing LEDs");
        scheduler.calculateSunTimes();
        // Save current transition state
        uint8_t prevBrightness = transition.getCurrentBrightness();
        uint32_t prevColor1 = transition.getCurrentColor1();
        uint32_t prevColor2 = transition.getCurrentColor2();
        setupLEDs();
        transition = TransitionEngine();
        // Restore previous state to new transition engine
        transition.startTransition(prevBrightness, 0);
        transition.startColorTransition(prevColor1, prevColor2, 0);
        // If you have custom effect objects, re-create them here as well
        updateLEDs();
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
        // Ensure transition starts from the actual brightness, not 0
        transition.forceCurrentBrightness(config.state.brightness);
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
    // Top of loop: check for unexpected resets
    static bool firstLoop = true;
    if (firstLoop) {
        debugPrintln("[DEBUG] loop() entered (device running)");
        Serial.println("[DEBUG] loop() entered (device running)");
        firstLoop = false;
    }
    // Update all systems
    scheduler.update();
    webServer.update();
    transition.update();
    
    // Check schedule every loop (scheduler has internal rate limiting)
    checkSchedule();
    
    // Update LEDs at target frame rate (manual timing)
    static uint32_t lastFrame = 0;
    uint32_t now = millis();
    if (now - lastFrame >= (1000 / FRAMES_PER_SECOND)) {
        lastFrame = now;
        updateLEDs();
    }
    
    // Save state periodically
    // State saving to file removed; only update lastStateSave timestamp
    if (millis() - lastStateSave > STATE_SAVE_INTERVAL) {
        lastStateSave = millis();
        transition.forceCurrentBrightness(config.state.brightness);
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
    // Set hostname
    #ifdef ESP8266
        WiFi.hostname(config.network.hostname);
    #else
        WiFi.setHostname(config.network.hostname.c_str());
    #endif
    
    // Connect to WiFi
    if (config.network.ssid.length() > 0) {
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
            stopCaptivePortal();
            return;
        }
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
    if (strip) {
        delete strip;
        strip = nullptr;
    }
    uint8_t pin = config.led.pin;
    uint16_t count = config.led.count;
    String type = config.led.type;
    String order = config.led.colorOrder;
    type.toUpperCase();
    order.toUpperCase();
    uint8_t wsType = NEO_GRB + NEO_KHZ800; // default
    if (type.indexOf("SK6812") >= 0) {
        if (order == "RGBW") wsType = NEO_RGBW + NEO_KHZ800;
        else if (order == "GRBW") wsType = NEO_GRBW + NEO_KHZ800;
        else wsType = NEO_GRBW + NEO_KHZ800; // default for SK6812
    } else if (type.indexOf("WS2812") >= 0) {
        if (order == "RGB") wsType = NEO_RGB + NEO_KHZ800;
        else if (order == "GRB") wsType = NEO_GRB + NEO_KHZ800;
        else wsType = NEO_GRB + NEO_KHZ800; // default for WS2812
    } else if (type.indexOf("APA106") >= 0) {
        wsType = NEO_RGB + NEO_KHZ800;
    }
    strip = new WS2812FX(count, pin, wsType);
    strip->init();
    strip->setBrightness(config.state.brightness);
    strip->start();
    Serial.print("LEDs initialized (WS2812FX): ");
    Serial.print(count);
    Serial.print(" type: ");
    Serial.print(type);
    Serial.print(" order: ");
    Serial.println(order);
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
    uint32_t transTime = config.state.transitionTime;
    // Ensure minimum transition time
    if (transTime < config.safety.minTransitionTime) {
        transTime = config.safety.minTransitionTime;
    }
    // Apply safety limits to brightness
    uint8_t safeBrightness = min(preset.brightness, config.safety.maxBrightness);
    // Start transitions
    transition.startTransition(safeBrightness, transTime);
    // No color transition, just set effect and color directly
    setEffect(preset.effect, preset.params);
    config.state.currentPreset = presetId;
    config.state.power = true;
    config.state.inTransition = true;
    webServer.broadcastState();
}

void setPower(bool power) {
    if (config.state.power == power) {
        return;
    }
    config.state.power = power;
    uint8_t targetBrightness = power ? config.state.brightness : 0;
    uint32_t transTime = config.state.transitionTime;
    if (transTime < config.safety.minTransitionTime) {
        transTime = config.safety.minTransitionTime;
    }
    if (power) {
        transition.forceCurrentBrightness(config.state.brightness);
    }
    if (transition.getCurrentBrightness() != targetBrightness || !transition.isTransitioning()) {
        transition.startTransition(targetBrightness, transTime);
    }
    Serial.print("Power: ");
    Serial.println(power ? "ON" : "OFF");
    webServer.broadcastState();
}

void setBrightness(uint8_t brightness) {
    brightness = min(brightness, config.safety.maxBrightness);
    uint32_t transTime = config.state.transitionTime;
    if (transTime < config.safety.minTransitionTime) {
        transTime = config.safety.minTransitionTime;
    }
    uint8_t current = transition.getCurrentBrightness();
    if (brightness != current) {
        if (!transition.isTransitioning()) {
            transition.forceCurrentBrightness(config.state.brightness);
        }
        transition.startTransition(brightness, transTime);
        Serial.print("Brightness: ");
        Serial.println(brightness);
        webServer.broadcastState();
    }
}

void setEffect(uint8_t effect, const EffectParams& params) {
    config.state.effect = effect;
    config.state.params = params;
    // Set WS2812FX effect and color
    if (strip) {
        strip->setMode(effect);
        strip->setColor(params.color1);
    }
    Serial.print("Effect changed to: ");
    Serial.println(effect);
    webServer.broadcastState();
}

void updateLEDs() {
    // Only update brightness and call WS2812FX service
    if (!config.state.power) {
        strip->setBrightness(0);
        strip->service();
        config.state.inTransition = false;
        config.state.brightness = 0;
        return;
    }
    uint8_t currentBrightness = config.state.brightness;
    strip->setBrightness(currentBrightness);
    strip->service();
    config.state.inTransition = false;
    config.state.brightness = currentBrightness;
}

void checkSchedule() {
    int8_t presetId = scheduler.checkTimers();
    
    if (presetId >= 0 && presetId < MAX_PRESETS) {
        Serial.print("Timer triggered, applying preset: ");
        Serial.println(presetId);
        applyPreset(presetId);
    }
}
