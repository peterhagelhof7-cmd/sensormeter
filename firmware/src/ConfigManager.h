#pragma once

#include <Arduino.h>

// Laufzeitkonfiguration gemaess docs/lastenheft.txt ("LittleFS fuer
// Einstellungen" / config.xml). Persistenz (Laden/Speichern auf LittleFS,
// XML-Import/-Export) folgt in Phase P2 - P0 legt nur Struktur + Defaults an.

struct DeviceConfig {
  String systemName = "Sensormeter";
  String systemType = "Sensormeter";  // "Sensormeter" oder "Sensormeter PRO"
  String settingsPassword = "installer";

  bool lanDhcp = true;
  String lanIp;
  String lanMask;
  String lanGateway;

  bool wlanDhcp = true;
  String wlanSsid;
  String wlanPsk;

  String syslogServer = "0.0.0.0";

  bool sensor2Enabled = false;
  String sensor2Name = "Extern";
};

class ConfigManager {
 public:
  void begin();  // TODO (P2): aus LittleFS/config.xml laden statt Defaults
  const DeviceConfig& getConfig() const { return _config; }

 private:
  DeviceConfig _config;
};
