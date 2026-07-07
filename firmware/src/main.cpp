// ============================================================================
// Sensormeter (WT32-ETH01) - Phase P0: Grundgeruest & Zustandsmodell
//
// Verdrahtet die Module (DataManager/ConfigManager/StorageManager/
// TimeManager/NetworkManager) und bringt Ethernet per DHCP hoch, um den
// Boot-Zustandsautomaten aus docs/lastenheft.txt Abschnitt 12 real anzutreiben.
//
// Was hier bewusst noch fehlt (siehe docs/implementierungsplan.html):
//   P1 Netzwerk/Zeit (WLAN-Fallback, statische IP, NTP)
//   P2 Konfigurationspersistenz (LittleFS/config.xml)
//   P3 Sensorik (DHT11/DHT22, Ringpuffer-Befuellung)
//   P4-P7 Display, Webserver, SNMP v1, Syslog
// ============================================================================

#include <Arduino.h>

#include "ConfigManager.h"
#include "DataManager.h"
#include "NetworkManager.h"
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
TimeManager timeManager;
NetworkManager networkManager(dataManager);

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

  networkManager.begin();  // setzt Zustand auf INIT, dann NETWORK_CHECK
}

void loop() {
  networkManager.loop();
  timeManager.loop();
  delay(50);
}
