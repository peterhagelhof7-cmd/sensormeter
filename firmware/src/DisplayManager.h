#pragma once

#include <Arduino.h>
#include "ConfigManager.h"
#include "DataManager.h"
#include "NetworkManager.h"

// OLED-Anzeige (Pflichtenheft-Task "DisplayTask"): SSD1306 128x64 I2C auf
// IO32 (SCL)/IO33 (SDA), Adresse 0x3C. Rotierende Infoseiten alle 10s
// (Lastenheft Abschnitt 11: Systemname / IPs / Uhrzeit / Sensorwerte /
// Status LAN+WLAN). Waehrend des Bootens (BOOT/INIT/NETWORK_CHECK)
// stattdessen Systemname + Countdown 100->0 bis das Netzwerk bereit ist.

class DisplayManager {
 public:
  DisplayManager(DataManager& dataManager, ConfigManager& configManager, NetworkManager& networkManager);

  void begin();
  void loop();

 private:
  DataManager& _data;
  ConfigManager& _config;
  NetworkManager& _network;

  bool _initialized = false;

  unsigned long _lastPageSwitchMillis = 0;
  int _currentPage = 0;
  static const int PAGE_COUNT = 5;

  unsigned long _lastCountdownTickMillis = 0;
  int _countdownValue = 100;

  void drawLines(const String lines[], int count);
  void drawBootScreen();
  void drawPage(int page);
  void drawSystemNamePage();
  void drawIpsPage();
  void drawTimePage();
  void drawSensorsPage();
  void drawStatusPage();
};
