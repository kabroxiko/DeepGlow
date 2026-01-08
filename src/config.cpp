#include "config.h"
#include <LittleFS.h>

#define FILESYSTEM LittleFS

// Utility to delete presets file
void Configuration::resetPresetsFile() {
    if (FILESYSTEM.exists(PRESET_FILE)) {
        FILESYSTEM.remove(PRESET_FILE);
    }
}

bool Configuration::loadFromFile(const char* path, JsonDocument& doc) {
    File file = FILESYSTEM.open(path, "r");
    if (!file) {
        return false;
    }
    size_t fileSize = file.size();
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        return false;
    }

    return true;
}

bool Configuration::saveToFile(const char* path, const JsonDocument& doc) {
    File file = FILESYSTEM.open(path, "w");
    if (!file) {
        return false;
    }
    size_t written = serializeJson(doc, file);
    file.close();
    return written > 0;
}

bool Configuration::load() {
    StaticJsonDocument<2048> doc;
    
    if (!loadFromFile(CONFIG_FILE, doc)) {
        setDefaults();
        return false;
    }
    
    // LED Configuration
    if (doc.containsKey("led")) {
        JsonObject ledObj = doc["led"];
        led.pin = ledObj["pin"] | DEFAULT_LED_PIN;
        led.count = ledObj["count"] | DEFAULT_LED_COUNT;
        led.type = ledObj["type"] | DEFAULT_LED_TYPE;
        led.colorOrder = ledObj["colorOrder"] | DEFAULT_COLOR_ORDER;
        led.relayPin = ledObj.containsKey("relayPin") ? (int)ledObj["relayPin"] : DEFAULT_LED_RELAY_PIN;
        led.relayActiveHigh = ledObj.containsKey("relayActiveHigh") ? (bool)ledObj["relayActiveHigh"] : true;
    }
    
    // Safety Configuration
    if (doc.containsKey("safety")) {
        JsonObject safetyObj = doc["safety"];
        safety.minTransitionTime = safetyObj["minTransitionTime"] | DEFAULT_MIN_TRANSITION_TIME;
        // Convert percent (0–100) to hardware value (1–255)
        if (safetyObj.containsKey("maxBrightness")) {
            int percent = safetyObj["maxBrightness"];
            if (percent <= 0) safety.maxBrightness = 1;
            else if (percent >= 100) safety.maxBrightness = 255;
            else safety.maxBrightness = 1 + (254 * percent) / 100;
        } else {
            safety.maxBrightness = DEFAULT_MAX_BRIGHTNESS;
        }
    }
    
    // Network Configuration
    if (doc.containsKey("network")) {
        JsonObject netObj = doc["network"];
        network.hostname = netObj["hostname"] | DEFAULT_HOSTNAME;
        network.apPassword = netObj["apPassword"] | DEFAULT_AP_PASSWORD;
        network.ssid = netObj["ssid"] | "";
        network.password = netObj["password"] | "";
    }
    
    // Time Configuration
    if (doc.containsKey("time")) {
        JsonObject timeObj = doc["time"];
        time.ntpServer = timeObj["ntpServer"] | DEFAULT_NTP_SERVER;
        time.timezone = timeObj["timezone"] | String("Etc/UTC");
        time.latitude = timeObj["latitude"] | 0.0;
        time.longitude = timeObj["longitude"] | 0.0;
        time.gpsValid = timeObj["gpsValid"] | false;
        time.dstEnabled = timeObj["dstEnabled"] | false;
    }
    
    // Timers
    if (doc.containsKey("timers")) {
        JsonArray timersArray = doc["timers"];
        for (size_t i = 0; i < timersArray.size() && i < (MAX_TIMERS + MAX_SUN_TIMERS); i++) {
            JsonObject timerObj = timersArray[i];
            timers[i].enabled = timerObj["enabled"] | false;
            timers[i].type = (TimerType)(timerObj["type"] | 0);
            timers[i].hour = timerObj["hour"] | 0;
            timers[i].minute = timerObj["minute"] | 0;
            timers[i].days = timerObj["days"] | 0b1111111;
            timers[i].offset = timerObj["offset"] | 0;
            timers[i].presetId = timerObj["presetId"] | 0;
        }
    }
    
    return true;
}

bool Configuration::save() {
    StaticJsonDocument<2048> doc;
    
    // LED Configuration
    JsonObject ledObj = doc.createNestedObject("led");
    ledObj["pin"] = led.pin;
    ledObj["count"] = led.count;
    ledObj["type"] = led.type;
    ledObj["colorOrder"] = led.colorOrder;
    ledObj["relayPin"] = led.relayPin;
    ledObj["relayActiveHigh"] = led.relayActiveHigh;
    
    // Safety Configuration
    JsonObject safetyObj = doc.createNestedObject("safety");
    safetyObj["minTransitionTime"] = safety.minTransitionTime;
    // Convert hardware value (1–255) to percent (0–100) for API output
    safetyObj["maxBrightness"] = (int)round(((safety.maxBrightness - 1) / 254.0) * 100);
    
    // Network Configuration
    JsonObject netObj = doc.createNestedObject("network");
    netObj["hostname"] = network.hostname;
    netObj["apPassword"] = network.apPassword;
    netObj["ssid"] = network.ssid;
    netObj["password"] = network.password;
    
    // Time Configuration
    JsonObject timeObj = doc.createNestedObject("time");
    timeObj["ntpServer"] = time.ntpServer;
    timeObj["timezone"] = time.timezone;
    timeObj["latitude"] = time.latitude;
    timeObj["longitude"] = time.longitude;
    timeObj["gpsValid"] = time.gpsValid;
    timeObj["dstEnabled"] = time.dstEnabled;
    
    // Timers
    JsonArray timersArray = doc.createNestedArray("timers");
    for (size_t i = 0; i < (MAX_TIMERS + MAX_SUN_TIMERS); i++) {
        JsonObject timerObj = timersArray.createNestedObject();
        timerObj["enabled"] = timers[i].enabled;
        timerObj["type"] = timers[i].type;
        timerObj["hour"] = timers[i].hour;
        timerObj["minute"] = timers[i].minute;
        timerObj["days"] = timers[i].days;
        timerObj["offset"] = timers[i].offset;
        timerObj["presetId"] = timers[i].presetId;
    }
    
    return saveToFile(CONFIG_FILE, doc);
}

bool Configuration::loadPresets() {
    // Delete presets file before loading (force regeneration)
    resetPresetsFile();

    size_t capacity = 8192;
    DynamicJsonDocument doc(capacity);

    if (!loadFromFile(PRESET_FILE, doc)) {
        setDefaultPresets();
        return false;
    }

    if (!doc.containsKey("presets")) {
        setDefaultPresets();
        return false;
    }

    JsonArray presetsArray = doc["presets"];
    for (size_t i = 0; i < presetsArray.size() && i < MAX_PRESETS; i++) {
        JsonObject presetObj = presetsArray[i];
        presets[i].name = presetObj["name"] | "";
        presets[i].brightness = presetObj["brightness"] | 128;
        presets[i].effect = presetObj["effect"] | 0;
        presets[i].enabled = presetObj["enabled"] | true;

        if (presetObj.containsKey("params")) {
            JsonObject paramsObj = presetObj["params"];
            presets[i].params.speed = paramsObj["speed"] | 128;
            presets[i].params.intensity = paramsObj["intensity"] | 128;
            presets[i].params.color1 = paramsObj["color1"] | 0x0000FF;
            presets[i].params.color2 = paramsObj["color2"] | 0x00FFFF;
        }
    }
    return true;
}

bool Configuration::savePresets() {
    // Use DynamicJsonDocument for heap allocation
    size_t capacity = 8192;
    DynamicJsonDocument doc(capacity);
    JsonArray presetsArray = doc.createNestedArray("presets");

    for (size_t i = 0; i < MAX_PRESETS; i++) {
        if (presets[i].name.length() == 0 && i > 0) continue;

        JsonObject presetObj = presetsArray.createNestedObject();
        presetObj["name"] = presets[i].name;
        presetObj["brightness"] = presets[i].brightness;
        presetObj["effect"] = presets[i].effect;
        presetObj["enabled"] = presets[i].enabled;

        JsonObject paramsObj = presetObj.createNestedObject("params");
        paramsObj["speed"] = presets[i].params.speed;
        paramsObj["intensity"] = presets[i].params.intensity;
        paramsObj["color1"] = presets[i].params.color1;
        paramsObj["color2"] = presets[i].params.color2;
    }

    return saveToFile(PRESET_FILE, doc);
}

// Helper function to map percent (0-100) to hardware brightness (1-255)
uint8_t percentToBrightness(uint8_t percent) {
    if (percent <= 0) return 1;
    if (percent >= 100) return 255;
    return (uint8_t)(1 + ((254 * percent) / 100));
}

void Configuration::setDefaults() {
    led = LEDConfig();
    safety = SafetyConfig();
    network = NetworkConfig();
    time = TimeConfig();
    state = SystemState();
    
    for (int i = 0; i < MAX_TIMERS + MAX_SUN_TIMERS; i++) {
        timers[i] = Timer();
    }
    
    setDefaultPresets();
    savePresets();
}

void Configuration::setDefaultPresets() {
    // Preset 0: Morning Sun (Sunrise)
    presets[0].name = "Morning Sun";
    presets[0].brightness = percentToBrightness(70); // 70%
    presets[0].effect = 15; // FX_MODE_FADE
    presets[0].params.speed = 80;
    presets[0].params.intensity = 200;
    presets[0].params.color1 = 0xFF8800;  // Orange
    presets[0].params.color2 = 0xFFFF00;  // Yellow

    // Preset 1: Daylight
    presets[1].name = "Daylight";
    presets[1].brightness = percentToBrightness(100); // 100%
    presets[1].effect = 0; // FX_MODE_STATIC
    presets[1].params.color1 = 0xFFFFFF;  // White

    // Preset 2: Afternoon Ripple
    presets[2].name = "Afternoon Ripple";
    presets[2].brightness = percentToBrightness(70); // 70%
    presets[2].effect = 12; // FX_MODE_RAINBOW_CYCLE
    presets[2].params.speed = 100;
    presets[2].params.intensity = 150;
    presets[2].params.color1 = 0x0088FF;  // Blue
    presets[2].params.color2 = 0x00FFFF;  // Cyan

    // Preset 3: Gentle Wave
    presets[3].name = "Gentle Wave";
    presets[3].brightness = percentToBrightness(47); // 47%
    presets[3].effect = 3; // FX_MODE_COLOR_WIPE
    presets[3].params.speed = 60;
    presets[3].params.intensity = 100;
    presets[3].params.color1 = 0x00BFFF;  // Light Blue
    presets[3].params.color2 = 0xFFFFFF;  // White

    // Preset 4: Coral Shimmer
    presets[4].name = "Coral Shimmer";
    presets[4].brightness = percentToBrightness(59); // 59%
    presets[4].effect = 21; // FX_MODE_TWINKLE_FADE
    presets[4].params.speed = 120;
    presets[4].params.intensity = 180;
    presets[4].params.color1 = 0xFF4500;  // Coral
    presets[4].params.color2 = 0xFF69B4;  // Pink
    // Add third color for shimmer
    // (If you want to support 3 colors, extend EffectParams and preset saving logic)

    // Preset 5: Deep Ocean
    presets[5].name = "Deep Ocean";
    presets[5].brightness = percentToBrightness(31); // 31%
    presets[5].effect = 37; // FX_MODE_CHASE_BLUE
    presets[5].params.speed = 40;
    presets[5].params.intensity = 60;
    presets[5].params.color1 = 0x000080;  // Navy
    presets[5].params.color2 = 0x0000CD;  // Medium Blue

    // Preset 6: Moonlight
    presets[6].name = "Moonlight";
    presets[6].brightness = percentToBrightness(12); // 12%
    presets[6].effect = 2; // FX_MODE_BREATH
    presets[6].params.speed = 50;
    presets[6].params.intensity = 40;
    presets[6].params.color1 = 0x0A0A20;  // Dark Blue
    
    // Mark remaining presets as disabled
    for (int i = 7; i < MAX_PRESETS; i++) {
        presets[i].name = "";
        presets[i].enabled = false;
    }
}

// Update location from GPS data
void Configuration::updateLocationFromGPS(float lat, float lon, bool valid) {
    time.latitude = lat;
    time.longitude = lon;
    time.gpsValid = valid;
}

// Get timezone offset in seconds (stub, needs library for real implementation)
int Configuration::getTimezoneOffsetSeconds() {
    // TODO: Use a timezone library to convert time.timezone to offset
    // For now, return 0 for UTC
    if (time.timezone == "Etc/UTC") return 0;
    // Example: hardcoded offset for "America/Los_Angeles" (-8 hours)
    if (time.timezone == "America/Los_Angeles") return -8 * 3600;
    // Add more mappings or integrate a library
    return 0;
}
