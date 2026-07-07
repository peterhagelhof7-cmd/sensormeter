#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "ConfigManager.h"
#include "DataManager.h"

// Treibt den Boot-Zustandsautomaten an (docs/lastenheft.txt Abschnitt 8+12):
//   BOOT -> INIT -> NETWORK_CHECK -> RUN_NORMAL bzw. FALLBACK_MODE
//
// P1 ergaenzt gegenueber P0:
//   - statische IP fuer LAN/WLAN (aus ConfigManager, aktuell noch Defaults -
//     echtes Laden aus config.xml folgt in P2)
//   - WLAN-Verbindung parallel zu Ethernet, falls SSID konfiguriert
//   - echter Fallback: nach 5 Minuten ohne Netzwerk wird versucht, das
//     Recovery-WLAN "installer"/"installer" zu joinen (siehe
//     docs/entscheidungen.md fuer die Auslegung dieser Spezifikationsstelle)
//   - Hilfsfunktionen fuer TimeManager's NTP-Fehlerkette (DHCP-Test/Restore)

class NetworkManager {
 public:
  NetworkManager(DataManager& dataManager, ConfigManager& configManager);

  void begin();
  void loop();

  bool isLanUp() const { return _ethGotIp; }
  bool isWlanUp() const { return _wlanGotIp; }
  bool isUsingFallbackWlan() const { return _inFallbackWlan && _wlanGotIp; }

  // Fuer Display (P4) / Webserver (P5). Implementiert in .cpp, damit dieser
  // Header nicht <ETH.h> einbinden muss (Makro-Reihenfolge, siehe .cpp).
  IPAddress getLanIp() const;
  IPAddress getWlanIp() const;

  // Fuer TimeManager (Lastenheft: "nach 5 min ohne NTP -> DHCP aktivieren,
  // nach weiteren 3 min -> gesetzte IP-Einstellungen wiederherstellen").
  bool hasStaticConfig() const;
  void beginDhcpFallbackTest();
  void restoreConfiguredAddresses();

 private:
  DataManager& _data;
  ConfigManager& _config;

  unsigned long _networkCheckStartedMillis = 0;
  unsigned long _lastFallbackJoinAttemptMillis = 0;
  bool _inFallbackWlan = false;
  bool _wlanConfigured = false;

  void applyLanConfig();
  void applyWlanConfig();
  bool networkOk() const { return _ethGotIp || _wlanGotIp; }

  static NetworkManager* _instance;
  static void onNetworkEvent(WiFiEvent_t event);

  volatile bool _ethConnected = false;
  volatile bool _ethGotIp = false;
  volatile bool _wlanGotIp = false;
};
