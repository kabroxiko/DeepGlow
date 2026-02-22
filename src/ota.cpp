#define DEEPGLOW_REPO_URL "https://github.com/kabroxiko/DeepGlow"

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
