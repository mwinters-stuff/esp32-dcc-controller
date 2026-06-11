#include "ScreenshotHttp.h"
#include "Screenshot.h"

#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_spiffs.h>

#include <lvgl.h>

#include <cstdio>
#include <cstring>
#include <dirent.h>

namespace utilities {
namespace {

static const char *TAG = "ScreenshotHttp";
static httpd_handle_t s_httpd = nullptr;

static void queueScreenshotCapture(void *) {
  if (!saveActiveScreenScreenshot()) {
    ESP_LOGE(TAG, "Screenshot capture failed");
  }
}

static esp_err_t screenshot_status_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");

  const CaptureStatus status = getCaptureStatus();
  const char *statusStr = (status == CaptureStatus::InProgress) ? "in_progress" : "ready";

  char response[128];
  std::snprintf(response, sizeof(response), "{\"status\":\"%s\"}", statusStr);

  httpd_resp_sendstr(req, response);
  return ESP_OK;
}

static esp_err_t screenshot_index_handler(httpd_req_t *req) {
  static const char kHtml[] =
      "<!doctype html>\n"
      "<html lang=\"en\">\n"
      "<head>\n"
      "  <meta charset=\"utf-8\">\n"
      "  <meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
      "  <title>Screenshot Tools</title>\n"
      "  <style>\n"
      "    :root { --bg: #0f172a; --panel: #111827; --text: #e5e7eb; --btn: #1f2937; --btnh: #374151; --success: "
      "#10b981; "
      "--disabled: #4b5563; }\n"
      "    * { box-sizing: border-box; }\n"
      "    body { margin: 0; min-height: 100vh; display: grid; place-items: center; background: radial-gradient(circle "
      "at top, #1e293b, var(--bg)); color: var(--text); font-family: sans-serif; }\n"
      "    .card { width: min(92vw, 360px); background: rgba(17,24,39,0.85); border: 1px solid #374151; border-radius: "
      "14px; padding: 18px; }\n"
      "    h1 { margin: 0 0 14px; font-size: 1.2rem; text-align: center; }\n"
      "    .buttons { display: grid; gap: 10px; }\n"
      "    button, a { display: block; width: 100%; text-align: center; text-decoration: none; color: var(--text); "
      "background: var(--btn); border: 1px solid #4b5563; border-radius: 10px; padding: 12px 10px; font-size: 1rem; "
      "cursor: pointer; transition: background 0.2s; }\n"
      "    button:hover:not(:disabled), a:hover { background: var(--btnh); }\n"
      "    button:disabled { background: var(--disabled); cursor: not-allowed; opacity: 0.6; }\n"
      "    #status { margin-top: 10px; min-height: 1.4em; font-size: 0.9rem; text-align: center; color: #cbd5e1; "
      "transition: color 0.3s; }\n"
      "    #status.success { color: var(--success); }\n"
      "    #status.working { color: #f59e0b; }\n"
      "  </style>\n"
      "</head>\n"
      "<body>\n"
      "  <main class=\"card\">\n"
      "    <h1>Screenshot Tools</h1>\n"
      "    <div class=\"buttons\">\n"
      "      <button id=\"takeBtn\" type=\"button\">Take Screenshot</button>\n"
      "      <a id=\"downloadLink\" href=\"/screenshot_latest.ppm\" download=\"screenshot_latest.ppm\" "
      "style=\"pointer-events: none; opacity: 0.6;\">Download Screenshot</a>\n"
      "      <button id=\"deleteBtn\" type=\"button\">Delete Screenshots</button>\n"
      "    </div>\n"
      "    <div id=\"status\"></div>\n"
      "  </main>\n"
      "  <script>\n"
      "    const takeBtn = document.getElementById('takeBtn');\n"
      "    const downloadLink = document.getElementById('downloadLink');\n"
      "    const statusEl = document.getElementById('status');\n"
      "    let pollInterval = null;\n"
      "    let hasScreenshot = false;\n"
      "\n"
      "    async function checkCaptureStatus() {\n"
      "      try {\n"
      "        const res = await fetch('/screenshot_status', { method: 'GET', cache: 'no-store' });\n"
      "        const data = await res.json();\n"
      "        const isInProgress = data.status === 'in_progress';\n"
      "        \n"
      "        takeBtn.disabled = isInProgress;\n"
      "        \n"
      "        if (isInProgress) {\n"
      "          statusEl.textContent = 'Capturing...';\n"
      "          statusEl.className = 'working';\n"
      "        } else if (pollInterval !== null) {\n"
      "          statusEl.textContent = 'Complete!';\n"
      "          statusEl.className = 'success';\n"
      "          clearInterval(pollInterval);\n"
      "          pollInterval = null;\n"
      "          hasScreenshot = true;\n"
      "          updateDownloadLink();\n"
      "          setTimeout(() => { statusEl.textContent = ''; statusEl.className = ''; }, 2000);\n"
      "        }\n"
      "      } catch (e) {\n"
      "        statusEl.textContent = 'Error checking status';\n"
      "      }\n"
      "    }\n"
      "\n"
      "    function updateDownloadLink() {\n"
      "      if (hasScreenshot) {\n"
      "        downloadLink.style.pointerEvents = 'auto';\n"
      "        downloadLink.style.opacity = '1';\n"
      "      } else {\n"
      "        downloadLink.style.pointerEvents = 'none';\n"
      "        downloadLink.style.opacity = '0.6';\n"
      "      }\n"
      "    }\n"
      "\n"
      "    async function takeScreenshot() {\n"
      "      takeBtn.disabled = true;\n"
      "      statusEl.textContent = 'Queuing...';\n"
      "      statusEl.className = 'working';\n"
      "      try {\n"
      "        await fetch('/screenshot_take', { method: 'GET', cache: 'no-store' });\n"
      "        if (pollInterval === null) {\n"
      "          pollInterval = setInterval(checkCaptureStatus, 200);\n"
      "        }\n"
      "        checkCaptureStatus();\n"
      "      } catch (e) {\n"
      "        statusEl.textContent = 'Request failed';\n"
      "        takeBtn.disabled = false;\n"
      "      }\n"
      "    }\n"
      "\n"
      "    async function deleteScreenshots() {\n"
      "      if (!confirm('Delete all screenshots?')) return;\n"
      "      statusEl.textContent = 'Deleting...';\n"
      "      statusEl.className = 'working';\n"
      "      try {\n"
      "        const res = await fetch('/screenshot_cleanup?all=1', { method: 'GET', cache: 'no-store' });\n"
      "        const txt = await res.text();\n"
      "        statusEl.textContent = txt || 'Deleted';\n"
      "        hasScreenshot = false;\n"
      "        updateDownloadLink();\n"
      "        setTimeout(() => { statusEl.textContent = ''; statusEl.className = ''; }, 2000);\n"
      "      } catch (e) {\n"
      "        statusEl.textContent = 'Delete failed';\n"
      "      }\n"
      "    }\n"
      "\n"
      "    takeBtn.addEventListener('click', takeScreenshot);\n"
      "    document.getElementById('deleteBtn').addEventListener('click', deleteScreenshots);\n"
      "\n"
      "    window.addEventListener('load', () => {\n"
      "      checkCaptureStatus();\n"
      "    });\n"
      "  </script>\n"
      "</body>\n"
      "</html>\n";

  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_send(req, kHtml, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t screenshot_take_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  lv_async_call(queueScreenshotCapture, nullptr);
  httpd_resp_sendstr(req, "Screenshot queued.");
  return ESP_OK;
}

static esp_err_t screenshot_get_handler(httpd_req_t *req) {
  const char *path = "/spiffs/screenshot_latest.ppm";
  if (!ensureScreenshotStorageMounted()) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Screenshot storage is unavailable.");
    return ESP_OK;
  }
  FILE *f = std::fopen(path, "rb");
  if (!f) {
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "No screenshot yet. Trigger a screenshot first.");
    return ESP_OK;
  }

  httpd_resp_set_type(req, "image/x-portable-pixmap");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=\"screenshot_latest.ppm\"");

  char chunk[1024];
  while (true) {
    const size_t n = std::fread(chunk, 1, sizeof(chunk), f);
    if (n > 0) {
      const esp_err_t sendErr = httpd_resp_send_chunk(req, chunk, n);
      if (sendErr != ESP_OK) {
        std::fclose(f);
        return sendErr;
      }
    }

    if (n < sizeof(chunk)) {
      break;
    }
  }

  std::fclose(f);
  return httpd_resp_send_chunk(req, nullptr, 0);
}

static esp_err_t screenshot_cleanup_handler(httpd_req_t *req) {
  const char *latestPath = "/spiffs/screenshot_latest.ppm";
  const char *tmpPath = "/spiffs/screenshot_latest.tmp";
  const char *spiffsDir = "/spiffs";

  if (!ensureScreenshotStorageMounted()) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Screenshot storage is unavailable.");
    return ESP_OK;
  }

  // Log SPIFFS status for diagnostics
  size_t spiffsUsed = 0;
  size_t spiffsTotal = 0;
  if (esp_spiffs_info("spiffs", &spiffsTotal, &spiffsUsed) == ESP_OK) {
    ESP_LOGI(TAG, "SPIFFS before cleanup: used=%u bytes, total=%u bytes, free=%u bytes",
             static_cast<unsigned>(spiffsUsed), static_cast<unsigned>(spiffsTotal),
             static_cast<unsigned>(spiffsTotal - spiffsUsed));
  }

  int removedCount = 0;

  // Remove temp file if it exists
  if (std::remove(tmpPath) == 0) {
    removedCount++;
    ESP_LOGI(TAG, "Deleted %s", tmpPath);
  }

  // Scan directory for old timestamped screenshot files
  DIR *dir = opendir(spiffsDir);
  if (dir) {
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
      // Match old pattern: screenshot_<digits>.ppm (but not screenshot_latest.ppm)
      if (std::strncmp(entry->d_name, "screenshot_", 11) == 0 &&
          std::strcmp(entry->d_name, "screenshot_latest.ppm") != 0) {
        // Check if it ends with .ppm
        const size_t len = std::strlen(entry->d_name);
        if (len > 4 && std::strcmp(entry->d_name + len - 4, ".ppm") == 0) {
          char fullPath[512];
          std::snprintf(fullPath, sizeof(fullPath), "%s/%s", spiffsDir, entry->d_name);

          if (std::remove(fullPath) == 0) {
            removedCount++;
            ESP_LOGI(TAG, "Deleted %s", fullPath);
          } else {
            ESP_LOGW(TAG, "Failed to delete %s", fullPath);
          }
        }
      }
    }
    closedir(dir);
  } else {
    ESP_LOGW(TAG, "Failed to open %s directory", spiffsDir);
  }

  // Remove current latest if requested (user can add ?all=1)
  const char *queryStr = strchr(req->uri, '?');
  if (queryStr && std::strstr(queryStr, "all=1")) {
    if (std::remove(latestPath) == 0) {
      removedCount++;
      ESP_LOGI(TAG, "Deleted %s", latestPath);
    }
  }

  // Log SPIFFS status after cleanup
  if (esp_spiffs_info("spiffs", &spiffsTotal, &spiffsUsed) == ESP_OK) {
    ESP_LOGI(TAG, "SPIFFS after cleanup: used=%u bytes, total=%u bytes, free=%u bytes",
             static_cast<unsigned>(spiffsUsed), static_cast<unsigned>(spiffsTotal),
             static_cast<unsigned>(spiffsTotal - spiffsUsed));
  }

  char response[256];
  std::snprintf(response, sizeof(response), "Deleted %d old screenshot(s). Use ?all=1 to also delete latest.",
                removedCount);

  httpd_resp_set_type(req, "text/plain");
  httpd_resp_sendstr(req, response);
  return ESP_OK;
}

} // namespace

void startScreenshotHttpServer() {
  if (s_httpd != nullptr) {
    return;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 8080;
  config.ctrl_port = 32768;
  config.max_uri_handlers = 5;

  const esp_err_t startErr = httpd_start(&s_httpd, &config);
  if (startErr != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(startErr));
    s_httpd = nullptr;
    return;
  }

  httpd_uri_t indexUri = {};
  indexUri.uri = "/";
  indexUri.method = HTTP_GET;
  indexUri.handler = screenshot_index_handler;
  indexUri.user_ctx = nullptr;

  const esp_err_t indexErr = httpd_register_uri_handler(s_httpd, &indexUri);
  if (indexErr != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register index endpoint: %s", esp_err_to_name(indexErr));
    httpd_stop(s_httpd);
    s_httpd = nullptr;
    return;
  }

  httpd_uri_t statusUri = {};
  statusUri.uri = "/screenshot_status";
  statusUri.method = HTTP_GET;
  statusUri.handler = screenshot_status_handler;
  statusUri.user_ctx = nullptr;

  const esp_err_t statusErr = httpd_register_uri_handler(s_httpd, &statusUri);
  if (statusErr != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register status endpoint: %s", esp_err_to_name(statusErr));
    httpd_stop(s_httpd);
    s_httpd = nullptr;
    return;
  }

  httpd_uri_t takeUri = {};
  takeUri.uri = "/screenshot_take";
  takeUri.method = HTTP_GET;
  takeUri.handler = screenshot_take_handler;
  takeUri.user_ctx = nullptr;

  const esp_err_t takeErr = httpd_register_uri_handler(s_httpd, &takeUri);
  if (takeErr != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register take endpoint: %s", esp_err_to_name(takeErr));
    httpd_stop(s_httpd);
    s_httpd = nullptr;
    return;
  }

  httpd_uri_t screenshotUri = {};
  screenshotUri.uri = "/screenshot_latest.ppm";
  screenshotUri.method = HTTP_GET;
  screenshotUri.handler = screenshot_get_handler;
  screenshotUri.user_ctx = nullptr;

  const esp_err_t regErr = httpd_register_uri_handler(s_httpd, &screenshotUri);
  if (regErr != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register screenshot endpoint: %s", esp_err_to_name(regErr));
    httpd_stop(s_httpd);
    s_httpd = nullptr;
    return;
  }

  httpd_uri_t cleanupUri = {};
  cleanupUri.uri = "/screenshot_cleanup";
  cleanupUri.method = HTTP_GET;
  cleanupUri.handler = screenshot_cleanup_handler;
  cleanupUri.user_ctx = nullptr;

  const esp_err_t cleanupErr = httpd_register_uri_handler(s_httpd, &cleanupUri);
  if (cleanupErr != ESP_OK) {
    ESP_LOGW(TAG, "Failed to register cleanup endpoint: %s", esp_err_to_name(cleanupErr));
  }

  ESP_LOGI(TAG, "Screenshot UI ready on port 8080: /");
  ESP_LOGI(TAG, "Screenshot take endpoint ready: /screenshot_take");
  ESP_LOGI(TAG, "Screenshot status endpoint ready: /screenshot_status");
  ESP_LOGI(TAG, "Screenshot HTTP endpoint ready on port 8080: /screenshot_latest.ppm");
  ESP_LOGI(TAG, "Screenshot cleanup endpoint ready: /screenshot_cleanup");
}

void stopScreenshotHttpServer() {
  if (s_httpd == nullptr) {
    return;
  }

  httpd_stop(s_httpd);
  s_httpd = nullptr;
  ESP_LOGI(TAG, "Screenshot HTTP server stopped");
}

} // namespace utilities
