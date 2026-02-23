#define DEEPGLOW_REPO_URL "https://github.com/kabroxiko/DeepGlow"

// ── Common includes (both IDF and Arduino) ─────────────────────────────────────
#include "ota.h"
#include "debug.h"
#include "webserver.h"
#include "config.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <ArduinoJson.h>
#include "uzlib.h"
#include <string>
#include <stdio.h>
#include <string.h>

extern WebServerManager *webServerPtr;

volatile bool otaInProgress  = false;
volatile bool otaRequested   = false;
volatile bool otaAckReceived = false;

// Reset the watchdog for the current task, registering it first if needed.
static inline void ota_wdt_reset() {
    esp_err_t err = esp_task_wdt_reset();
    if (err == ESP_ERR_NOT_FOUND) {
        esp_task_wdt_add(NULL);
        esp_task_wdt_reset();
    }
}

#if defined(ESP_PLATFORM) && !defined(ARDUINO)

// ── IDF-only includes ──────────────────────────────────────────────────────────
#include "scheduler.h"
#include "transition.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_littlefs.h"
#include <sys/stat.h>

static const char *TAG = "ota";

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
    ota_wdt_reset();
    return true;
}

// ── Streaming byte reader for uzlib (avoids loading entire gz into RAM) ───────
static FILE *g_idf_gz_fp = nullptr;

static unsigned int idf_uzlib_read_byte(TINF_DATA *d, unsigned char *out) {
    (void)d;
    if (!g_idf_gz_fp) return (unsigned int)-1;
    int c = fgetc(g_idf_gz_fp);
    if (c == EOF) return (unsigned int)-1;
    *out = (unsigned char)c;
    return 1;
}

// ── Decompress .gz from disk and flash via gzWriteCallback ───────────────────
static bool idf_decompressGzAndFlash(const char *path, std::string &errorOut) {
    g_idf_gz_fp = fopen(path, "rb");
    if (!g_idf_gz_fp) { errorOut = "gz file open failed"; return false; }

    // Size is used only for progress estimation — no large malloc needed.
    fseek(g_idf_gz_fp, 0, SEEK_END);
    long fsz = ftell(g_idf_gz_fp);
    fseek(g_idf_gz_fp, 0, SEEK_SET);

    const size_t   OUT_CHUNK = 4096;
    unsigned char *dict      = (unsigned char *)malloc(32768);
    uint8_t       *outbuf    = (uint8_t *)malloc(OUT_CHUNK);
    if (!dict || !outbuf) {
        if (dict)   free(dict);
        if (outbuf) free(outbuf);
        fclose(g_idf_gz_fp); g_idf_gz_fp = nullptr;
        errorOut = "malloc dict/outbuf failed"; return false;
    }

    s_ota_started   = false;
    s_ota_written   = 0;
    s_ota_total_est = (int)(fsz * 3 / 2);

    TINF_DATA d = {};
    uzlib_init();
    d.readSourceByte = idf_uzlib_read_byte;
    if (uzlib_gzip_parse_header(&d) != TINF_OK) {
        free(dict); free(outbuf);
        fclose(g_idf_gz_fp); g_idf_gz_fp = nullptr;
        errorOut = "gzip header parse failed"; return false;
    }
    uzlib_uncompress_init(&d, dict, 32768);

    bool ok  = true;
    int  ret = TINF_OK;
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
    free(outbuf); free(dict);
    fclose(g_idf_gz_fp); g_idf_gz_fp = nullptr;

    if (!ok || !s_ota_started) {
        if (s_ota_started) esp_ota_abort(s_ota_handle);
        if (errorOut.empty()) errorOut = "gz decompression/flash failed";
        return false;
    }
    if (esp_ota_end(s_ota_handle) != ESP_OK)              { errorOut = "esp_ota_end failed";        return false; }
    if (esp_ota_set_boot_partition(s_ota_part) != ESP_OK) { errorOut = "set_boot_partition failed"; return false; }
    return true;
}

// ── Remote gz OTA ──────────────────────────────────────────────────────────────
bool performGzOtaUpdate(std::string &errorOut) {
    otaInProgress = true;
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

    // Download .bin.gz to a temp file on flash
    const char *tmpPath = "/data/ota_tmp.bin.gz";
    FILE *fout = fopen(tmpPath, "wb");
    if (!fout) {
        esp_http_client_close(client); esp_http_client_cleanup(client);
        otaInProgress = false; errorOut = "tmpfile open failed"; broadcastOtaStatus("error", errorOut, -1); return false;
    }
    char buf[2048]; int rd;
    while ((rd = esp_http_client_read(client, buf, sizeof(buf))) > 0)
        fwrite(buf, 1, rd, fout);
    fclose(fout);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (!idf_decompressGzAndFlash(tmpPath, errorOut)) {
        remove(tmpPath);
        otaInProgress = false;
        broadcastOtaStatus("error", errorOut, -1);
        return false;
    }
    remove(tmpPath);
    otaInProgress = false;
    broadcastOtaStatus("progress", "", 100);
    broadcastOtaStatus("success", "OTA update successful", -1);
    return true;
}

// ── Local firmware upload handler (esp_http_server) ───────────────────────────
esp_err_t handleOtaUpload(httpd_req_t *req) {
    otaInProgress = true;
    broadcastOtaStatus("start", "Local OTA upload started", -1);

    int total = req->content_len;
    char buf[2048];

    // Peek first 2 bytes to detect gzip magic (0x1f 0x8b)
    uint8_t magic[2] = {0, 0};
    {
        int r = httpd_req_recv(req, (char *)magic, 2);
        if (r < 2) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload too short");
            otaInProgress = false; return ESP_FAIL;
        }
    }
    bool isGzip = (magic[0] == 0x1f && magic[1] == 0x8b);

    if (!isGzip) {
        // ── Plain .bin: write directly via esp_ota_ops ──────────────────
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
        if (esp_ota_write(ota_handle, magic, 2) != ESP_OK) {
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota_write error");
            otaInProgress = false; return ESP_FAIL;
        }
        int remaining = (total > 0) ? total - 2 : total;
        int written_total = 2;
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
            ota_wdt_reset();
        }
        if (esp_ota_end(ota_handle) != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota_end failed");
            otaInProgress = false; return ESP_FAIL;
        }
        if (esp_ota_set_boot_partition(ota_part) != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "set_boot failed");
            otaInProgress = false; return ESP_FAIL;
        }
    } else {
        // ── .bin.gz: save to tmp file, decompress via uzlib, flash ───────
        const char *tmpPath = "/data/ota_up.bin.gz";
        {
            FILE *fout = fopen(tmpPath, "wb");
            if (!fout) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "tmpfile open failed");
                otaInProgress = false; return ESP_FAIL;
            }
            fwrite(magic, 1, 2, fout);
            int remaining = (total > 0) ? total - 2 : -1;
            int totalRecv = 2;
            int lastPct = -1;
            while (remaining != 0) {
                int toRead = (remaining > 0) ? MIN((int)sizeof(buf), remaining) : (int)sizeof(buf);
                int recv = httpd_req_recv(req, buf, toRead);
                if (recv < 0) {
                    if (recv == HTTPD_SOCK_ERR_TIMEOUT) continue;
                    fclose(fout); remove(tmpPath);
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive error");
                    otaInProgress = false; return ESP_FAIL;
                }
                if (recv == 0) break;
                fwrite(buf, 1, recv, fout);
                totalRecv += recv;
                if (total > 0) {
                    remaining -= recv;
                    int pct = totalRecv * 45 / total;
                    if (pct != lastPct) { broadcastOtaStatus("progress", "", pct); lastPct = pct; }
                }
                ota_wdt_reset();
            }
            fclose(fout);
            ESP_LOGI(TAG, "gz upload saved %d bytes", totalRecv);
        }

        std::string decompErr;
        if (!idf_decompressGzAndFlash(tmpPath, decompErr)) {
            remove(tmpPath);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                decompErr.empty() ? "gz decompress/flash failed" : decompErr.c_str());
            otaInProgress = false; return ESP_FAIL;
        }
        remove(tmpPath);
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

// ── Arduino-only includes ──────────────────────────────────────────────────────
#include <Arduino.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include "esp_wifi.h"

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

// ── Decompress .gz from LittleFS and flash via Update (progress 45–99%) ─────────
static bool arduino_flashGzFromLittleFS(const char *path, std::string &errorOut) {
    g_ota_file = LittleFS.open(path, "r");
    if (!g_ota_file) { errorOut = "LittleFS open for read failed"; return false; }
    int fileSize = (int)g_ota_file.size();
    Serial.printf("[ota] Decompressing %d bytes...\n", fileSize);

    unsigned char *dict   = (unsigned char*)malloc(32768);
    uint8_t       *outbuf = (uint8_t*)malloc(4096);
    if (!dict || !outbuf) {
        if (dict)   free(dict);
        if (outbuf) free(outbuf);
        g_ota_file.close();
        errorOut = "malloc failed"; return false;
    }

    TINF_DATA d;
    memset(&d, 0, sizeof(d));
    d.readSourceByte = uzlib_file_read_byte;
    d.log            = ota_uzlib_log;
    uzlib_init();

    if (uzlib_gzip_parse_header(&d) != TINF_OK) {
        free(dict); free(outbuf); g_ota_file.close();
        errorOut = "gzip header parse failed"; return false;
    }
    uzlib_uncompress_init(&d, dict, 32768);

    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        free(dict); free(outbuf); g_ota_file.close();
        errorOut = "Update.begin failed: " + std::to_string((int)Update.getError());
        return false;
    }

    size_t totalOut = 0;
    int    lastPct  = -1;
    int    ret      = TINF_OK;
    while (true) {
        d.dest          = outbuf;
        d.destStart     = outbuf;
        d.destSize      = 4096;
        d.destRemaining = 4096;
        ret = uzlib_uncompress(&d);
        size_t produced = (size_t)(d.dest - outbuf);
        if (produced > 0) {
            if (Update.write(outbuf, produced) != produced) {
                errorOut = "Update.write failed";
                Update.abort(); free(dict); free(outbuf); g_ota_file.close();
                return false;
            }
            totalOut += produced;
        }
        if (ret == TINF_DONE) break;
        if (ret < 0) {
            errorOut = "decompress error: " + std::to_string(ret);
            Update.abort(); free(dict); free(outbuf); g_ota_file.close();
            return false;
        }
        if (fileSize > 0) {
            int pct = 45 + ((int)g_ota_file.position() * 54 / fileSize);
            if (pct > 99) pct = 99;
            if (pct != lastPct) { broadcastOtaStatus("progress", "", pct); lastPct = pct; }
        }
        ota_wdt_reset();
    }
    free(dict); free(outbuf); g_ota_file.close();
    Serial.printf("[ota] decompressed %u bytes\n", totalOut);

    if (!Update.end(true) || !Update.isFinished()) {
        errorOut = std::string("Update.end failed: ") + std::to_string((int)Update.getError())
                   + " (" + Update.errorString() + ")";
        return false;
    }
    return true;
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
            ota_wdt_reset();
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

    // ── Phase 2: Decompress and flash ─────────────────────────────────────
    if (!arduino_flashGzFromLittleFS(tmpPath, errorOut)) {
        LittleFS.remove(tmpPath);
        otaInProgress = false;
        return false;
    }
    LittleFS.remove(tmpPath);
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
    char buf[2048];

    // Peek first 2 bytes to detect gzip magic (0x1f 0x8b)
    uint8_t magic[2] = {0, 0};
    {
        int r = httpd_req_recv(req, (char *)magic, 2);
        if (r < 2) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload too short");
            otaInProgress = false;
            return ESP_FAIL;
        }
    }
    bool isGzip = (magic[0] == 0x1f && magic[1] == 0x8b);

    if (!isGzip) {
        // ── Plain .bin: Update library path ────────────────────────────────
        if (!Update.begin(total > 0 ? (size_t)total : UPDATE_SIZE_UNKNOWN)) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Update.begin failed");
            otaInProgress = false;
            return ESP_FAIL;
        }
        if (Update.write(magic, 2) != 2) {
            Update.abort();
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota_write error");
            otaInProgress = false;
            return ESP_FAIL;
        }
        int remaining = (total > 0) ? total - 2 : -1;
        int written_total = 2;
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
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota_write error");
                otaInProgress = false;
                return ESP_FAIL;
            }
            written_total += recv;
            if (total > 0) remaining -= recv;
            if (total > 0) {
                int pct = written_total * 100 / total;
                if (pct != lastPct) { broadcastOtaStatus("progress", "", pct); lastPct = pct; }
            }
            ota_wdt_reset();
        }
        if (!Update.end(true)) {
            char err[64];
            snprintf(err, sizeof(err), "Update.end failed: %d", (int)Update.getError());
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            otaInProgress = false;
            return ESP_FAIL;
        }
    } else {
        // ── .bin.gz: save to LittleFS, decompress via uzlib, flash ─────────
        const char *tmpPath = "/ota_up.gz";
        {
            File f = LittleFS.open(tmpPath, "w");
            if (!f) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "LittleFS open failed");
                otaInProgress = false;
                return ESP_FAIL;
            }
            f.write(magic, 2);
            int remaining = (total > 0) ? total - 2 : -1;
            int totalRecv = 2;
            int lastPct = -1;
            while ((total < 0) || (remaining > 0)) {
                int recv = httpd_req_recv(req, buf, sizeof(buf));
                if (recv < 0) {
                    if (recv == HTTPD_SOCK_ERR_TIMEOUT) continue;
                    f.close(); LittleFS.remove(tmpPath);
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive error");
                    otaInProgress = false;
                    return ESP_FAIL;
                }
                if (recv == 0) break;
                f.write((uint8_t *)buf, recv);
                totalRecv += recv;
                if (total > 0) {
                    remaining -= recv;
                    int pct = totalRecv * 45 / total;
                    if (pct != lastPct) { broadcastOtaStatus("progress", "", pct); lastPct = pct; }
                }
                ota_wdt_reset();
            }
            f.close();
            Serial.printf("[ota] gz upload saved %d bytes\n", totalRecv);
        }

        std::string decompErr;
        if (!arduino_flashGzFromLittleFS(tmpPath, decompErr)) {
            LittleFS.remove(tmpPath);
            char err[128]; snprintf(err, sizeof(err), "%s", decompErr.c_str());
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            otaInProgress = false;
            return ESP_FAIL;
        }
        LittleFS.remove(tmpPath);
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
