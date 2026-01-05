
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
#include <Adafruit_NeoPixel.h>
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
#include <type_traits>

Adafruit_NeoPixel* strip = nullptr;

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
    uint32_t debugStart = millis();
    debugPrintln("[DEBUG] setupLEDs() started");
    // Detect LED type and allocate/init accordingly
    String type = config.led.type;
    type.toUpperCase();
    uint8_t pin = DEFAULT_LED_PIN;
    uint16_t count = config.led.count;
    // Optimize clearing: only clear both protocols if type changes
    static String prevType = "";
    const uint16_t MAX_LED_SAFE = 256; // adjust as needed for your hardware
    uint32_t clearStart = millis();
    if (prevType.length() > 0 && prevType != type) {
        debugPrintln("[DEBUG] LED type changed, clearing both protocols");
        Adafruit_NeoPixel clearGRB(MAX_LED_SAFE, pin, NEO_GRB + NEO_KHZ800);
        clearGRB.begin();
        for (uint16_t i = 0; i < MAX_LED_SAFE; i++) clearGRB.setPixelColor(i, 0);
        clearGRB.show();
        Adafruit_NeoPixel clearGRBW(MAX_LED_SAFE, pin, NEO_GRBW + NEO_KHZ800);
        clearGRBW.begin();
        for (uint16_t i = 0; i < MAX_LED_SAFE; i++) clearGRBW.setPixelColor(i, 0);
        clearGRBW.show();
    } else {
        debugPrintln("[DEBUG] LED type unchanged, clearing current protocol");
        uint32_t proto = (type.indexOf("SK6812") >= 0) ? (NEO_GRBW + NEO_KHZ800) : (NEO_GRB + NEO_KHZ800);
        Adafruit_NeoPixel clearStrip(MAX_LED_SAFE, pin, proto);
        clearStrip.begin();
        for (uint16_t i = 0; i < MAX_LED_SAFE; i++) clearStrip.setPixelColor(i, 0);
        clearStrip.show();
    }
    if (strip) {
        delete strip;
    }
    uint32_t clearEnd = millis();
    debugPrint("[DEBUG] LED clearing took: ");
    debugPrintln((int)(clearEnd - clearStart));
    debugPrintln(" ms");
        uint32_t debugEnd = millis();
        debugPrint("[DEBUG] setupLEDs() total time: ");
        debugPrintln((int)(debugEnd - debugStart));
        debugPrintln(" ms");
    prevType = type;
    // Now create the actual strip
    if (type.indexOf("SK6812") >= 0) {
        // SK6812 RGBW
        strip = new Adafruit_NeoPixel(count, pin, NEO_GRBW + NEO_KHZ800);
        Serial.print("LEDs initialized (SK6812 RGBW): ");
    } else {
        // Default: WS2812B or compatible RGB
        strip = new Adafruit_NeoPixel(count, pin, NEO_GRB + NEO_KHZ800);
        Serial.print("LEDs initialized (WS2812B RGB): ");
    }
    strip->begin();
    strip->show();
    Serial.print(count);
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

    // For demonstration, fill all LEDs with color1 (expand as needed for effects)
    uint8_t r = (currentParams.color1 >> 16) & 0xFF;
    uint8_t g = (currentParams.color1 >> 8) & 0xFF;
    uint8_t b = currentParams.color1 & 0xFF;
    uint8_t w = 0; // You can add logic to use white channel if desired
    for (uint16_t i = 0; i < strip->numPixels(); i++) {
        strip->setPixelColor(i, strip->Color(r, g, b, w));
    }
    strip->setBrightness(currentBrightness);
    strip->show();
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
