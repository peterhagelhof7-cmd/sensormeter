#pragma once

#include <Arduino.h>

// Laufzeitkonfiguration gemaess docs/lastenheft.txt ("LittleFS fuer
// Einstellungen" / config.xml). P2: echtes Laden/Speichern auf LittleFS +
// XML-Import/-Export. Die eigentliche Einstellungsseite (Web-UI zum Aendern
// dieser Werte) folgt in P5 - hier steht nur die Persistenzschicht.
//
// Schema (config.xml), erweitert gegenueber dem knappen Beispiel im
// Lastenheft um wlan ip/mask/gateway (siehe docs/entscheidungen.md):
//
// <config>
//   <network>
//     <lan dhcp="true" ip="" mask="" gateway=""/>
//     <wlan dhcp="true" ssid="" psk="" ip="" mask="" gateway=""/>
//   </network>
//   <system>
//     <name>Sensormeter</name>
//     <type>Sensormeter</type>
//     <password>installer</password>
//   </system>
//   <syslog>
//     <server>0.0.0.0</server>
//   </syslog>
//   <sensors>
//     <sensor2 enabled="false" name="Extern"/>
//   </sensors>
//   <snmp community="public"/>
// </config>

struct DeviceConfig {
  String systemName = "Sensormeter";
  String systemType = "Sensormeter";  // "Sensormeter" oder "Sensormeter PRO"
  String settingsPassword = "installer";

  bool lanDhcp = true;
  String lanIp;
  String lanMask;
  String lanGateway;

  bool wlanDhcp = true;
  String wlanIp;
  String wlanMask;
  String wlanGateway;
  String wlanSsid;
  String wlanPsk;

  String syslogServer = "0.0.0.0";

  bool sensor2Enabled = false;
  String sensor2Name = "Extern";

  String snmpCommunity = "public";
};

class ConfigManager {
 public:
  // Laedt config.xml von LittleFS. Fehlt die Datei oder ist sie ungueltig,
  // werden Defaults verwendet und sofort als neue config.xml gespeichert.
  void begin();

  const DeviceConfig& getConfig() const { return _config; }

  // Uebernimmt eine neue Konfiguration und speichert sie sofort (fuer die
  // Einstellungsseite in P5).
  void setConfig(const DeviceConfig& config);

  // XML-Import/-Export (Lastenheft: "import einer xml configuration" /
  // "export der laufenden konfiguration"). importXml uebernimmt nur bei
  // erfolgreichem Parsen und speichert dann.
  bool importXml(const String& xml);
  String exportXml() const;

  bool save();

 private:
  DeviceConfig _config;
  bool load();
};
