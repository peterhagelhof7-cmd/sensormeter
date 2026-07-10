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
// /api/config), OTA-Update per lokalem .bin-Upload (kein HTTPS-Client,
// siehe docs/entscheidungen.md fuer den Flash-Budget-Hintergrund).
// Async (non-blocking) per AsyncWebServer, siehe Pflichtenheft Abschnitt 5+8
// - Ausnahme: OTA-Flash ist eine admin-ausgeloeste Einzelaktion und blockiert
// kurzzeitig. Der WLAN-Scan (/api/wifi/scan) laeuft dagegen bewusst NICHT
// blockierend (WiFi.scanNetworks(true) + Polling durch die Seite) - siehe
// docs/entscheidungen.md.

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
  void handleApiWifiConnect(AsyncWebServerRequest* request);
  // scope=settings: nur config.xml auf Defaults; scope=all: zusaetzlich
  // /history.csv loeschen.
  void handleApiFactoryReset(AsyncWebServerRequest* request);
  // Eigener Button "IP-Einstellungen uebernehmen & neu starten" pro
  // Interface (iface=lan|wlan, getrennt vom allgemeinen "Speichern"-Button
  // und von handleApiWifiConnect()): prueft die Erreichbarkeit VOR dem
  // Uebernehmen - bei DHCP per echtem Lease-Test, bei statischer IP per
  // Ping (ipRespondsToPing()) - und speichert + startet neu nur bei Erfolg.
  void handleApiNetworkApply(AsyncWebServerRequest* request);

  String buildPageShell(const String& title, const String& bodyContent) const;
  String buildMainPageBody() const;
  String buildSettingsPageBody() const;
};
