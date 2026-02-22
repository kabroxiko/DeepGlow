#pragma once
#include "config.h"
#if defined(ESP_IDF_VERSION_MAJOR)
#include <ArduinoJson.h>
#endif
#include <vector>

// Preset management API
bool loadPresets(std::vector<Preset> &presets);
bool savePresets(const std::vector<Preset> &presets);
void resetPresetsFile();
