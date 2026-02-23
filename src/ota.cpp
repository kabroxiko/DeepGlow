#define DEEPGLOW_REPO_URL "https://github.com/kabroxiko/DeepGlow"

#if defined(ESP_PLATFORM) && !defined(ARDUINO)

#include "ota.h"
#include "debug.h"
#include "webserver.h"
#include "config.h"
#include "scheduler.h"
#include "transition.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_littlefs.h"
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <string>
#if defined(ESP_IDF_VERSION_MAJOR)
#include <ArduinoJson.h>
#endif
#include "uzlib.h"

static const char *TAG = "ota";

extern WebServerManager *webServerPtr;

volatile bool otaInProgress = false;
volatile bool otaRequested  = false;
volatile bool otaAckReceived = false;

// ── OTA status broadcast ───────────────────────────────────────────────────────
static void broadcastOtaStatus(const std::string &status,
                                const std::string &msg, int progress) {
    if (progress >= 0)
        ESP_LOGI(TAG, "OTA status=%s msg=%s progress=%d",
                 status.c_str(), msg.c_str(), progress);
    else
        ESP_LOGI(TAG, "OTA status=%s msg=%s", status.c_str(), msg.c_str());

    if (webServerPtr) {
        if (progress >= 0)
            webServerPtr->broadcastOtaStatus(status, msg, progress);
        else
            webServerPtr->broadcastOtaStatus(status, msg);
    }
}

// ── No-op stubs for ArduinoOTA compatibility ───────────────────────────────────
void setupArduinoOTA(const char * /* hostname */) {}
void handleArduinoOTA() {}

// ── HTTPS helper: fetch URL into a std::string ─────────────────────────────────
static std::string httpsGet(const char *url) {
    std::string result;
    esp_http_client_config_t cfg = {};
    cfg.url    = url;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.method = HTTP_METHOD_GET;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return result;

    if (esp_http_client_open(client, 0) != ESP_OK) {
        esp_http_client_cleanup(client);
        return result;
    }
    int64_t clen = esp_http_client_fetch_headers(client);
    int code = esp_http_client_get_status_code(client);
    if (code == 200 && clen > 0) {
        result.resize((size_t)clen, '\0');
        int got = esp_http_client_read(client, &result[0], (int)clen);
        if (got < 0) result.clear();
        else         result.resize((size_t)got);
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return result;
}

// ── Manifest helpers ───────────────────────────────────────────────────────────
std::string fetchRemoteManifestJson() {
    const char *url = DEEPGLOW_REPO_URL "/releases/latest/download/manifest.json";
    return httpsGet(url);
}

std::string getLatestFirmwareUrl(std::string &latestVersion) {
    std::string payload = fetchRemoteManifestJson();
    if (payload.empty()) { latestVersion = ""; return ""; }

    #if defined(ESP_IDF_VERSION_MAJOR)
        DynamicJsonDocument doc(2048);
        if (deserializeJson(doc, payload)) { latestVersion = ""; return ""; }

        const char *targetEnv = OTA_ENV;
        for (JsonVariant entry : doc.as<JsonArray>()) {
            if (strcmp(entry["env"] | "", targetEnv) == 0) {
                latestVersion = entry["version"] | "";
                std::string url  = entry["url"] | "";
                return url;
            }
        }
        latestVersion = "";
        return "";
    #else
        // Arduino: manual JSON parsing (assumes manifest.json is a flat array of objects)
        latestVersion = "";
        std::string url = "";
        size_t pos = 0;
        const std::string envKey = "\"env\":\"";
        const std::string versionKey = "\"version\":\"";
        const std::string urlKey = "\"url\":\"";
        while ((pos = payload.find(envKey, pos)) != std::string::npos) {
            size_t envStart = pos + envKey.length();
            size_t envEnd = payload.find("\"", envStart);
            std::string envVal = payload.substr(envStart, envEnd - envStart);
            if (envVal == OTA_ENV) {
                // Find version
                size_t versionPos = payload.find(versionKey, envEnd);
                if (versionPos != std::string::npos) {
                    size_t versionStart = versionPos + versionKey.length();
                    size_t versionEnd = payload.find("\"", versionStart);
                    latestVersion = payload.substr(versionStart, versionEnd - versionStart);
                }
                // Find url
                size_t urlPos = payload.find(urlKey, envEnd);
                if (urlPos != std::string::npos) {
                    size_t urlStart = urlPos + urlKey.length();
                    size_t urlEnd = payload.find("\"", urlStart);
                    url = payload.substr(urlStart, urlEnd - urlStart);
                }
                return url;
            }
            pos = envEnd;
        }
        return "";
    #endif
}

// ── gz write callback – uses esp_ota_ops ──────────────────────────────────────
static esp_ota_handle_t     s_ota_handle    = 0;
static const esp_partition_t *s_ota_part    = nullptr;
static bool                 s_ota_started   = false;
static size_t               s_ota_written   = 0;
static int                  s_ota_total_est = 0;

static bool gzWriteCallback(unsigned char *buff, size_t buffsize) {
    if (!s_ota_started) {
        s_ota_part = esp_ota_get_next_update_partition(NULL);
        if (!s_ota_part) {
            ESP_LOGE(TAG, "No OTA partition");
            return false;
        }
        if (esp_ota_begin(s_ota_part, OTA_WITH_SEQUENTIAL_WRITES, &s_ota_handle) != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_begin failed");
            return false;
        }
        s_ota_started = true;
        s_ota_written = 0;
    }
    if (esp_ota_write(s_ota_handle, buff, buffsize) != ESP_OK) return false;
    s_ota_written += buffsize;
    if (s_ota_total_est > 0) {
        int pct = (int)(s_ota_written * 100 / (size_t)s_ota_total_est);
        if (pct > 100) pct = 100;
        static int lastPct = -1;
        if (pct != lastPct) { broadcastOtaStatus("progress", "", pct); lastPct = pct; }
    }
    esp_task_wdt_reset();
    return true;
}

// ── Remote gz OTA ──────────────────────────────────────────────────────────────
bool performGzOtaUpdate(std::string &errorOut) {
    otaInProgress = true;
    s_ota_started = false;
    s_ota_written = 0;

    broadcastOtaStatus("start", "OTA update started", -1);

    std::string latestVersion;
    std::string firmwareUrl = getLatestFirmwareUrl(latestVersion);
    if (firmwareUrl.empty()) {
        errorOut = "Could not get firmware URL";
        otaInProgress = false;
        broadcastOtaStatus("error", errorOut, -1);
        return false;
    }
    ESP_LOGI(TAG, "Firmware URL: %s", firmwareUrl.c_str());

    // Download firmware blob into memory (up to 2 MB)
    esp_http_client_config_t cfg = {};
    cfg.url               = firmwareUrl.c_str();
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.method            = HTTP_METHOD_GET;
    cfg.timeout_ms        = 30000;
    cfg.buffer_size       = 4096;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) { otaInProgress = false; errorOut = "http init failed"; broadcastOtaStatus("error", errorOut, -1); return false; }

    if (esp_http_client_open(client, 0) != ESP_OK) {
        esp_http_client_cleanup(client);
        otaInProgress = false; errorOut = "http open failed"; broadcastOtaStatus("error", errorOut, -1); return false;
    }
    int64_t clen = esp_http_client_fetch_headers(client);
    int code     = esp_http_client_get_status_code(client);
    if (code != 200 || clen <= 0) {
        esp_http_client_close(client); esp_http_client_cleanup(client);
        otaInProgress = false; errorOut = "HTTP error " + std::to_string(code);
        broadcastOtaStatus("error", errorOut, -1); return false;
    }
    s_ota_total_est = (int)(clen * 3 / 2); // estimate decompressed size

    // Decompress streaming directly through GzUnpacker
    // We need a File-like object; save to temp file on flash first
    const char *tmpPath = "/data/ota_tmp.bin.gz";
    FILE *fout = fopen(tmpPath, "wb");
    if (!fout) {
        esp_http_client_close(client); esp_http_client_cleanup(client);
        otaInProgress = false; errorOut = "tmpfile open failed"; broadcastOtaStatus("error", errorOut, -1); return false;
    }
    char buf[2048];
    int  rd;
    while ((rd = esp_http_client_read(client, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, rd, fout);
    }
    fclose(fout);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    // Decompress the .bin.gz using uzlib and write chunks via OTA callback
    s_ota_started = false; s_ota_written = 0;
    FILE *fin = fopen(tmpPath, "rb");
    if (!fin) {
        otaInProgress = false; errorOut = "tmpfile reopen failed"; broadcastOtaStatus("error", errorOut, -1); return false;
    }
    fseek(fin, 0, SEEK_END);
    long fsz = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    uint8_t *gz_buf = (uint8_t *)malloc(fsz);
    bool ok = false;
    if (!gz_buf) {
        fclose(fin);
        otaInProgress = false; errorOut = "malloc failed for gz buffer"; broadcastOtaStatus("error", errorOut, -1); return false;
    }
    fread(gz_buf, 1, fsz, fin);
    fclose(fin);

    // Allocate a dictionary buffer (32 KB) for uzlib sliding window
    unsigned int dictSize = 32768;
    unsigned char *dict = (unsigned char *)malloc(dictSize);
    if (!dict) {
        free(gz_buf); remove(tmpPath);
        otaInProgress = false; errorOut = "malloc failed for dict"; broadcastOtaStatus("error", errorOut, -1); return false;
    }

    TINF_DATA d = {};
    uzlib_init();
    d.source       = gz_buf;
    d.source_limit = gz_buf + fsz;

    // Parse the gzip header
    if (uzlib_gzip_parse_header(&d) != TINF_OK) {
        free(gz_buf); free(dict); remove(tmpPath);
        otaInProgress = false; errorOut = "gzip header parse failed"; broadcastOtaStatus("error", errorOut, -1); return false;
    }
    uzlib_uncompress_init(&d, dict, dictSize);

    // Decompress in 4 KB chunks and write to OTA
    const size_t OUT_CHUNK = 4096;
    uint8_t *outbuf = (uint8_t *)malloc(OUT_CHUNK);
    if (!outbuf) {
        free(gz_buf); free(dict); remove(tmpPath);
        otaInProgress = false; errorOut = "malloc outbuf failed"; broadcastOtaStatus("error", errorOut, -1); return false;
    }
    ok = true;
    int ret = TINF_OK;
    while (ret == TINF_OK) {
        d.dest          = outbuf;
        d.destStart     = outbuf;
        d.destSize      = (unsigned int)OUT_CHUNK;
        d.destRemaining = (unsigned int)OUT_CHUNK;
        ret = uzlib_uncompress_chksum(&d);
        size_t produced = (size_t)(d.dest - outbuf);
        if (produced > 0) {
            if (!gzWriteCallback(outbuf, produced)) {
                ok = false;
                if (errorOut.empty()) errorOut = "OTA write failed";
                break;
            }
        }
        if (ret == TINF_DONE) break;
        if (ret < 0) { ok = false; errorOut = "gzip decompress error"; break; }
    }
    free(outbuf); free(dict); free(gz_buf);
    remove(tmpPath);

    if (!ok || !s_ota_started) {
        if (s_ota_started) esp_ota_abort(s_ota_handle);
        otaInProgress = false;
        if (errorOut.empty()) errorOut = "gz decompression/flash failed";
        broadcastOtaStatus("error", errorOut, -1);
        return false;
    }

    if (esp_ota_end(s_ota_handle) != ESP_OK) {
        otaInProgress = false; errorOut = "esp_ota_end failed";
        broadcastOtaStatus("error", errorOut, -1); return false;
    }
    if (esp_ota_set_boot_partition(s_ota_part) != ESP_OK) {
        otaInProgress = false; errorOut = "set_boot_partition failed";
        broadcastOtaStatus("error", errorOut, -1); return false;
    }

    otaInProgress = false;
    broadcastOtaStatus("progress", "", 100);
    broadcastOtaStatus("success", "OTA update successful", -1);
    return true;
}

// ── Local firmware upload handler (esp_http_server) ───────────────────────────
esp_err_t handleOtaUpload(httpd_req_t *req) {
    otaInProgress = true;
    broadcastOtaStatus("start", "Local OTA upload started", -1);

    esp_ota_handle_t       ota_handle = 0;
    const esp_partition_t *ota_part   = esp_ota_get_next_update_partition(NULL);
    if (!ota_part) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        otaInProgress = false; return ESP_FAIL;
    }
    if (esp_ota_begin(ota_part, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota_begin failed");
        otaInProgress = false; return ESP_FAIL;
    }

    int total = req->content_len;
    int remaining = total;
    char buf[2048];
    int written_total = 0;

    while (remaining > 0) {
        int recv = httpd_req_recv(req, buf, MIN((int)sizeof(buf), remaining));
        if (recv <= 0) {
            if (recv == HTTPD_SOCK_ERR_TIMEOUT) continue;
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive error");
            otaInProgress = false; return ESP_FAIL;
        }
        if (esp_ota_write(ota_handle, (const void *)buf, recv) != ESP_OK) {
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota_write error");
            otaInProgress = false; return ESP_FAIL;
        }
        written_total += recv;
        remaining    -= recv;
        if (total > 0) {
            int pct = written_total * 100 / total;
            static int lastPct = -1;
            if (pct != lastPct) { broadcastOtaStatus("progress", "", pct); lastPct = pct; }
        }
        esp_task_wdt_reset();
    }

    if (esp_ota_end(ota_handle) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota_end failed");
        otaInProgress = false; return ESP_FAIL;
    }
    if (esp_ota_set_boot_partition(ota_part) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "set_boot failed");
        otaInProgress = false; return ESP_FAIL;
    }

    otaInProgress = false;
    broadcastOtaStatus("progress", "", 100);
    broadcastOtaStatus("success", "OTA successful – rebooting", -1);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true,\"message\":\"Rebooting\"}");

    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

// ── otaTask: called from webserver when remote OTA is requested ────────────────
extern "C" void otaTask(void *parameter) {
    std::string error;
    bool ok = performGzOtaUpdate(error);
    if (ok) {
        otaAckReceived = false;
        uint64_t start = esp_timer_get_time() / 1000ULL;
        while ((esp_timer_get_time() / 1000ULL - start) < 3000) {
            if (otaAckReceived) break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (webServerPtr) webServerPtr->closeOtaClients();
        start = esp_timer_get_time() / 1000ULL;
        while ((esp_timer_get_time() / 1000ULL - start) < 2000) {
            if (webServerPtr && webServerPtr->otaClientsConnected() == 0) break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA failed: %s", error.c_str());
        broadcastOtaStatus("error", error.empty() ? "OTA failed" : error, -1);
    }
    vTaskDelete(NULL);
}

#else // Arduino – full implementation

#include "ota.h"
#include "debug.h"
#include "webserver.h"
#include "config.h"

#include <Arduino.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <uzlib.h>
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"

static const char *TAG_A = "ota";

extern WebServerManager *webServerPtr;

volatile bool otaInProgress  = false;
volatile bool otaRequested   = false;
volatile bool otaAckReceived = false;

static void broadcastOtaStatus(const std::string &status,
                                const std::string &msg, int progress = -1) {
    Serial.printf("[ota] status=%s msg=%s progress=%d\n",
                  status.c_str(), msg.c_str(), progress);
    if (webServerPtr) {
        if (progress >= 0)
            webServerPtr->broadcastOtaStatus(status, msg, progress);
        else
            webServerPtr->broadcastOtaStatus(status, msg);
    }
}

void setupArduinoOTA(const char *hostname) {
    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.setPort(3232);
    ArduinoOTA.onStart([]() {
        otaInProgress = true;
        Serial.println("[ota] ArduinoOTA start");
    });
    ArduinoOTA.onEnd([]() {
        otaInProgress = false;
        Serial.println("[ota] ArduinoOTA done");
    });
    ArduinoOTA.onError([](ota_error_t error) {
        otaInProgress = false;
        Serial.printf("[ota] ArduinoOTA error: %u\n", error);
    });
    ArduinoOTA.begin();
}

void handleArduinoOTA() {
    ArduinoOTA.handle();
}

std::string fetchRemoteManifestJson() {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15); // TCP timeout in seconds
    HTTPClient https;
    const char *url = DEEPGLOW_REPO_URL "/releases/latest/download/manifest.json";
    Serial.printf("[ota] fetchManifest: %s\n", url);
    https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    https.setTimeout(15000);
    if (!https.begin(client, url)) {
        Serial.println("[ota] fetchManifest: https.begin() failed");
        return "";
    }
    int code = https.GET();
    Serial.printf("[ota] fetchManifest HTTP %d\n", code);
    std::string result;
    if (code == 200) {
        String payload = https.getString();
        result = std::string(payload.c_str());
        // Log first 120 chars for debugging
        Serial.printf("[ota] fetchManifest payload[0..120]: %.120s\n", result.c_str());
    } else {
        String body = https.getString();
        Serial.printf("[ota] fetchManifest body[0..80]: %.80s\n", body.c_str());
    }
    https.end();
    return result;
}

std::string getLatestFirmwareUrl(std::string &latestVersion) {
    std::string payload = fetchRemoteManifestJson();
    latestVersion = "";
    if (payload.empty()) return "";

    // Parse with ArduinoJson (manifest is a JSON array of {env, version, url} objects)
    // Use a heap-allocated document to handle variable-size manifests
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, payload.c_str());
    if (err) {
        Serial.printf("[ota] manifest JSON parse error: %s\n", err.c_str());
        return "";
    }

    const char *targetEnv = OTA_ENV;
    Serial.printf("[ota] getLatestFirmwareUrl: looking for env='%s'\n", targetEnv);

    // Support both array format and single-object format
    JsonArray arr;
    if (doc.is<JsonArray>()) {
        arr = doc.as<JsonArray>();
    } else if (doc.is<JsonObject>()) {
        // Single object: wrap check inline
        JsonObject obj = doc.as<JsonObject>();
        const char *envVal = obj["env"] | "";
        Serial.printf("[ota] manifest single object env='%s'\n", envVal);
        if (strcmp(envVal, targetEnv) == 0) {
            latestVersion = obj["version"] | "";
            return std::string(obj["url"] | "");
        }
        Serial.printf("[ota] manifest env mismatch: got '%s' want '%s'\n", envVal, targetEnv);
        return "";
    }

    for (JsonObject entry : arr) {
        const char *envVal = entry["env"] | "";
        Serial.printf("[ota] manifest entry env='%s'\n", envVal);
        if (strcmp(envVal, targetEnv) == 0) {
            latestVersion = entry["version"] | "";
            return std::string(entry["url"] | "");
        }
    }
    Serial.printf("[ota] no manifest entry matched env='%s'\n", targetEnv);
    return "";
}

// -- uzlib helpers for remote OTA (file-based streaming) --
static File g_ota_file;

static void ota_uzlib_log(const char *fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.print(buf);
}

static unsigned int uzlib_file_read_byte(TINF_DATA *d, unsigned char *out) {
    if (!g_ota_file.available()) return (unsigned int)-1;
    int c = g_ota_file.read();
    if (c < 0) return (unsigned int)-1;
    *out = (unsigned char)c;
    return 1;
}

bool performGzOtaUpdate(std::string &errorOut) {
    otaInProgress = true;
    broadcastOtaStatus("start", "OTA update started");

    std::string latestVersion;
    std::string firmwareUrl = getLatestFirmwareUrl(latestVersion);
    if (firmwareUrl.empty()) {
        errorOut = "Could not get firmware URL";
        otaInProgress = false;
        return false;
    }
    Serial.printf("[ota] Downloading: %s\n", firmwareUrl.c_str());

    // ── Phase 1: Download .bin.gz to LittleFS ──────────────────────────────
    const char *tmpPath = "/ota_tmp.gz";
    {
        WiFiClientSecure dlClient;
        dlClient.setInsecure();
        dlClient.setTimeout(30);
        HTTPClient dlHttp;
        if (!dlHttp.begin(dlClient, firmwareUrl.c_str())) {
            errorOut = "http begin failed";
            otaInProgress = false;
            return false;
        }
        dlHttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        dlHttp.setTimeout(60000);
        int code = dlHttp.GET();
        if (code != 200) {
            dlHttp.end();
            errorOut = "HTTP " + std::to_string(code);
            otaInProgress = false;
            return false;
        }
        int contentLen = dlHttp.getSize();
        Serial.printf("[ota] Content-Length: %d\n", contentLen);

        File f = LittleFS.open(tmpPath, "w");
        if (!f) {
            dlHttp.end();
            errorOut = "LittleFS open for write failed";
            otaInProgress = false;
            return false;
        }

        WiFiClient *stream = dlHttp.getStreamPtr();
        uint8_t dlBuf[2048];
        int totalDl = 0;
        int lastPct = -1;
        while (dlHttp.connected() && (contentLen < 0 || totalDl < contentLen)) {
            size_t avail = stream->available();
            if (avail) {
                int rd = stream->readBytes(dlBuf, min(sizeof(dlBuf), avail));
                if (rd > 0) {
                    f.write(dlBuf, rd);
                    totalDl += rd;
                    if (contentLen > 0) {
                        int pct = totalDl * 45 / contentLen; // 0-45% for download
                        if (pct != lastPct) { broadcastOtaStatus("progress", "", pct); lastPct = pct; }
                    }
                }
            } else {
                yield();
            }
            esp_task_wdt_reset();
        }
        f.close();
        dlHttp.end();
        Serial.printf("[ota] Downloaded %d bytes\n", totalDl);

        if (contentLen > 0 && totalDl < contentLen) {
            LittleFS.remove(tmpPath);
            errorOut = "Download incomplete";
            otaInProgress = false;
            return false;
        }
    } // dlClient / dlHttp freed here

    // ── Phase 2: Decompress with uzlib and flash ───────────────────────────
    g_ota_file = LittleFS.open(tmpPath, "r");
    if (!g_ota_file) {
        LittleFS.remove(tmpPath);
        errorOut = "LittleFS open for read failed";
        otaInProgress = false;
        return false;
    }
    int fileSize = (int)g_ota_file.size();
    Serial.printf("[ota] Decompressing %d bytes...\n", fileSize);

    unsigned char *dict   = (unsigned char*)malloc(32768);
    uint8_t       *outbuf = (uint8_t*)malloc(4096);
    if (!dict || !outbuf) {
        if (dict)   free(dict);
        if (outbuf) free(outbuf);
        g_ota_file.close();
        LittleFS.remove(tmpPath);
        errorOut = "malloc failed";
        otaInProgress = false;
        return false;
    }

    TINF_DATA d;
    memset(&d, 0, sizeof(d));
    d.readSourceByte = uzlib_file_read_byte;
    d.log            = ota_uzlib_log;
    uzlib_init();

    int ret = uzlib_gzip_parse_header(&d);
    if (ret != TINF_OK) {
        free(dict); free(outbuf);
        g_ota_file.close();
        LittleFS.remove(tmpPath);
        errorOut = "gzip header parse failed: " + std::to_string(ret);
        otaInProgress = false;
        return false;
    }
    uzlib_uncompress_init(&d, dict, 32768);

    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        free(dict); free(outbuf);
        g_ota_file.close();
        LittleFS.remove(tmpPath);
        errorOut = "Update.begin failed: " + std::to_string((int)Update.getError());
        otaInProgress = false;
        return false;
    }

    size_t totalOut = 0;
    int lastPct = -1;
    ret = TINF_OK;
    while (ret == TINF_OK) {
        d.dest          = outbuf;
        d.destStart     = outbuf;
        d.destSize      = 4096;
        d.destRemaining = 4096;
        ret = uzlib_uncompress(&d);
        size_t produced = (size_t)(d.dest - outbuf);
        if (produced > 0) {
            if (Update.write(outbuf, produced) != produced) {
                errorOut = "Update.write failed";
                Update.abort();
                free(dict); free(outbuf);
                g_ota_file.close();
                LittleFS.remove(tmpPath);
                otaInProgress = false;
                return false;
            }
            totalOut += produced;
        }
        if (ret == TINF_DONE) break;
        if (ret < 0) {
            errorOut = "decompress error: " + std::to_string(ret);
            Update.abort();
            free(dict); free(outbuf);
            g_ota_file.close();
            LittleFS.remove(tmpPath);
            otaInProgress = false;
            return false;
        }
        // Decompress phase: 45-99%
        if (fileSize > 0) {
            int fpos = (int)g_ota_file.position();
            int pct  = 45 + (fpos * 54 / fileSize);
            if (pct > 99) pct = 99;
            if (pct != lastPct) { broadcastOtaStatus("progress", "", pct); lastPct = pct; }
        }
        esp_task_wdt_reset();
    }

    free(dict);
    free(outbuf);
    g_ota_file.close();
    LittleFS.remove(tmpPath);
    Serial.printf("[ota] decompressed %u bytes\n", totalOut);

    if (!Update.end(true) || !Update.isFinished()) {
        errorOut = std::string("Update.end failed: ") + std::to_string((int)Update.getError())
                   + " (" + Update.errorString() + ")";
        otaInProgress = false;
        return false;
    }

    otaInProgress = false;
    broadcastOtaStatus("progress", "", 100);
    broadcastOtaStatus("success", "OTA update successful");
    return true;
}

esp_err_t handleOtaUpload(httpd_req_t *req) {
    otaInProgress = true;
    broadcastOtaStatus("start", "Local OTA upload started");

    int total = req->content_len;
    Serial.printf("[ota] handleOtaUpload: content_len=%d\n", total);

    if (!Update.begin(total > 0 ? (size_t)total : UPDATE_SIZE_UNKNOWN)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Update.begin failed");
        otaInProgress = false;
        return ESP_FAIL;
    }

    int remaining = total;
    char buf[2048];
    int written_total = 0;
    int lastPct = -1;

    while ((total < 0 && !Update.isFinished()) || (total > 0 && remaining > 0)) {
        int recv = httpd_req_recv(req, buf, sizeof(buf));
        if (recv < 0) {
            if (recv == HTTPD_SOCK_ERR_TIMEOUT) continue;
            Update.abort();
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive error");
            otaInProgress = false;
            return ESP_FAIL;
        }
        if (recv == 0) break;
        if (Update.write((uint8_t *)buf, recv) != (size_t)recv) {
            Update.abort();
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Update.write failed");
            otaInProgress = false;
            return ESP_FAIL;
        }
        written_total += recv;
        if (total > 0) remaining -= recv;
        if (total > 0) {
            int pct = written_total * 100 / total;
            if (pct != lastPct) { broadcastOtaStatus("progress", "", pct); lastPct = pct; }
        }
    }

    if (!Update.end(true)) {
        char err[64];
        snprintf(err, sizeof(err), "Update.end failed: %d", (int)Update.getError());
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
        otaInProgress = false;
        return ESP_FAIL;
    }

    otaInProgress = false;
    broadcastOtaStatus("progress", "", 100);
    broadcastOtaStatus("success", "OTA successful – rebooting");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true,\"message\":\"Rebooting\"}");

    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

extern "C" void otaTask(void *parameter) {
    std::string error;
    bool ok = performGzOtaUpdate(error);
    if (ok) {
        otaAckReceived = false;
        uint64_t start = esp_timer_get_time() / 1000ULL;
        while ((esp_timer_get_time() / 1000ULL - start) < 3000) {
            if (otaAckReceived) break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (webServerPtr) webServerPtr->closeOtaClients();
        start = esp_timer_get_time() / 1000ULL;
        while ((esp_timer_get_time() / 1000ULL - start) < 2000) {
            if (webServerPtr && webServerPtr->otaClientsConnected() == 0) break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        esp_restart();
    } else {
        Serial.printf("[ota] failed: %s\n", error.c_str());
        broadcastOtaStatus("error", error.empty() ? "OTA failed" : error);
    }
    vTaskDelete(NULL);
}

#endif // ARDUINO
