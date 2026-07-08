#include "WebServerManager.h"

#include <ArduinoJson.h>
#include <Update.h>
#include <esp_timer.h>
#include <time.h>
#include "TimeUtils.h"

#if __has_include("config.h")
#include "config.h"
#endif
#ifndef DEVICE_FIRMWARE_VERSION
#define DEVICE_FIRMWARE_VERSION "0.0.0"
#endif

// Nur ein statischer Link fuer den Admin-Browser, kein Geraet-seitiger
// Netzwerkzugriff - daher unproblematisch ohne HTTPS-Client (siehe
// docs/entscheidungen.md zum Wegfall des GitHub-Versionschecks).
#define GITHUB_REPO_SLUG "peterhagelhof7-cmd/sensormeter"

WebServerManager::WebServerManager(DataManager& dataManager, ConfigManager& configManager,
                                    NetworkManager& networkManager, OtaManager& otaManager)
    : _data(dataManager), _config(configManager), _network(networkManager), _ota(otaManager), _server(80) {}

bool WebServerManager::checkAuth(AsyncWebServerRequest* request) {
  if (!request->authenticate("admin", _config.getConfig().settingsPassword.c_str())) {
    // Fester Benutzername "admin" - Lastenheft definiert nur ein Passwort,
    // keinen Benutzernamen. Realm-Text gibt einen Hinweis im Browser-Dialog.
    request->requestAuthentication("Sensormeter (Benutzername: admin)");
    return false;
  }
  return true;
}

// ----------------------------------------------------------------------------
// Seiten-Grundgeruest (Lastenheft: schwarzer Hintergrund, weisse Schrift,
// 20pt, Rahmen um Wertebloecke, weisse Schaltflaechen mit invertierter
// Schrift, zentriert). Tabellen/Eingabefelder sind bewusst kleiner gesetzt,
// da dichte Tabellendaten bei 20pt nicht lesbar dargestellt werden koennten.
// ----------------------------------------------------------------------------
String WebServerManager::buildPageShell(const String& title, const String& bodyContent) const {
  String html;
  html.reserve(bodyContent.length() + 1200);
  html += "<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"UTF-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<title>" + title + "</title><style>";
  html += "body{background:#000;color:#fff;font-size:20pt;text-align:center;font-family:sans-serif;margin:0;padding:20px;}";
  html += "h1{font-size:1.1em;}";
  html += ".block{border:1px solid #fff;border-radius:6px;padding:14px 24px;margin:16px auto;max-width:680px;}";
  html += ".block h2{font-size:1em;margin:0 0 10px;}";
  html += ".row{display:flex;justify-content:space-between;gap:16px;margin:6px 0;text-align:left;}";
  html += "button,input[type=submit]{background:#fff;color:#000;border:none;padding:10px 20px;font-size:20pt;border-radius:6px;cursor:pointer;margin:8px;}";
  html += "table{margin:12px auto;border-collapse:collapse;font-size:14pt;}";
  html += "td,th{border:1px solid #fff;padding:6px 12px;}";
  html += "input[type=text],input[type=password]{font-size:16pt;padding:6px;width:80%;}";
  html += "label{display:block;margin-top:10px;text-align:left;max-width:420px;margin-left:auto;margin-right:auto;}";
  html += "a{color:#fff;text-decoration:none;}";
  html += "canvas{max-width:100%;background:#111;border-radius:6px;}";
  html += "#scanResult div{cursor:pointer;padding:4px;font-size:14pt;}";
  html += "#scanResult div:hover{background:#222;}";
  html += "</style></head><body>";
  html += bodyContent;
  html += "</body></html>";
  return html;
}

String WebServerManager::buildMainPageBody() const {
  const DeviceConfig& cfg = _config.getConfig();
  SensorReading s1 = _data.getSensor1();
  SensorReading s2 = _data.getSensor2();

  unsigned long uptimeSec = (unsigned long)(esp_timer_get_time() / 1000000ULL);
  char uptimeBuf[16];
  snprintf(uptimeBuf, sizeof(uptimeBuf), "%02lu:%02lu:%02lu", uptimeSec / 3600, (uptimeSec / 60) % 60, uptimeSec % 60);

  String timeStr = "--:--:--";
  if (isTimeSynced()) {
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    char buf[32];
    strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &ti);
    timeStr = buf;
  }

  String html;
  html += "<h1>" + cfg.systemName + "</h1>";

  html += "<div class=\"block\"><h2>System</h2>";
  html += "<div class=\"row\"><span>Zeit</span><span>" + timeStr + "</span></div>";
  html += "<div class=\"row\"><span>Firmware</span><span>" DEVICE_FIRMWARE_VERSION "</span></div>";
  html += "<div class=\"row\"><span>Systemtyp</span><span>" + cfg.systemType + "</span></div>";
  html += "<div class=\"row\"><span>Uptime</span><span>" + String(uptimeBuf) + "</span></div>";
  html += "<div class=\"row\"><span>Freier Heap</span><span>" + String(ESP.getFreeHeap() / 1024) + " kB</span></div>";
  html += "</div>";

  html += "<div class=\"block\"><h2>Netzwerk</h2>";
  html += "<div class=\"row\"><span>LAN IP</span><span>" + (_network.isLanUp() ? _network.getLanIp().toString() : String("-")) + "</span></div>";
  html += "<div class=\"row\"><span>LAN Link</span><span>" + String(_network.isLanLinkUp() ? "verbunden" : "getrennt") + "</span></div>";
  html += "<div class=\"row\"><span>WLAN IP</span><span>" + (_network.isWlanUp() ? _network.getWlanIp().toString() : String("-")) + "</span></div>";
  html += "<div class=\"row\"><span>WLAN SSID</span><span>" + (_network.isWlanUp() ? _network.getWlanSsid() : String("-")) + "</span></div>";
  html += "<div class=\"row\"><span>WLAN RSSI</span><span>" + (_network.isWlanUp() ? String(_network.getWlanRssi()) + " dBm" : String("-")) + "</span></div>";
  html += "</div>";

  html += "<div class=\"block\"><h2>Sensoren</h2>";
  html += "<div class=\"row\"><span>Intern</span><span>" +
          (s1.valid ? String(s1.temperature, 1) + " C / " + String(s1.humidity, 0) + " %" : String("-")) +
          "</span></div>";
  if (cfg.sensor2Enabled) {
    html += "<div class=\"row\"><span>" + cfg.sensor2Name + "</span><span>" +
            (s2.valid ? String(s2.temperature, 1) + " C / " + String(s2.humidity, 0) + " %" : String("-")) +
            "</span></div>";
  }
  html += "</div>";

  html += "<div class=\"block\"><h2>7-Tage-Verlauf</h2><canvas id=\"chart\" height=\"200\"></canvas></div>";

  html += "<div class=\"block\"><h2>Letzte Meldungen</h2><table id=\"logtable\"><tr><th>Zeit</th><th>Meldung</th></tr></table></div>";

  html += "<div class=\"block\"><a href=\"/values.csv\"><button>values.csv</button></a>";
  html += "<a href=\"/settings\"><button>Einstellungen</button></a></div>";

  html += "<script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script><script>";
  html += "fetch('/api/graph').then(r=>r.json()).then(d=>{";
  html += "new Chart(document.getElementById('chart'),{type:'line',data:{labels:d.labels,datasets:[";
  html += "{label:'Temperatur (C)',data:d.temperature,borderColor:'red',yAxisID:'y'},";
  html += "{label:'Luftfeuchte (%)',data:d.humidity,borderColor:'blue',yAxisID:'y1'}]},";
  html += "options:{scales:{y:{position:'left'},y1:{position:'right',grid:{drawOnChartArea:false}}}}});});";
  html += "fetch('/api/logs').then(r=>r.json()).then(d=>{let t=document.getElementById('logtable');";
  html += "d.entries.forEach(e=>{let r=t.insertRow();r.insertCell(0).innerText=e.time;r.insertCell(1).innerText=e.message;});});";
  html += "setInterval(()=>location.reload(),60000);";
  html += "</script>";

  return html;
}

String WebServerManager::buildSettingsPageBody() const {
  const DeviceConfig& cfg = _config.getConfig();

  String html;
  html += "<h1>Einstellungen</h1>";
  html += "<form method=\"POST\" action=\"/api/config\">";

  html += "<div class=\"block\"><h2>System</h2>";
  html += "<label>Systemname<input type=\"text\" name=\"systemName\" value=\"" + cfg.systemName + "\"></label>";
  html += "<label>Neues Passwort (leer = unveraendert)<input type=\"password\" name=\"newPassword\"></label>";
  html += "</div>";

  html += "<div class=\"block\"><h2>LAN</h2>";
  html += "<label><input type=\"checkbox\" name=\"lanDhcp\" " + String(cfg.lanDhcp ? "checked" : "") + "> DHCP</label>";
  html += "<label>IP<input type=\"text\" name=\"lanIp\" value=\"" + cfg.lanIp + "\"></label>";
  html += "<label>Netzmaske<input type=\"text\" name=\"lanMask\" value=\"" + cfg.lanMask + "\"></label>";
  html += "<label>Gateway<input type=\"text\" name=\"lanGateway\" value=\"" + cfg.lanGateway + "\"></label>";
  html += "</div>";

  html += "<div class=\"block\"><h2>WLAN</h2>";
  html += "<label><input type=\"checkbox\" name=\"wlanDhcp\" " + String(cfg.wlanDhcp ? "checked" : "") + "> DHCP</label>";
  html += "<label>SSID<input type=\"text\" name=\"wlanSsid\" id=\"wlanSsid\" value=\"" + cfg.wlanSsid + "\"></label>";
  html += "<button type=\"button\" onclick=\"scanWifi()\">SSIDs suchen</button><div id=\"scanResult\"></div>";
  html += "<label>PSK<input type=\"password\" name=\"wlanPsk\" value=\"" + cfg.wlanPsk + "\"></label>";
  html += "<label>IP<input type=\"text\" name=\"wlanIp\" value=\"" + cfg.wlanIp + "\"></label>";
  html += "<label>Netzmaske<input type=\"text\" name=\"wlanMask\" value=\"" + cfg.wlanMask + "\"></label>";
  html += "<label>Gateway<input type=\"text\" name=\"wlanGateway\" value=\"" + cfg.wlanGateway + "\"></label>";
  html += "</div>";

  html += "<div class=\"block\"><h2>Sensoren</h2>";
  html += "<label><input type=\"checkbox\" name=\"sensor2Enabled\" " + String(cfg.sensor2Enabled ? "checked" : "") +
          "> Sensor 2 (extern) aktiv</label>";
  html += "<label>Sensor-2-Name<input type=\"text\" name=\"sensor2Name\" value=\"" + cfg.sensor2Name + "\"></label>";
  html += "</div>";

  html += "<div class=\"block\"><h2>Syslog</h2>";
  html += "<label>Syslog-Server-IP<input type=\"text\" name=\"syslogServer\" value=\"" + cfg.syslogServer + "\"></label>";
  html += "</div>";

  html += "<div class=\"block\"><h2>SNMP</h2>";
  html += "<label>Community<input type=\"text\" name=\"snmpCommunity\" value=\"" + cfg.snmpCommunity + "\"></label>";
  html += "</div>";

  html += "<div class=\"block\"><input type=\"submit\" value=\"Speichern (LittleFS)\"></div>";
  html += "</form>";

  html += "<div class=\"block\"><h2>Konfiguration</h2>";
  html += "<a href=\"/api/config/export\"><button type=\"button\">XML Export</button></a>";
  html += "<form method=\"POST\" action=\"/api/config/import\" enctype=\"multipart/form-data\">";
  html += "<input type=\"file\" name=\"file\" accept=\".xml\"><input type=\"submit\" value=\"XML Import\">";
  html += "</form></div>";

  html += "<div class=\"block\"><h2>Firmware</h2>";
  html += "<form method=\"POST\" action=\"/api/ota/upload\" enctype=\"multipart/form-data\">";
  html += "<input type=\"file\" name=\"file\" accept=\".bin\"><input type=\"submit\" value=\".bin hochladen\">";
  html += "</form>";
  html += "<a href=\"https://github.com/" GITHUB_REPO_SLUG "/releases\" target=\"_blank\"><button type=\"button\">Releases auf GitHub</button></a>";
  html += "</div>";

  html += "<div class=\"block\"><form method=\"POST\" action=\"/api/reboot\" onsubmit=\"return confirm('Wirklich neu starten?')\">";
  html += "<input type=\"submit\" value=\"Reboot\"></form></div>";

  html += "<script>";
  html += "function scanWifi(){fetch('/api/wifi/scan').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('scanResult').innerHTML=d.networks.map(n=>";
  html += "`<div onclick=\"document.getElementById('wlanSsid').value='${n.ssid}'\">${n.ssid} (${n.rssi} dBm)</div>`).join('');});}";
  html += "</script>";

  return html;
}

// ----------------------------------------------------------------------------
// Seiten
// ----------------------------------------------------------------------------
void WebServerManager::handleRoot(AsyncWebServerRequest* request) {
  request->send(200, "text/html", buildPageShell(_config.getConfig().systemName, buildMainPageBody()));
}

void WebServerManager::handleSettingsPage(AsyncWebServerRequest* request) {
  if (!checkAuth(request)) return;
  request->send(200, "text/html", buildPageShell("Einstellungen", buildSettingsPageBody()));
}

void WebServerManager::handleValuesCsv(AsyncWebServerRequest* request) {
  HourValue buffer[DataManager::RINGBUFFER_SIZE];
  size_t count = _data.getRingbuffer(buffer, DataManager::RINGBUFFER_SIZE);

  String csv = "timestamp,temperature,humidity\n";
  for (size_t i = 0; i < count; i++) {
    csv += String((unsigned long)buffer[i].timestamp) + "," + String(buffer[i].temperature, 1) + "," +
           String(buffer[i].humidity, 1) + "\n";
  }

  AsyncWebServerResponse* response = request->beginResponse(200, "text/csv", csv);
  response->addHeader("Content-Disposition", "attachment; filename=values.csv");
  request->send(response);
}

// ----------------------------------------------------------------------------
// REST-API (Pflichtenheft 5.1: /api/status, /api/sensors, /api/network,
// /api/logs, /api/config)
// ----------------------------------------------------------------------------
void WebServerManager::handleApiStatus(AsyncWebServerRequest* request) {
  const DeviceConfig& cfg = _config.getConfig();

  JsonDocument doc;
  doc["systemName"] = cfg.systemName;
  doc["systemType"] = cfg.systemType;
  doc["firmwareVersion"] = DEVICE_FIRMWARE_VERSION;
  doc["uptimeSeconds"] = (unsigned long)(esp_timer_get_time() / 1000000ULL);
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["timeSynced"] = isTimeSynced();
  if (isTimeSynced()) doc["time"] = (unsigned long)time(nullptr);

  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

void WebServerManager::handleApiSensors(AsyncWebServerRequest* request) {
  const DeviceConfig& cfg = _config.getConfig();
  SensorReading s1 = _data.getSensor1();
  SensorReading s2 = _data.getSensor2();

  JsonDocument doc;
  JsonObject sensor1 = doc["sensor1"].to<JsonObject>();
  sensor1["name"] = "Intern";
  sensor1["valid"] = s1.valid;
  sensor1["temperature"] = s1.temperature;
  sensor1["humidity"] = s1.humidity;

  if (cfg.sensor2Enabled) {
    JsonObject sensor2 = doc["sensor2"].to<JsonObject>();
    sensor2["name"] = cfg.sensor2Name;
    sensor2["valid"] = s2.valid;
    sensor2["temperature"] = s2.temperature;
    sensor2["humidity"] = s2.humidity;
  }

  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

void WebServerManager::handleApiNetwork(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["lanUp"] = _network.isLanUp();
  doc["lanLinkUp"] = _network.isLanLinkUp();
  doc["lanIp"] = _network.getLanIp().toString();
  doc["lanGateway"] = _network.getLanGateway().toString();
  doc["lanDns"] = _network.getLanDns().toString();
  doc["lanMac"] = _network.getLanMac();

  doc["wlanUp"] = _network.isWlanUp();
  doc["wlanIp"] = _network.getWlanIp().toString();
  doc["wlanGateway"] = _network.getWlanGateway().toString();
  doc["wlanDns"] = _network.getWlanDns().toString();
  doc["wlanMac"] = _network.getWlanMac();
  doc["wlanSsid"] = _network.getWlanSsid();
  doc["wlanRssi"] = _network.getWlanRssi();
  doc["usingFallbackWlan"] = _network.isUsingFallbackWlan();

  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

void WebServerManager::handleApiLogs(AsyncWebServerRequest* request) {
  LogEntry entries[DataManager::LOG_CAPACITY];
  size_t count = _data.getLogEntries(entries, DataManager::LOG_CAPACITY);

  JsonDocument doc;
  JsonArray arr = doc["entries"].to<JsonArray>();
  for (size_t i = 0; i < count; i++) {
    JsonObject o = arr.add<JsonObject>();
    char buf[24];
    struct tm ti;
    localtime_r(&entries[i].timestamp, &ti);
    strftime(buf, sizeof(buf), "%d.%m. %H:%M:%S", &ti);
    o["time"] = buf;
    o["message"] = entries[i].message;
  }

  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

void WebServerManager::handleApiGraph(AsyncWebServerRequest* request) {
  HourValue buffer[DataManager::RINGBUFFER_SIZE];
  size_t count = _data.getRingbuffer(buffer, DataManager::RINGBUFFER_SIZE);

  JsonDocument doc;
  JsonArray labels = doc["labels"].to<JsonArray>();
  JsonArray temps = doc["temperature"].to<JsonArray>();
  JsonArray hums = doc["humidity"].to<JsonArray>();

  for (size_t i = 0; i < count; i++) {
    struct tm ti;
    localtime_r(&buffer[i].timestamp, &ti);
    char buf[6];
    strftime(buf, sizeof(buf), "%H:%M", &ti);
    labels.add(String(buf));
    temps.add(buffer[i].temperature);
    hums.add(buffer[i].humidity);
  }

  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

void WebServerManager::handleApiConfigGet(AsyncWebServerRequest* request) {
  if (!checkAuth(request)) return;
  const DeviceConfig& cfg = _config.getConfig();

  JsonDocument doc;
  doc["systemName"] = cfg.systemName;
  doc["systemType"] = cfg.systemType;
  doc["lanDhcp"] = cfg.lanDhcp;
  doc["lanIp"] = cfg.lanIp;
  doc["lanMask"] = cfg.lanMask;
  doc["lanGateway"] = cfg.lanGateway;
  doc["wlanDhcp"] = cfg.wlanDhcp;
  doc["wlanSsid"] = cfg.wlanSsid;
  doc["wlanIp"] = cfg.wlanIp;
  doc["wlanMask"] = cfg.wlanMask;
  doc["wlanGateway"] = cfg.wlanGateway;
  doc["sensor2Enabled"] = cfg.sensor2Enabled;
  doc["sensor2Name"] = cfg.sensor2Name;
  doc["syslogServer"] = cfg.syslogServer;
  doc["snmpCommunity"] = cfg.snmpCommunity;

  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

void WebServerManager::handleApiConfigPost(AsyncWebServerRequest* request) {
  if (!checkAuth(request)) return;

  DeviceConfig cfg = _config.getConfig();

  if (request->hasParam("systemName", true)) cfg.systemName = request->getParam("systemName", true)->value();
  if (request->hasParam("newPassword", true)) {
    String pw = request->getParam("newPassword", true)->value();
    if (pw.length() > 0) cfg.settingsPassword = pw;
  }

  cfg.lanDhcp = request->hasParam("lanDhcp", true);
  if (request->hasParam("lanIp", true)) cfg.lanIp = request->getParam("lanIp", true)->value();
  if (request->hasParam("lanMask", true)) cfg.lanMask = request->getParam("lanMask", true)->value();
  if (request->hasParam("lanGateway", true)) cfg.lanGateway = request->getParam("lanGateway", true)->value();

  cfg.wlanDhcp = request->hasParam("wlanDhcp", true);
  if (request->hasParam("wlanSsid", true)) cfg.wlanSsid = request->getParam("wlanSsid", true)->value();
  if (request->hasParam("wlanPsk", true)) cfg.wlanPsk = request->getParam("wlanPsk", true)->value();
  if (request->hasParam("wlanIp", true)) cfg.wlanIp = request->getParam("wlanIp", true)->value();
  if (request->hasParam("wlanMask", true)) cfg.wlanMask = request->getParam("wlanMask", true)->value();
  if (request->hasParam("wlanGateway", true)) cfg.wlanGateway = request->getParam("wlanGateway", true)->value();

  cfg.sensor2Enabled = request->hasParam("sensor2Enabled", true);
  if (request->hasParam("sensor2Name", true)) cfg.sensor2Name = request->getParam("sensor2Name", true)->value();

  if (request->hasParam("syslogServer", true)) cfg.syslogServer = request->getParam("syslogServer", true)->value();

  if (request->hasParam("snmpCommunity", true)) {
    String community = request->getParam("snmpCommunity", true)->value();
    if (community.length() > 0) cfg.snmpCommunity = community;
  }

  _config.setConfig(cfg);
  _data.pushLogEntry("Einstellungen gespeichert (Reboot noetig fuer Netzwerk-/SNMP-Aenderungen)");

  request->redirect("/settings");
}

void WebServerManager::handleApiConfigExport(AsyncWebServerRequest* request) {
  if (!checkAuth(request)) return;
  String xml = _config.exportXml();
  AsyncWebServerResponse* response = request->beginResponse(200, "application/xml", xml);
  response->addHeader("Content-Disposition", "attachment; filename=config.xml");
  request->send(response);
}

void WebServerManager::handleApiConfigImportUpload(AsyncWebServerRequest* request, const String& filename,
                                                    size_t index, uint8_t* data, size_t len, bool final) {
  if (!checkAuth(request)) return;

  if (index == 0) _importBuffer = "";
  for (size_t i = 0; i < len; i++) _importBuffer += (char)data[i];

  if (final) {
    if (_config.importXml(_importBuffer)) {
      _config.save();
      _data.pushLogEntry("Konfiguration importiert (Reboot empfohlen)");
    } else {
      _data.pushLogEntry("Konfigurationsimport fehlgeschlagen (ungueltiges XML)", 3);
    }
  }
}

void WebServerManager::handleApiReboot(AsyncWebServerRequest* request) {
  if (!checkAuth(request)) return;
  request->send(200, "text/plain", "Geraet startet neu...");
  _data.pushLogEntry("Reboot ueber Einstellungsseite ausgeloest");
  delay(500);
  ESP.restart();
}

void WebServerManager::handleApiWifiScan(AsyncWebServerRequest* request) {
  if (!checkAuth(request)) return;

  int n = WiFi.scanNetworks();  // blockierend, siehe Klassenkommentar
  JsonDocument doc;
  JsonArray networks = doc["networks"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject o = networks.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
  }
  WiFi.scanDelete();

  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

// ----------------------------------------------------------------------------
void WebServerManager::begin() {
  _server.on("/", HTTP_GET, [this](AsyncWebServerRequest* r) { handleRoot(r); });
  _server.on("/settings", HTTP_GET, [this](AsyncWebServerRequest* r) { handleSettingsPage(r); });
  _server.on("/values.csv", HTTP_GET, [this](AsyncWebServerRequest* r) { handleValuesCsv(r); });

  _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* r) { handleApiStatus(r); });
  _server.on("/api/sensors", HTTP_GET, [this](AsyncWebServerRequest* r) { handleApiSensors(r); });
  _server.on("/api/network", HTTP_GET, [this](AsyncWebServerRequest* r) { handleApiNetwork(r); });
  _server.on("/api/logs", HTTP_GET, [this](AsyncWebServerRequest* r) { handleApiLogs(r); });
  _server.on("/api/graph", HTTP_GET, [this](AsyncWebServerRequest* r) { handleApiGraph(r); });

  _server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* r) { handleApiConfigGet(r); });
  _server.on("/api/config", HTTP_POST, [this](AsyncWebServerRequest* r) { handleApiConfigPost(r); });
  _server.on("/api/config/export", HTTP_GET, [this](AsyncWebServerRequest* r) { handleApiConfigExport(r); });

  _server.on(
      "/api/config/import", HTTP_POST,
      [this](AsyncWebServerRequest* r) {
        if (checkAuth(r)) r->redirect("/settings");
      },
      [this](AsyncWebServerRequest* r, String filename, size_t index, uint8_t* data, size_t len, bool final) {
        handleApiConfigImportUpload(r, filename, index, data, len, final);
      });

  _server.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* r) { handleApiReboot(r); });
  _server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* r) { handleApiWifiScan(r); });

  _server.on(
      "/api/ota/upload", HTTP_POST,
      [this](AsyncWebServerRequest* r) {
        if (!checkAuth(r)) return;
        if (_otaSuccess) {
          r->send(200, "text/plain", "Update erfolgreich, Geraet startet neu...");
          _data.pushLogEntry("OTA (lokaler Upload) erfolgreich, Neustart");
          delay(500);
          ESP.restart();
        } else {
          _data.pushLogEntry("OTA (lokaler Upload) fehlgeschlagen", 3);
          r->send(500, "text/plain", "Update fehlgeschlagen");
        }
      },
      [this](AsyncWebServerRequest* r, String filename, size_t index, uint8_t* data, size_t len, bool final) {
        if (!checkAuth(r)) return;
        if (index == 0) {
          _otaInProgress = _ota.beginLocalUpdate(UPDATE_SIZE_UNKNOWN);
          _otaSuccess = false;
        }
        if (_otaInProgress) {
          _otaInProgress = _ota.writeLocalUpdateChunk(data, len);
        }
        if (final && _otaInProgress) {
          _otaSuccess = _ota.endLocalUpdate();
        }
      });

  _server.begin();
  Serial.println("[WEB] Server gestartet auf Port 80");
}
