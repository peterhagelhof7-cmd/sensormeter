#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "ConfigManager.h"
#include "DataManager.h"
#include "NetworkManager.h"
#include "OtaManager.h"

// Webserver (Pflichtenheft "WebServerTask"): Hauptseite (Status, Graph,
// Syslog-Tabelle, CSV-Download), passwortgeschuetzte Einstellungsseite,
// REST-API (/api/status, /api/sensors, /api/network, /api/logs,
// /api/config), OTA-Update (Versionscheck + GitHub-Release/lokaler Upload).
// Async (non-blocking) per AsyncWebServer, siehe Pflichtenheft Abschnitt 5+8
// - Ausnahmen: WLAN-Scan und OTA-Flash sind admin-ausgeloeste Einzelaktionen
// und blockieren kurzzeitig (siehe docs/entscheidungen.md).

class WebServerManager {
 public:
  WebServerManager(DataManager& dataManager, ConfigManager& configManager,
                    NetworkManager& networkManager, OtaManager& otaManager);

  void begin();

 private:
  DataManager& _data;
  ConfigManager& _config;
  NetworkManager& _network;
  OtaManager& _ota;

  AsyncWebServer _server;

  // Streaming-Zustand fuer den lokalen .bin-Upload (siehe /api/ota/upload).
  bool _otaInProgress = false;
  bool _otaSuccess = false;

  // Streaming-Puffer fuer den XML-Import (config.xml ist klein genug, um
  // komplett im RAM zwischengehalten zu werden).
  String _importBuffer;

  bool checkAuth(AsyncWebServerRequest* request);

  void handleRoot(AsyncWebServerRequest* request);
  void handleSettingsPage(AsyncWebServerRequest* request);
  void handleValuesCsv(AsyncWebServerRequest* request);

  void handleApiStatus(AsyncWebServerRequest* request);
  void handleApiSensors(AsyncWebServerRequest* request);
  void handleApiNetwork(AsyncWebServerRequest* request);
  void handleApiLogs(AsyncWebServerRequest* request);
  void handleApiGraph(AsyncWebServerRequest* request);

  void handleApiConfigGet(AsyncWebServerRequest* request);
  void handleApiConfigPost(AsyncWebServerRequest* request);
  void handleApiConfigExport(AsyncWebServerRequest* request);
  void handleApiConfigImportUpload(AsyncWebServerRequest* request, const String& filename, size_t index,
                                    uint8_t* data, size_t len, bool final);

  void handleApiReboot(AsyncWebServerRequest* request);
  void handleApiWifiScan(AsyncWebServerRequest* request);

  void handleApiOtaCheck(AsyncWebServerRequest* request);
  void handleApiOtaGithub(AsyncWebServerRequest* request);

  String buildPageShell(const String& title, const String& bodyContent) const;
  String buildMainPageBody() const;
  String buildSettingsPageBody() const;
};
