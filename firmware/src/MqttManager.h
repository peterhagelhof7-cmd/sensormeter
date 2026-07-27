#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include "ConfigManager.h"
#include "DataManager.h"
#include "NetworkManager.h"
#include "RelayManager.h"

// Home-Assistant-Anbindung ueber MQTT-Discovery (siehe
// sensormeter-poe/repo/docs/lastenheft.txt Abschnitt 16 fuer das
// vollstaendige Feature-Design). Deaktiviert, solange kein Broker
// konfiguriert ist (mqttEnabled=false, Default). Publiziert Sensorwerte bei
// jedem Sensorzyklus (erkannt wie bei SyslogManager an einer Aenderung von
// lastReadMillis), Relais-Zustand sofort bei Aenderung - Discovery-Payload
// nur einmal je (Re-)Connect.
//
// Anders als bei Sensormeter WLAN (nur WLAN) hat dieses Projekt zwei
// moegliche aktive Interfaces (LAN + WLAN). WiFiClient() ist trotzdem
// ausreichend: alte Arduino-ESP32-Versionen (dieses Projekt nutzt 2.0.17,
// siehe docs/entscheidungen.md) haben keine separate "EthernetClient"-
// Klasse - WiFiClient ist nur ein duenner Wrapper um lwIP-BSD-Sockets, die
// unabhaengig vom konkreten Netzwerk-Interface funktionieren (das lwIP-
// Routing entscheidet anhand der Ziel-IP, nicht die Applikationsschicht).
//
// Seit 2026-07-15 (ConfigManager::mqttInterface) wird das nicht mehr dem
// lwIP-Standardverhalten ueberlassen: ensureConnected() setzt vor jedem
// connect()-Versuch per NetworkManager::pinDefaultInterface() explizit das
// gewaehlte Interface als lwIP-Default-Netif und stellt danach ueber
// restoreDefaultInterface() den vorherigen Zustand wieder her - damit ist
// deterministisch festgelegt, ueber welches Interface der Broker erreicht
// wird, auch wenn beide gleichzeitig eine IP haben. Seit 2026-07-22 nutzt
// TimeManager denselben NetworkManager-Mechanismus fuer seine
// LAN-vor-WLAN-NTP-Fehlerkette (siehe TimeManager.h) - Implementierung
// daher in NetworkManager.cpp, nicht mehr hier. Details zur lwIP- vs.
// esp_netif-Funktionswahl siehe dortiger Kommentar und docs/entscheidungen.md.

class MqttManager {
 public:
  MqttManager(DataManager& dataManager, ConfigManager& configManager, NetworkManager& networkManager,
              RelayManager& relayManager);

  void begin();
  void loop();

  String topicPrefix() const;

 private:
  DataManager& _data;
  ConfigManager& _config;
  NetworkManager& _network;
  RelayManager& _relay;

  WiFiClient _transport;
  PubSubClient _client;

  bool _discoverySent = false;
  unsigned long _lastSensorReadMillisSeen = 0;
  unsigned long _lastReconnectAttemptMillis = 0;
  bool _lastRelayOnSeen = false;
  bool _relayStateKnown = false;

  bool mqttEnabled() const;
  void ensureConnected();
  void publishDiscovery();
  void publishState();
  void publishRelayState();
  void subscribeCommandTopics();

  static MqttManager* _instance;
  static void onMqttMessage(char* topic, uint8_t* payload, unsigned int length);
  void handleMessage(const String& topic, const String& payload);
};
