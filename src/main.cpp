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

#include "network.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <string.h>
#include "bus_manager.h"
#include "config.h"
#include "debug.h"
#include "effects.h"
#include "ota.h"
#include "presets.h"
#include "scheduler.h"
#include "state.h"
#include "transition.h"
#include "webserver.h"

#include "display.h"
#include "inc/version.inc"
#include "inc/version_def.inc"

// Global BusManager instance
BusManager busManager;
WebServerManager *webServerPtr = nullptr;

// Track last configuration for change detection
Configuration lastConfiguration;

// Global objects
Configuration config;
Scheduler scheduler(&config);
TransitionEngine transition;
WebServerManager webServer(&config, &scheduler);

// Use void* for runtime type switching
void *strip = nullptr;

// Timing
uint32_t lastStateSave = 0;
uint32_t lastUpdate = 0;
bool g_bootComplete = false;

// Track last timers for schedule update
std::vector<Timer> lastTimers;

// Track last scheduled preset applied by timer
int8_t lastScheduledPreset = -1;

extern TransitionEngine transition;
static bool apFallbackTriggered = false;

// Function declarations
void setupLEDs();
void addBusToManager();
void checkSchedule();
void checkAndApplyScheduleAfterBoot();
extern "C" void main_task(void *pvParameters);

// ESP-IDF: Replace setup() with app_main()
#if defined(ESP_PLATFORM) && !defined(ARDUINO)
extern "C" void app_main() {
  // app_main has a small stack — do nothing heavy here.
  // Just delay for USB enumeration, then hand off to main_task.
  vTaskDelay(pdMS_TO_TICKS(3000));
  xTaskCreate(main_task, "main_task", 32768, NULL, 5, NULL);
}
#else
// Arduino: setup() / loop() entry points
void setup() {
  Serial.begin(115200);
  delay(200); // brief settle so USB CDC is ready
  esp_log_level_set("*", ESP_LOG_VERBOSE); // enable ESP_LOGI/LOGD for all tags
  Serial.println("[setup] starting main_task");
  // Spin up main_task on a large stack; FreeRTOS is always active on Arduino ESP32
  xTaskCreate(main_task, "main_task", 32768, NULL, 5, NULL);
}
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000)); // yield; real work happens in main_task
}
#endif

void main_task(void *pvParameters) {
// On Arduino ESP_LOGI routes via esp-idf log buffers which may not appear in
// the Arduino serial monitor — emit to Serial directly as well.
#ifdef ARDUINO
  #define STEP(msg) do { ESP_LOGI("main", msg); Serial.println("[main_task] " msg); } while(0)
#else
  #define STEP(msg) ESP_LOGI("main", msg)
#endif

  STEP("entered");

#if defined(ESP_PLATFORM) && !defined(ARDUINO)
  esp_log_level_set("main", ESP_LOG_INFO);
  ESP_LOGI("main", "Aquarium LED Controller starting... stack=%u", (unsigned)uxTaskGetStackHighWaterMark(NULL));

  // NVS must be initialized before WiFi
  esp_err_t nvs_err = nvs_flash_init();
  if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(nvs_err);
#else
  // Arduino already initializes NVS before setup() — skip nvs_flash_init
  STEP("skipped nvs_flash_init (Arduino handles it)");
#endif

  STEP("step: webServerPtr");
  webServerPtr = &webServer;
  STEP("step: config.load");

  // Load configuration
  if (!config.load()) {
    config.setDefaults();
    config.save();
  }
  lastConfiguration = config;
  ESP_LOGI("main", "config: hostname=%s ssid=%s pin=%d count=%d type=%s maxBrightness=%d%% minTransition=%dms",
           config.network.hostname.c_str(), config.network.ssid.c_str(),
           config.led.pin, config.led.count, config.led.type.c_str(),
           hexToPercent(config.safety.maxBrightness), config.safety.minTransitionTime);
#ifdef ARDUINO
  Serial.printf("[main_task] config: hostname=%s ssid=%s pin=%d count=%d type=%s\n",
                config.network.hostname.c_str(), config.network.ssid.c_str(),
                config.led.pin, config.led.count, config.led.type.c_str());
#endif
  STEP("step: presets");

  // Load presets
  if (!loadPresets(config.presets)) {
    debugPrintln("Failed to load presets");
    savePresets(config.presets);
  }
  ESP_LOGI("main", "step: LEDs pin=%d count=%d type=%s order=%s",
           config.led.pin, config.led.count,
           config.led.type.c_str(), config.led.colorOrder.c_str());
#ifdef ARDUINO
  Serial.printf("[main_task] step: LEDs pin=%d count=%d\n", config.led.pin, config.led.count);
#endif

  // Guard: skip LED init if config is invalid
  if (config.led.count > 0 && config.led.count <= 512 && config.led.pin < 22) {
    setupLEDs();
    updatePixelCount();
    busManager.turnOffLEDs();
  } else {
    ESP_LOGW("main", "Skipping LED init: invalid config (count=%d pin=%d)", config.led.count, config.led.pin);
  }

  // Initialize relay pin from config
  STEP("step: relay gpio");
  gpio_set_direction((gpio_num_t)config.led.relayPin, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)config.led.relayPin, config.led.relayActiveHigh ? 0 : 1);

  // Initialize transition engine brightness to default
  STEP("step: transition");
  transition.forceCurrentBrightness(state.brightness);
  STEP("step: network");

  // Initialize display
#ifdef DISPLAY_ENABLED
  setup_display();
#endif

  // Connect to WiFi
  STEP("step: networkSetup");
  networkSetup(config);
  vTaskDelay(pdMS_TO_TICKS(500));
  STEP("step: webserver setup");

  // Setup web server callbacks
  webServer.onPowerChange(setPower);
  webServer.onBrightnessChange(setBrightness);
  webServer.onEffectChange(setEffect);
  webServer.onPresetApply([](uint8_t presetId) {
    applyPreset(presetId, transition._targetState.brightness);
  });
  webServer.onConfigChange([]() {
    gpio_set_direction((gpio_num_t)config.led.relayPin, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)config.led.relayPin,
           state.power ? (config.led.relayActiveHigh ? 1 : 0)
                 : (config.led.relayActiveHigh ? 0 : 1));
    bool locationChanged =
        config.time.latitude != lastConfiguration.time.latitude ||
        config.time.longitude != lastConfiguration.time.longitude;
    if (locationChanged) {
      scheduler.calculateSunTimes();
      lastConfiguration.time.latitude = config.time.latitude;
      lastConfiguration.time.longitude = config.time.longitude;
    }
    bool timersChanged = config.timers != lastConfiguration.timers;
    if (timersChanged) {
      scheduler.begin();
      lastConfiguration.timers = config.timers;
    }
    bool ledChanged = config.led.pin != lastConfiguration.led.pin ||
                      config.led.count != lastConfiguration.led.count ||
                      config.led.type != lastConfiguration.led.type ||
                      config.led.colorOrder != lastConfiguration.led.colorOrder;
    if (ledChanged) {
      setupLEDs();
      updatePixelCount();
      lastConfiguration.led.pin = config.led.pin;
      lastConfiguration.led.count = config.led.count;
      lastConfiguration.led.type = config.led.type;
      lastConfiguration.led.colorOrder = config.led.colorOrder;
    }
    if (ledChanged) {
      transition._previousState = transition._currentState;
      transition = TransitionEngine();
      transition.startTransition(transition._previousState, 0);
      updateLEDs();
      setEffect(state.effect, state.params);
      setBrightness(state.brightness);
      setPower(state.power);
    }

    // --- Apply transition if maxBrightness changed ---
    if (config.safety.maxBrightness != lastConfiguration.safety.maxBrightness) {
      uint8_t targetBrightness = state.brightness;
      if (state.brightness > config.safety.maxBrightness) {
        targetBrightness = config.safety.maxBrightness;
      }
      setBrightness(targetBrightness);
      lastConfiguration.safety.maxBrightness = config.safety.maxBrightness;
    }
  });

  webServer.begin();
  STEP("step: webserver.begin done");
  scheduler.begin();
  STEP("step: scheduler.begin done");
  setupArduinoOTA(config.network.hostname.c_str());

  // Only wait for time sync if connected to WiFi (STA mode)
  bool wifi_sta_connected = networkIsStaConnected();
  if (wifi_sta_connected) {
    ESP_LOGI("main", "Waiting for time sync...");
    for (int i = 0; i < 30; i++) {
      scheduler.update();
      if (scheduler.isTimeValid()) {
        ESP_LOGI("main", "Time synchronized!");
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  } else {
    ESP_LOGI("main", "Skipping time sync (AP mode)");
  }

  ESP_LOGI("main", "System ready!");
  ESP_LOGI("main", "IP Address: %s", getCurrentIpString(config).c_str());
  ESP_LOGI("main", "=================================");

  transition.forceCurrentBrightness(state.brightness);
  setEffect(state.effect, state.params);
  setBrightness(state.brightness);
  setPower(state.power);

  // Restore last state (preset + brightness + power) saved before previous reboot
  {
    uint8_t lastPresetId = 0, lastBrightness = 0;
    bool lastPower = false;
    if (config.loadLastState(lastPresetId, lastBrightness, lastPower)) {
      ESP_LOGI("main", "Restoring last state: preset=%d brightness=%d power=%s",
               lastPresetId, lastBrightness, lastPower ? "on" : "off");
#ifdef ARDUINO
      Serial.printf("[main_task] restoring last state: preset=%d bri=%d power=%s\n",
                    lastPresetId, lastBrightness, lastPower ? "on" : "off");
#endif
      if (lastPower && lastBrightness > 0) {
        applyPreset(lastPresetId, lastBrightness);
      }
    } else {
      ESP_LOGI("main", "No saved state found, starting with defaults");
    }
  }
  g_bootComplete = true; // Now safe to persist state changes

  // --- Main loop ---
  int lastCheckedMinute = -1;
  uint64_t lastFrame = 0;
  std::string lastPreset;
  bool lastPower = false;
  uint8_t lastBrightness = 0;
  std::string lastIp;
  while (true) {
    if (otaInProgress) {
      handleArduinoOTA();
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    if (scheduler.isTimeValid()) {
      checkAndApplyScheduleAfterBoot();
      int currentMinute = scheduler.getCurrentMinute();
      if (currentMinute != lastCheckedMinute) {
        checkSchedule();
        lastCheckedMinute = currentMinute;
      }
    }
    handleArduinoOTA();
    scheduler.update();
    webServer.update();
    transition.update();
    networkLoop(config);
    uint64_t now = esp_timer_get_time() / 1000;
    if (now - lastFrame >= (1000 / FRAMES_PER_SECOND)) {
      lastFrame = now;
      updateLEDs();
      std::string presetName = "-";
      if (state.preset < config.getPresetCount()) {
        presetName = config.presets[state.preset].name;
      }
      std::string ipStr = getCurrentIpString(config);
      if (presetName != lastPreset || state.power != lastPower ||
          state.brightness != lastBrightness || ipStr != lastIp) {
        #ifdef DISPLAY_ENABLED
        display_status(presetName.c_str(), state.power, ipStr.c_str());
        #endif
        lastPreset = presetName;
        lastPower = state.power;
        lastBrightness = state.brightness;
        lastIp = ipStr;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to yield
  }
}

void setupLEDs() {
  busManager.setupStrip(config.led.type, config.led.colorOrder, config.led.pin,
                        config.led.count);
}

void handleScheduledPreset(int8_t presetId, int currentMinutes) {
  const Timer *activeTimer = scheduler.getActiveTimer();
  if (activeTimer && activeTimer->presetId == presetId &&
      presetId != lastScheduledPreset) {
    uint8_t brightness = activeTimer->brightness;
    // If this is the first schedule application after boot, use powerOn
    // transition time
    static bool firstScheduleApplied = false;
    uint32_t transitionTime = firstScheduleApplied
                                  ? config.transitionTimes.schedule
                                  : config.transitionTimes.powerOn;
    webServer.applyTransitionTimeLimit(transitionTime);
    state.transitionTime = transitionTime;
    applyPreset(presetId, brightness);
    firstScheduleApplied = true;
    lastScheduledPreset = presetId;
  }
}

void checkSchedule() {
  const Timer *activeTimer = scheduler.getActiveTimer();
  if (activeTimer) {
    handleScheduledPreset(activeTimer->presetId,
                          scheduler.getTimerMinutes(*activeTimer));
  }
}

// Apply the correct schedule as soon as time becomes valid after boot (only
// once)

void checkAndApplyScheduleAfterBoot() {
  static bool scheduleApplied = false;
  if (!scheduleApplied) {
    if (scheduler.isTimeValid()) {
      checkSchedule();
      scheduleApplied = true;
    }
  }
}
