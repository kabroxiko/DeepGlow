#pragma once

#include <string>

#if defined(ESP_PLATFORM) && !defined(ARDUINO)

#include "esp_http_server.h"
#include "esp_ota_ops.h"

extern volatile bool otaInProgress;
extern volatile bool otaRequested;
extern volatile bool otaAckReceived;

// Remote gz OTA from GitHub releases
bool performGzOtaUpdate(std::string &errorOut);

// Stubs – keep signatures so main.cpp compiles unchanged
void setupArduinoOTA(const char *hostname);
void handleArduinoOTA();

// Local firmware upload handler (esp_http_server POST body handler)
// OTA is read via httpd_req_recv inside this function.
esp_err_t handleOtaUpload(httpd_req_t *req);

// Fetch manifest JSON from GitHub (empty string on failure)
std::string fetchRemoteManifestJson();

// Fetch latest firmware URL for this env (OTA_ENV) (empty string on failure)
std::string getLatestFirmwareUrl(std::string &latestVersion);

class WebServerManager;
extern "C" void otaTask(void *parameter);

#else // Arduino

#include "esp_http_server.h"

extern volatile bool otaInProgress;
extern volatile bool otaRequested;
extern volatile bool otaAckReceived;

bool performGzOtaUpdate(std::string &errorOut);
void setupArduinoOTA(const char *hostname);
void handleArduinoOTA();
std::string fetchRemoteManifestJson();
std::string getLatestFirmwareUrl(std::string &latestVersion);
esp_err_t handleOtaUpload(httpd_req_t *req);
extern "C" void otaTask(void *parameter);

#endif // ESP_PLATFORM && !ARDUINO
