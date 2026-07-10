// ============================================================================
// Sensormeter (WT32-ETH01) - Phase P7: Syslog
//
// Verdrahtet alle Module. ConfigManager laedt/speichert config.xml auf
// LittleFS; NetworkManager bringt Ethernet (DHCP/statisch) und optional
// WLAN hoch und treibt den Boot-Zustandsautomaten aus docs/lastenheft.txt
// Abschnitt 12 an; TimeManager haengt sich mit der NTP-Sync- und
// Fehlerkette daran; SensorManager liest DHT11/DHT22 im 60s-Takt und
// fuellt den stuendlichen Ringpuffer; DisplayManager zeigt Boot-Countdown
// und rotierende Infoseiten; WebServerManager stellt Hauptseite,
// Einstellungsseite, REST-API und lokalen OTA-Upload bereit (async, Port
// 80); SNMPManager beantwortet SNMP-v1/v2c-GET-Anfragen read-only (Port
// 161); SyslogManager sendet bei jedem Sensorzyklus einen Statusreport
// sowie Fehler-Events sofort per UDP (Port 514).
//
// Damit sind alle Phasen aus docs/implementierungsplan.html (P0-P7)
// umgesetzt.
// ============================================================================

#include <Arduino.h>
#include <ESPmDNS.h>

#include "ConfigManager.h"
#include "DataManager.h"
#include "DisplayManager.h"
#include "NetworkManager.h"
#include "OtaManager.h"
#include "SNMPManager.h"
#include "SensorManager.h"
#include "StorageManager.h"
#include "SyslogManager.h"
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
SNMPManager snmpManager(dataManager, configManager, networkManager);
SyslogManager syslogManager(dataManager, configManager, networkManager);

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
  dataManager.loadRingbuffer();
  configManager.begin();
  timeManager.begin();
  sensorManager.begin();
  displayManager.begin();
  syslogManager.begin();

  networkManager.begin();  // setzt Zustand auf INIT, dann NETWORK_CHECK
  webServerManager.begin();  // async - kein eigener loop()-Aufruf noetig
  snmpManager.begin();
}

void loop() {
  networkManager.loop();
  timeManager.loop();
  sensorManager.loop();
  displayManager.loop();
  snmpManager.loop();
  syslogManager.loop();

  // Einmaliger mDNS-Start, sobald irgendein Interface eine IP hat (LAN oder
  // WLAN-Fallback) - vor RUN_NORMAL ist noch keine IP vergeben, ein frueherer
  // Start wuerde ins Leere laufen. sensormeter.local loest dann auf allen
  // Interfaces auf, ueber die das Geraet gerade erreichbar ist.
  static bool mdnsStarted = false;
  if (!mdnsStarted && (networkManager.isLanUp() || networkManager.isWlanUp())) {
    String hostname = NetworkManager::sanitizeHostname(configManager.getConfig().systemName);
    if (MDNS.begin(hostname.c_str())) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[NET] mDNS gestartet: http://%s.local/\n", hostname.c_str());
    } else {
      Serial.println("[NET] mDNS-Start fehlgeschlagen");
    }
    mdnsStarted = true;
  }

  delay(50);
}
