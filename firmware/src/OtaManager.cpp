#include "OtaManager.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>

#if __has_include("config.h")
#include "config.h"
#endif

#ifndef OTA_GITHUB_OWNER
#define OTA_GITHUB_OWNER "peterhagelhof7-cmd"
#endif
#ifndef OTA_GITHUB_REPO
#define OTA_GITHUB_REPO "sensormeter"
#endif
#ifndef DEVICE_FIRMWARE_VERSION
#define DEVICE_FIRMWARE_VERSION "0.0.0"
#endif

// Release-Asset muss bei jedem GitHub-Release exakt so heissen.
static const char* RELEASE_ASSET_NAME = "firmware.bin";

OtaCheckResult OtaManager::checkLatestRelease() {
  OtaCheckResult result;

  WiFiClientSecure client;
  client.setInsecure();  // siehe OtaManager.h / docs/entscheidungen.md

  HTTPClient http;
  String url = String("https://api.github.com/repos/") + OTA_GITHUB_OWNER + "/" + OTA_GITHUB_REPO + "/releases/latest";
  if (!http.begin(client, url)) {
    result.latestVersion = "Verbindung fehlgeschlagen";
    return result;
  }
  http.addHeader("User-Agent", "sensormeter-firmware");
  http.addHeader("Accept", "application/vnd.github+json");

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    result.latestVersion = "HTTP " + String(httpCode);
    http.end();
    return result;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) {
    result.latestVersion = "JSON-Fehler";
    return result;
  }

  result.latestVersion = doc["tag_name"] | "";
  for (JsonObject asset : doc["assets"].as<JsonArray>()) {
    String name = asset["name"] | "";
    if (name == RELEASE_ASSET_NAME) {
      result.assetUrl = asset["browser_download_url"] | "";
      break;
    }
  }

  // Vereinfachter Vergleich (keine Semver-Ordnung): jeder abweichende
  // Tag-Name gilt als "Update verfuegbar". Fuer ein Solo-Hobbyprojekt
  // ausreichend, siehe docs/entscheidungen.md.
  String current = DEVICE_FIRMWARE_VERSION;
  result.updateAvailable = result.latestVersion.length() > 0 && result.assetUrl.length() > 0 &&
                            result.latestVersion != current;
  result.success = true;
  return result;
}

bool OtaManager::installFromUrl(const String& binUrl, String& errorOut) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, binUrl)) {
    errorOut = "Verbindung zum Release-Asset fehlgeschlagen";
    return false;
  }
  http.addHeader("User-Agent", "sensormeter-firmware");

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    errorOut = "HTTP " + String(httpCode);
    http.end();
    return false;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0) {
    errorOut = "Unbekannte Dateigroesse";
    http.end();
    return false;
  }

  if (!Update.begin(contentLength)) {
    errorOut = "Zu wenig Platz fuer Update: " + String(Update.errorString());
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t written = Update.writeStream(*stream);
  http.end();

  if (written != (size_t)contentLength) {
    errorOut = "Uebertragung unvollstaendig (" + String(written) + "/" + String(contentLength) + ")";
    Update.abort();
    return false;
  }

  if (!Update.end(true)) {
    errorOut = "Update.end() fehlgeschlagen: " + String(Update.errorString());
    return false;
  }

  return true;
}

bool OtaManager::beginLocalUpdate(size_t contentLength) {
  return Update.begin(contentLength);
}

bool OtaManager::writeLocalUpdateChunk(uint8_t* data, size_t len) {
  return Update.write(data, len) == len;
}

bool OtaManager::endLocalUpdate() {
  return Update.end(true);
}
