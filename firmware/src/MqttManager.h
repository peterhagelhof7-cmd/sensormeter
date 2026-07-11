#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include "ConfigManager.h"
#include "DataManager.h"
#include "NetworkManager.h"

// Home-Assistant-Anbindung ueber MQTT-Discovery (siehe
// sensormeter-poe/repo/docs/lastenheft.txt Abschnitt 16 fuer das
// vollstaendige Feature-Design, hier auf die Sensor-Rolle beschraenkt -
// kein Relais/Aktor, siehe docs/entscheidungen.md "Portierungs-Kandidaten
// aus sensormeter-poe geprueft"). Deaktiviert, solange kein Broker
// konfiguriert ist (mqttEnabled=false, Default). Publiziert bei jedem
// Sensorzyklus (erkannt wie bei SyslogManager an einer Aenderung von
// lastReadMillis) - Discovery-Payload nur einmal je (Re-)Connect.
//
// Anders als bei Sensormeter WLAN (nur WLAN) hat dieses Projekt zwei
// moegliche aktive Interfaces (LAN + WLAN). WiFiClient() ist trotzdem
// ausreichend: alte Arduino-ESP32-Versionen (dieses Projekt nutzt 2.0.17,
// siehe docs/entscheidungen.md) haben keine separate "EthernetClient"-
// Klasse - WiFiClient ist nur ein duenner Wrapper um lwIP-BSD-Sockets, die
// unabhaengig vom konkreten Netzwerk-Interface funktionieren (das lwIP-
// Routing entscheidet anhand der Ziel-IP, nicht die Applikationsschicht).
// Welches Interface dabei tatsaechlich genutzt wird, wenn BEIDE gleich-
// zeitig eine IP haben, ist nicht explizit erzwingbar und wurde nicht auf
// echter Hardware verifiziert (siehe docs/entscheidungen.md).

class MqttManager {
 public:
  MqttManager(DataManager& dataManager, ConfigManager& configManager, NetworkManager& networkManager);

  void begin();
  void loop();

 private:
  DataManager& _data;
  ConfigManager& _config;
  NetworkManager& _network;

  WiFiClient _transport;
  PubSubClient _client;

  bool _discoverySent = false;
  unsigned long _lastSensorReadMillisSeen = 0;
  unsigned long _lastReconnectAttemptMillis = 0;

  bool mqttEnabled() const;
  void ensureConnected();
  void publishDiscovery();
  void publishState();
  String topicPrefix() const;
};
