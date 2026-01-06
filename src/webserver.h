#ifndef HTTP_GET
#define HTTP_GET     0b00000001
#define HTTP_POST    0b00000010
#define HTTP_DELETE  0b00000100
#define HTTP_PUT     0b00001000
#define HTTP_PATCH   0b00010000
#define HTTP_HEAD    0b00100000
#define HTTP_OPTIONS 0b01000000
#define HTTP_ANY     0b01111111
#endif
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>
#ifdef ESP8266
    #include <ESP8266WiFi.h>
    #include <ESPAsyncTCP.h>
#else
    #include <WiFi.h>
    #include <AsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "scheduler.h"

class WebServerManager {
public:
    WebServerManager(Configuration* config, Scheduler* scheduler);
    
    void begin();
    void update();
    void broadcastState();
    
    // Callbacks for control actions
    void onPowerChange(void (*callback)(bool));
    void onBrightnessChange(void (*callback)(uint8_t));
    void onEffectChange(void (*callback)(EffectMode, const EffectParams&));
    void onPresetApply(void (*callback)(uint8_t));
    void onConfigChange(void (*callback)());
    
private:
    Configuration* _config;
    Scheduler* _scheduler;
    AsyncWebServer* _server;
    AsyncWebSocket* _ws;
    
    uint32_t _lastBroadcast = 0;
    
    // Callbacks
    void (*_powerCallback)(bool) = nullptr;
    void (*_brightnessCallback)(uint8_t) = nullptr;
    void (*_effectCallback)(EffectMode, const EffectParams&) = nullptr;
    void (*_presetCallback)(uint8_t) = nullptr;
    void (*_configCallback)() = nullptr;
    
    // Setup handlers
    void setupRoutes();
    void setupWebSocket();
    
    // REST API handlers
    void handleGetState(AsyncWebServerRequest* request);
    void handleSetState(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetPresets(AsyncWebServerRequest* request);
    void handleSetPreset(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetConfig(AsyncWebServerRequest* request);
    void handleSetConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetTimers(AsyncWebServerRequest* request);
    void handleSetTimer(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    
    // Helper functions
    String getStateJSON();
    String getPresetsJSON();
    String getConfigJSON();
    String getTimersJSON();
    bool applySafetyLimits(uint8_t& brightness, uint32_t& transitionTime);
};

#endif
