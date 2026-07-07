#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "DataManager.h"

// Treibt den Boot-Zustandsautomaten an (docs/lastenheft.txt Abschnitt 12):
// BOOT -> INIT -> NETWORK_CHECK -> RUN_NORMAL bzw. FALLBACK_MODE.
//
// P0 bringt Ethernet per DHCP hoch (uebernommen aus dem Code-Prototyp, siehe
// docs/entscheidungen.md) und meldet den Zustand an den DataManager. Was in
// P0 bewusst noch fehlt (folgt in P1): statische IP, WLAN-Fallback "installer",
// die 5min/3min-Fehlerkette.

class NetworkManager {
 public:
  explicit NetworkManager(DataManager& dataManager);

  void begin();
  void loop();

 private:
  DataManager& _data;
  unsigned long _networkCheckStartedMillis = 0;

  static NetworkManager* _instance;
  static void onEthEvent(WiFiEvent_t event);

  volatile bool _ethConnected = false;
  volatile bool _ethGotIp = false;
};
