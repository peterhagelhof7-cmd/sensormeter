#pragma once

#include <Arduino.h>

// GitHub-Versionscheck + OTA-Flash (siehe docs/entscheidungen.md fuer die
// Design-Entscheidung: Update-Button direkt aus GitHub-Release ODER
// lokaler .bin-Upload, beide ueber Update.h in die freie OTA-Partition).
//
// Sicherheitshinweis (siehe docs/entscheidungen.md): TLS-Zertifikatspruefung
// ist bewusst deaktiviert (WiFiClientSecure::setInsecure), um keine
// Root-CA-Pins pflegen zu muessen, die bei Rotation brechen wuerden. Als
// Ausgleich sind alle OTA-Endpunkte im Webserver hinter demselben
// Passwort-Schutz wie die Einstellungsseite.

struct OtaCheckResult {
  bool success = false;
  String latestVersion;
  String assetUrl;
  bool updateAvailable = false;
};

class OtaManager {
 public:
  // Fragt die GitHub-Releases-API ab (siehe OTA_GITHUB_OWNER/OTA_GITHUB_REPO
  // in config.h) und vergleicht mit DEVICE_FIRMWARE_VERSION.
  OtaCheckResult checkLatestRelease();

  // Laedt ein Firmware-Binary von einer URL (typischerweise assetUrl aus
  // checkLatestRelease) und flasht es. true = Flash erfolgreich, Reboot noetig.
  bool installFromUrl(const String& binUrl, String& errorOut);

  // Fuer den lokalen Upload (Streaming-Callback aus WebServerManager).
  bool beginLocalUpdate(size_t contentLength);
  bool writeLocalUpdateChunk(uint8_t* data, size_t len);
  bool endLocalUpdate();
};
