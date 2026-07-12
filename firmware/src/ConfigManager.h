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
//     <lan dhcp="true" ip="" mask="" gateway="" dns=""/>
//     <wlan dhcp="true" ssid="" psk="" ip="" mask="" gateway="" dns="" pendingTest="false"/>
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
//     <sensor1 tempOffset="0.0" humOffset="0.0" calibratedTs="0"/>
//     <sensor2 enabled="false" name="Extern" tempOffset="0.0" humOffset="0.0" calibratedTs="0"/>
//   </sensors>
//   <snmp community="public"/>
//   <aktor relayEnabled="false"/>
//   <mqtt enabled="false" server="" port="1883" user="" password=""/>
//   <branding vendorName=""/>
// </config>

struct DeviceConfig {
  String systemName = "Sensormeter";
  String systemType = "Sensormeter";  // "Sensormeter" oder "Sensormeter PRO"
  String settingsPassword = "installer";

  // Kalibrierkorrektur je Sensor (fester Grad-/Prozent-Versatz, positiv
  // oder negativ) - falls ein Sensor systematisch von einem Referenzwert
  // abweicht. Wird direkt in SensorManager auf den validierten Rohmesswert
  // angewendet, damit Anzeige, SNMP UND Stundenwerte/CSV immer denselben,
  // bereits korrigierten Wert sehen (siehe docs/entscheidungen.md).
  float sensor1TempOffset = 0.0f;
  float sensor1HumOffset = 0.0f;
  float sensor2TempOffset = 0.0f;
  float sensor2HumOffset = 0.0f;

  // Wall-Clock-Zeitpunkt (time(nullptr)), zu dem die jeweiligen Offsets
  // zuletzt TATSAECHLICH geaendert wurden (nicht nur gespeichert - siehe
  // WebServerManager::handleApiConfigPost()). 0 = noch nie kalibriert.
  uint32_t sensor1CalibratedTs = 0;
  uint32_t sensor2CalibratedTs = 0;

  bool lanDhcp = true;
  String lanIp;
  String lanMask;
  String lanGateway;
  String lanDns;  // leer = Gateway als DNS verwenden (siehe NetworkManager::applyLanConfig)

  bool wlanDhcp = true;
  String wlanIp;
  String wlanMask;
  String wlanGateway;
  String wlanDns;  // leer = Gateway als DNS verwenden (siehe NetworkManager::applyWlanConfig)
  String wlanSsid;
  String wlanPsk;
  // Einmal-Flag: nach Eingabe neuer WLAN-Zugangsdaten ueber die
  // Einstellungsseite im Fallback-Access-Point gesetzt, damit
  // NetworkManager den anschliessenden Verbindungsversuch nur kurz statt
  // 5 Minuten abwartet (schnelles Feedback), bevor er wieder auf den
  // Fallback-AP zurueckfaellt. Wird beim naechsten Boot sofort gelesen und
  // geloescht - ueberlebt also nur genau einen Neustart (siehe
  // NetworkManager::begin()).
  bool wlanPendingTest = false;

  String syslogServer = "0.0.0.0";

  bool sensor2Enabled = false;
  String sensor2Name = "Extern";

  String snmpCommunity = "public";

  // Relais (Aktor) - rein manuell, keine Auto-Erkennung moeglich (analog
  // sensormeter-poe/repo/docs/lastenheft.txt Abschnitt 15.3), unabhaengig
  // von sensor2Enabled (RJ45-Pins ueberschneiden sich nicht, siehe pins.h).
  // Der aktuelle Schaltzustand selbst wird NICHT persistiert -
  // RelayManager startet nach jedem Boot sicherheitshalber immer mit AUS,
  // siehe dortige Begruendung.
  bool relayEnabled = false;

  // Home-Assistant-Anbindung ueber MQTT-Discovery (siehe
  // sensormeter-poe/repo/docs/lastenheft.txt Abschnitt 16 fuer das
  // vollstaendige Feature-Design), jetzt inklusive Aktor-Rolle (Relais,
  // siehe docs/entscheidungen.md). Topic-Praefix wird NICHT gespeichert,
  // sondern wie der mDNS-Hostname zur Laufzeit aus systemName abgeleitet
  // (NetworkManager::sanitizeHostname) - identisches Muster zu Sensormeter
  // WLAN.
  bool mqttEnabled = false;
  String mqttServer;
  uint16_t mqttPort = 1883;
  String mqttUser;
  String mqttPassword;

  // Anbieter-Branding (Weisslabel): frei einstellbarer Vendor-Name, erscheint
  // zusaetzlich zum weiterhin bestehenden, frei editierbaren Systemnamen auf
  // OLED-Slide und Webseiten-Header, sobald gesetzt. Das Logo-Bild selbst
  // wird NICHT hier gespeichert (Binaerdaten gehoeren nicht in die
  // config.xml), sondern separat als Datei auf LittleFS - siehe
  // BrandingManager. Leer = Feature inaktiv (Default), kein
  // Verhaltensunterschied fuer bestehende Installationen.
  String brandingVendorName;
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
