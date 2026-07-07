// ============================================================================
// Sensormeter (WT32-ETH01) - Phase P4: Display
//
// Verdrahtet die Module (DataManager/ConfigManager/StorageManager/
// NetworkManager/TimeManager/SensorManager/DisplayManager). ConfigManager
// laedt/speichert config.xml auf LittleFS (Default-Fallback beim ersten
// Boot); NetworkManager bringt Ethernet (DHCP/statisch) und optional WLAN
// hoch und treibt den Boot-Zustandsautomaten aus docs/lastenheft.txt
// Abschnitt 12 an; TimeManager haengt sich mit der NTP-Sync- und
// Fehlerkette (DHCP-Test/Restore) daran; SensorManager liest DHT11
// (intern) und optional DHT22 (extern, Sensormeter PRO) im 60s-Takt und
// fuellt den stuendlichen Ringpuffer; DisplayManager zeigt waehrend des
// Bootens Systemname + Countdown, danach rotierende Infoseiten (10s-Takt).
//
// Was hier bewusst noch fehlt (siehe docs/implementierungsplan.html):
//   P5-P7 Webserver, SNMP v1, Syslog, OTA
// ============================================================================

#include <Arduino.h>

#include "ConfigManager.h"
#include "DataManager.h"
#include "DisplayManager.h"
#include "NetworkManager.h"
#include "SensorManager.h"
#include "StorageManager.h"
#include "SystemState.h"
#include "TimeManager.h"

#if __has_include("config.h")
#include "config.h"
#else
#error "config.h fehlt! Kopiere include/config.h.example nach include/config.h."
#endif

DataManager dataManager;
ConfigManager configManager;
StorageManager storageManager;
NetworkManager networkManager(dataManager, configManager);
TimeManager timeManager(dataManager, networkManager);
SensorManager sensorManager(dataManager, configManager);
DisplayManager displayManager(dataManager, configManager, networkManager);

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.print("=== Sensormeter ");
  Serial.print(DEVICE_FIRMWARE_VERSION);
  Serial.println(" ===");

  dataManager.begin();
  dataManager.setSystemState(SystemState::BOOT);

  storageManager.begin();
  configManager.begin();
  timeManager.begin();
  sensorManager.begin();
  displayManager.begin();

  networkManager.begin();  // setzt Zustand auf INIT, dann NETWORK_CHECK
}

void loop() {
  networkManager.loop();
  timeManager.loop();
  sensorManager.loop();
  displayManager.loop();
  delay(50);
}
