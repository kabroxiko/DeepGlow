#include "presets.h"
#include "inc/presets_json.inc"
#include <ArduinoJson.h>
#include "esp_log.h"

static const char *TAG = "presets";

#ifdef ARDUINO
#include <LittleFS.h>
#define PRESET_FILE "/presets.json"

void resetPresetsFile() {
  if (LittleFS.exists(PRESET_FILE))
    LittleFS.remove(PRESET_FILE);
}

bool loadPresets(std::vector<Preset> &presets) {
  size_t capacity = 8192;
  DynamicJsonDocument doc(capacity);

  bool loaded = false;
  File file = LittleFS.open(PRESET_FILE, "r");
  if (file) {
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (!err && doc.containsKey("presets")) {
      ESP_LOGI(TAG, "loaded from file");
      loaded = true;
    }
  }
  if (!loaded) {
    ESP_LOGI(TAG, "loading from embedded asset");
    DeserializationError err = deserializeJson(doc, web_presets_json, web_presets_json_len);
    if (!err && doc.containsKey("presets")) loaded = true;
  }
  if (!loaded) return false;

  JsonArray presetsArray = doc["presets"];
  presets.clear();
  for (size_t i = 0; i < presetsArray.size(); i++) {
    JsonObject presetObj = presetsArray[i];
    Preset p;
    p.id      = presetObj["id"] | i;
    p.name    = presetObj["name"] | "";
    p.effect  = presetObj["effect"] | 0;
    p.enabled = presetObj["enabled"] | true;
    if (presetObj.containsKey("params")) {
      JsonObject paramsObj = presetObj["params"];
      p.params.speed     = paramsObj["speed"].isNull()     ? percentToHex(100) : percentToHex((uint8_t)paramsObj["speed"]);
      p.params.intensity = paramsObj["intensity"].isNull() ? percentToHex(50)  : percentToHex((uint8_t)paramsObj["intensity"]);
      p.params.colors.clear();
      if (paramsObj.containsKey("colors")) {
        for (JsonVariant v : paramsObj["colors"].as<JsonArray>())
          if (v.is<const char *>()) p.params.colors.push_back(std::string(v.as<const char *>()));
      }
    }
    presets.push_back(p);
  }
  ESP_LOGI(TAG, "loaded %d presets", (int)presets.size());
  return true;
}

bool savePresets(const std::vector<Preset> &presets) {
  size_t capacity = 8192;
  DynamicJsonDocument doc(capacity);
  JsonArray presetsArray = doc.createNestedArray("presets");
  for (size_t i = 0; i < presets.size(); i++) {
    if (presets[i].name.length() == 0 && i > 0) continue;
    JsonObject presetObj = presetsArray.createNestedObject();
    presetObj["name"]    = presets[i].name;
    presetObj["effect"]  = presets[i].effect;
    presetObj["enabled"] = presets[i].enabled;
    JsonObject paramsObj = presetObj.createNestedObject("params");
    paramsObj["speed"]     = hexToPercent(presets[i].params.speed);
    paramsObj["intensity"] = hexToPercent(presets[i].params.intensity);
    JsonArray colorsArr = paramsObj.createNestedArray("colors");
    for (const auto &c : presets[i].params.colors) colorsArr.add(c);
  }
  File file = LittleFS.open(PRESET_FILE, "w");
  if (!file) { ESP_LOGE(TAG, "savePresets: cannot open %s for write", PRESET_FILE); return false; }
  size_t written = serializeJson(doc, file);
  file.flush();
  file.close();
  return written > 0;
}

#else
// ─── ESP-IDF path (POSIX via VFS) ────────────────────────────────────────────
#include "esp_littlefs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/stat.h>
#include <stdio.h>
#include <string>

#define PRESET_MOUNT_POINT "/data"
#define PRESET_FILE        "/presets.json"

static bool s_presets_fs_mounted = false;
static bool ensureFilesystemMounted() {
  if (s_presets_fs_mounted) return true;
  if (esp_littlefs_mounted("spiffs")) { s_presets_fs_mounted = true; return true; }
  esp_vfs_littlefs_conf_t conf = {};
  conf.base_path          = PRESET_MOUNT_POINT;
  conf.partition_label    = "spiffs";
  conf.format_if_mount_failed = true;
  conf.dont_mount         = false;
  if (esp_vfs_littlefs_register(&conf) != ESP_OK) {
    ESP_LOGE(TAG, "LittleFS mount failed");
    return false;
  }
  s_presets_fs_mounted = true;
  return true;
}

void resetPresetsFile() {
  if (!ensureFilesystemMounted()) return;
  char path[64];
  snprintf(path, sizeof(path), "%s%s", PRESET_MOUNT_POINT, PRESET_FILE);
  if (access(path, F_OK) == 0) remove(path);
}

bool loadPresets(std::vector<Preset> &presets) {
  size_t capacity = 8192;
  DynamicJsonDocument doc(capacity);

  bool loaded = false;
  char path[64];
  snprintf(path, sizeof(path), "%s%s", PRESET_MOUNT_POINT, PRESET_FILE);
  FILE *file = ensureFilesystemMounted() ? fopen(path, "r") : nullptr;
  if (file) {
    fseek(file, 0, SEEK_END); long sz = ftell(file); fseek(file, 0, SEEK_SET);
    std::string content(sz, '\0');
    fread(&content[0], 1, sz, file);
    fclose(file);
    DeserializationError err = deserializeJson(doc, content.c_str(), sz);
    if (!err && doc.containsKey("presets")) {
      ESP_LOGI(TAG, "loaded from file");
      loaded = true;
    }
  }
  if (!loaded) {
    ESP_LOGI(TAG, "loading from embedded asset");
    DeserializationError err = deserializeJson(doc, web_presets_json, web_presets_json_len);
    if (!err && doc.containsKey("presets")) loaded = true;
  }
  if (!loaded) return false;

  JsonArray presetsArray = doc["presets"];
  presets.clear();
  for (size_t i = 0; i < presetsArray.size(); i++) {
    JsonObject presetObj = presetsArray[i];
    Preset p;
    p.id      = presetObj["id"] | i;
    p.name    = presetObj["name"] | "";
    p.effect  = presetObj["effect"] | 0;
    p.enabled = presetObj["enabled"] | true;
    if (presetObj.containsKey("params")) {
      JsonObject paramsObj = presetObj["params"];
      p.params.speed     = paramsObj["speed"].isNull()     ? percentToHex(100) : percentToHex((uint8_t)paramsObj["speed"]);
      p.params.intensity = paramsObj["intensity"].isNull() ? percentToHex(50)  : percentToHex((uint8_t)paramsObj["intensity"]);
      p.params.colors.clear();
      if (paramsObj.containsKey("colors")) {
        for (JsonVariant v : paramsObj["colors"].as<JsonArray>())
          if (v.is<const char *>()) p.params.colors.push_back(std::string(v.as<const char *>()));
      }
    }
    presets.push_back(p);
  }
  ESP_LOGI(TAG, "loaded %d presets", (int)presets.size());
  return true;
}

bool savePresets(const std::vector<Preset> &presets) {
  size_t capacity = 8192;
  DynamicJsonDocument doc(capacity);
  JsonArray presetsArray = doc.createNestedArray("presets");
  for (size_t i = 0; i < presets.size(); i++) {
    if (presets[i].name.length() == 0 && i > 0) continue;
    JsonObject presetObj = presetsArray.createNestedObject();
    presetObj["name"]    = presets[i].name;
    presetObj["effect"]  = presets[i].effect;
    presetObj["enabled"] = presets[i].enabled;
    JsonObject paramsObj = presetObj.createNestedObject("params");
    paramsObj["speed"]     = hexToPercent(presets[i].params.speed);
    paramsObj["intensity"] = hexToPercent(presets[i].params.intensity);
    JsonArray colorsArr = paramsObj.createNestedArray("colors");
    for (const auto &c : presets[i].params.colors) colorsArr.add(c);
  }
  if (!ensureFilesystemMounted()) return false;
  char path[64];
  snprintf(path, sizeof(path), "%s%s", PRESET_MOUNT_POINT, PRESET_FILE);
  FILE *fp = fopen(path, "w");
  if (!fp) { ESP_LOGE(TAG, "savePresets: cannot open %s", path); return false; }
  std::string out;
  size_t written = serializeJson(doc, out);
  fwrite(out.c_str(), 1, out.length(), fp);
  fflush(fp); fclose(fp);
  vTaskDelay(pdMS_TO_TICKS(10));
  return written > 0;
}
#endif
