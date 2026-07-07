// ============================================================================
// Sensormeter (WT32-ETH01) - Phase P5: Webserver
//
// Verdrahtet alle Module. ConfigManager laedt/speichert config.xml auf
// LittleFS; NetworkManager bringt Ethernet (DHCP/statisch) und optional
// WLAN hoch und treibt den Boot-Zustandsautomaten aus docs/lastenheft.txt
// Abschnitt 12 an; TimeManager haengt sich mit der NTP-Sync- und
// Fehlerkette daran; SensorManager liest DHT11/DHT22 im 60s-Takt und
// fuellt den stuendlichen Ringpuffer; DisplayManager zeigt Boot-Countdown
// und rotierende Infoseiten; WebServerManager stellt Hauptseite,
// Einstellungsseite, REST-API und OTA-Update bereit (async, Port 80).
//
// Was hier bewusst noch fehlt (siehe docs/implementierungsplan.html):
//   P6 SNMP v1, P7 Syslog (UDP-Versand der DataManager-Log-Eintraege)
// ============================================================================

#include <Arduino.h>

#include "ConfigManager.h"
#include "DataManager.h"
#include "DisplayManager.h"
#include "NetworkManager.h"
#include "OtaManager.h"
#include "SensorManager.h"
#include "StorageManager.h"
#include "SystemState.h"
#include "TimeManager.h"
#include "WebServerManager.h"

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
OtaManager otaManager;
WebServerManager webServerManager(dataManager, configManager, networkManager, otaManager);

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
  webServerManager.begin();  // async - kein eigener loop()-Aufruf noetig
}

void loop() {
  networkManager.loop();
  timeManager.loop();
  sensorManager.loop();
  displayManager.loop();
  delay(50);
}
