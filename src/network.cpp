#include "network.h"
#include "inc/wifi_html.inc"
#include "debug.h"
#include "config.h"
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

static bool apFallbackTriggered = false;

DNSServer captiveDnsServer;

void networkSetup(Configuration& config) {
    debugPrintln("[WiFi] networkSetup() called");
    debugPrint("Connecting to WiFi");
#ifdef ESP8266
    WiFi.hostname(config.network.hostname);
#else
    WiFi.setHostname(config.network.hostname.c_str());
#endif
    if (config.network.ssid.length() > 0 && !apFallbackTriggered) {
        debugPrintln("");
        debugPrintln("[WiFi] Calling WiFi.begin");
        const unsigned long wifiTimeout = 10000;
        unsigned long wifiStart = millis();
        bool wrongPassword = false;
        WiFi.begin(config.network.ssid.c_str(), config.network.password.c_str());
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && !apFallbackTriggered && (millis() - wifiStart < wifiTimeout)) {
            delay(500);
            debugPrint(".");
#ifdef ESP8266
            debugPrint(", error WL_WRONG_PASSWORD = ");
            debugPrintln(String(WL_WRONG_PASSWORD).c_str());
            if (WiFi.status() == WL_WRONG_PASSWORD) {
                debugPrintln("\n[WiFi] Wrong password detected, switching to AP mode");
                wrongPassword = true;
                break;
            }
#else
            debugPrint(", error WL_CONNECT_FAILED = ");
            debugPrint(String(WL_CONNECT_FAILED).c_str());
#if defined(ARDUINO_ARCH_ESP32)
            debugPrint(", esp_err_t = ");
            debugPrintln(String(WiFi.status()).c_str());
#endif
            if (WiFi.status() == WL_CONNECT_FAILED) {
                debugPrintln("\n[WiFi] Connection failed, switching to AP mode");
                wrongPassword = true;
                break;
            }
#endif
            attempts++;
        }
        debugPrintln("");
        debugPrintln("[WiFi] Connection attempt done");
        if (WiFi.status() == WL_CONNECTED) {
            debugPrintln("");
            debugPrintln("[WiFi] Connected!");
            debugPrint("Connected! IP: ");
            debugPrintln(WiFi.localIP());
            stopCaptivePortal();
            return;
        }
        if (apFallbackTriggered || wrongPassword || (millis() - wifiStart >= wifiTimeout)) {
            debugPrintln("\n[WiFi] Fallback to AP mode after failed attempts or timeout");
            apFallbackTriggered = true;
        }
    }
#ifndef ESP8266
    if (WiFi.status() != WL_CONNECTED) {
        debugPrintln("");
        debugPrintln("[WiFi] Not connected after attempts, switching to AP mode");
        apFallbackTriggered = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP(config.network.hostname.c_str(), config.network.apPassword.c_str());
        debugPrint("AP IP: ");
        debugPrintln(WiFi.softAPIP());
        startCaptivePortal(WiFi.softAPIP());
        return;
    }
#endif
    debugPrintln("");
    debugPrintln("[WiFi] Starting Access Point mode");
    apFallbackTriggered = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(config.network.hostname.c_str(), config.network.apPassword.c_str());
    debugPrint("AP IP: ");
    debugPrintln(WiFi.softAPIP());
    startCaptivePortal(WiFi.softAPIP());
}


// Returns the current IP as a string (AP or STA mode)
String getCurrentIpString(const Configuration& config) {
    if (WiFi.getMode() == WIFI_AP) {
        return WiFi.softAPIP().toString();
    } else {
        return WiFi.localIP().toString();
    }
}

// Handles WiFi reconnect and captive portal DNS (call in main loop)
void networkLoop(Configuration& config) {
    // Reconnect logic if needed (if you have a reconnect timer, move it here)
    // Captive portal DNS loop (if using DNSServer)
    #ifdef USE_DNSSERVER
    if (dnsServer) dnsServer->processNextRequest();
    #endif
    // Add any periodic WiFi/captive portal logic here
}


String urlDecode(const String &input) {
    String decoded;
    char temp[3] = {0};
    for (size_t i = 0; i < input.length(); ++i) {
        if (input[i] == '%') {
            if (i + 2 < input.length()) {
                temp[0] = input[i + 1];
                temp[1] = input[i + 2];
                decoded += (char)strtol(temp, nullptr, 16);
                i += 2;
            }
        } else if (input[i] == '+') {
            decoded += ' ';
        } else {
            decoded += input[i];
        }
    }
    return decoded;
}

void setupWiFi(Configuration& config) {
    debugPrintln("[WiFi] setupWiFi() called");
    debugPrint("Connecting to WiFi");
#ifdef ESP8266
    WiFi.hostname(config.network.hostname);
#else
    WiFi.setHostname(config.network.hostname.c_str());
#endif
    if (config.network.ssid.length() > 0 && !apFallbackTriggered) {
        debugPrintln("");
        debugPrintln("[WiFi] Calling WiFi.begin");
        const unsigned long wifiTimeout = 10000;
        unsigned long wifiStart = millis();
        bool wrongPassword = false;
        WiFi.begin(config.network.ssid.c_str(), config.network.password.c_str());
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && !apFallbackTriggered && (millis() - wifiStart < wifiTimeout)) {
            delay(500);
            debugPrint(".");
#ifdef ESP8266
            debugPrint(", error WL_WRONG_PASSWORD = ");
            debugPrintln(String(WL_WRONG_PASSWORD).c_str());
            if (WiFi.status() == WL_WRONG_PASSWORD) {
                debugPrintln("\n[WiFi] Wrong password detected, switching to AP mode");
                wrongPassword = true;
                break;
            }
#else
            debugPrint(", error WL_CONNECT_FAILED = ");
            debugPrint(String(WL_CONNECT_FAILED).c_str());
#if defined(ARDUINO_ARCH_ESP32)
            debugPrint(", esp_err_t = ");
            debugPrintln(String(WiFi.status()).c_str());
#endif
            if (WiFi.status() == WL_CONNECT_FAILED) {
                debugPrintln("\n[WiFi] Connection failed, switching to AP mode");
                wrongPassword = true;
                break;
            }
#endif
            attempts++;
        }
        debugPrintln("");
        debugPrintln("[WiFi] Connection attempt done");
        if (WiFi.status() == WL_CONNECTED) {
            debugPrintln("");
            debugPrintln("[WiFi] Connected!");
            debugPrint("Connected! IP: ");
            debugPrintln(WiFi.localIP());
            stopCaptivePortal();
            return;
        }
        if (apFallbackTriggered || wrongPassword || (millis() - wifiStart >= wifiTimeout)) {
            debugPrintln("\n[WiFi] Fallback to AP mode after failed attempts or timeout");
            apFallbackTriggered = true;
        }
    }
#ifndef ESP8266
    if (WiFi.status() != WL_CONNECTED) {
        debugPrintln("");
        debugPrintln("[WiFi] Not connected after attempts, switching to AP mode");
        apFallbackTriggered = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP(config.network.hostname.c_str(), config.network.apPassword.c_str());
        debugPrint("AP IP: ");
        debugPrintln(WiFi.softAPIP());
        startCaptivePortal(WiFi.softAPIP());
        return;
    }
#endif
    debugPrintln("");
    debugPrintln("[WiFi] Starting Access Point mode");
    apFallbackTriggered = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(config.network.hostname.c_str(), config.network.apPassword.c_str());
    debugPrint("AP IP: ");
    debugPrintln(WiFi.softAPIP());
    startCaptivePortal(WiFi.softAPIP());
}

void handleWiFiReconnect(Configuration& config) {
    static uint32_t lastWiFiCheck = 0;
    static int wifiReconnectAttempts = 0;
    const int wifiReconnectInterval = 10000;
    const int maxWiFiReconnectAttempts = 5;
    if (WiFi.getMode() != WIFI_AP && config.network.ssid.length() > 0) {
        if (WiFi.status() != WL_CONNECTED) {
            uint32_t now = millis();
            if (now - lastWiFiCheck > wifiReconnectInterval) {
                debugPrintln("[WiFi] Lost connection, attempting reconnect...");
                WiFi.disconnect();
                delay(100);
                WiFi.begin(config.network.ssid.c_str(), config.network.password.c_str());
                wifiReconnectAttempts++;
                lastWiFiCheck = now;
                if (wifiReconnectAttempts >= maxWiFiReconnectAttempts) {
                    debugPrintln("[WiFi] Too many failed reconnects, switching to AP mode");
                    apFallbackTriggered = true;
                    WiFi.mode(WIFI_AP);
                    WiFi.softAP(config.network.hostname.c_str(), config.network.apPassword.c_str());
                    startCaptivePortal(WiFi.softAPIP());
                    wifiReconnectAttempts = 0;
                }
            }
        } else {
            wifiReconnectAttempts = 0;
        }
    }
}

void setupWiFiHandlers(AsyncWebServer* server, Configuration* config) {
    // POST handler for /wifi
    server->on("/wifi", HTTP_POST,
        [config](AsyncWebServerRequest *request) {
            if (request->hasParam("ssid", true)) {
                String ssid = urlDecode(request->getParam("ssid", true)->value());
                String password = request->hasParam("password", true)
                    ? urlDecode(request->getParam("password", true)->value())
                    : "";
                if (ssid.length() > 0) {
                    config->network.ssid = ssid;
                    config->network.password = password;
                    config->save();
                    String html = "<html><body><h2>Connecting to WiFi...</h2><p>Device will reboot if successful.</p></body></html>";
                    request->send(200, "text/html", html);
                    delay(1000);
                    ESP.restart();
                    return;
                }
                request->send_P(200, "text/html", web_wifi_html, web_wifi_html_len);
            }
        },
        nullptr,
        [config](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t, size_t) {
            String body;
            for (size_t i = 0; i < len; ++i)
                body += (char)data[i];
            String ssid, password;
            int ssidIdx = body.indexOf("ssid=");
            int passIdx = body.indexOf("password=");
            if (ssidIdx != -1) {
                int amp = body.indexOf('&', ssidIdx);
                ssid = urlDecode(body.substring(ssidIdx + 5, amp == -1 ? body.length() : amp));
            }
            if (passIdx != -1) {
                int amp = body.indexOf('&', passIdx);
                password = urlDecode(body.substring(passIdx + 9, amp == -1 ? body.length() : amp));
            }
            if (ssid.length() > 0) {
                config->network.ssid = ssid;
                config->network.password = password;
                config->save();
                String html = "<html><body><h2>Connecting to WiFi...</h2><p>Device will reboot if successful.</p></body></html>";
                request->send(200, "text/html", html);
                delay(1000);
                ESP.restart();
                return;
            }
            request->send_P(200, "text/html", web_wifi_html, web_wifi_html_len);
        }
    );

    // GET handler for /wifi
    server->on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", web_wifi_html, web_wifi_html_len);
    });
}

void startCaptivePortal(const IPAddress &apIP) {
    captiveDnsServer.start(53, "*", apIP);
}

void stopCaptivePortal() {
    captiveDnsServer.stop();
}

void handleCaptivePortalDns() {
    captiveDnsServer.processNextRequest();
}
