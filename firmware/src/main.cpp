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
// sowie Fehler-Events sofort per UDP (Port 514); MqttManager veroeffent-
// licht bei aktivierter Home-Assistant-Anbindung Discovery- und State-
// Payloads per MQTT (siehe sensormeter-poe/repo/docs/lastenheft.txt
// Abschnitt 16, docs/entscheidungen.md); BrandingManager haelt den
// optionalen Anbieter-Namen/das Logo (Weisslabel), das DisplayManager als
// eigene OLED-Seite und WebServerManager im Seiten-Header zeigt, sobald
// konfiguriert.
//
// Damit sind alle Phasen aus docs/implementierungsplan.html (P0-P7)
// umgesetzt.
// ============================================================================

#include <Arduino.h>
#include <ESPmDNS.h>
#include <LittleFS.h>

#include "BrandingManager.h"
#include "ConfigManager.h"
#include "DataManager.h"
#include "DisplayManager.h"
#include "MqttManager.h"
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
BrandingManager brandingManager(configManager);
DisplayManager displayManager(dataManager, configManager, networkManager, brandingManager);
OtaManager otaManager;
WebServerManager webServerManager(dataManager, configManager, networkManager, otaManager, brandingManager);
SNMPManager snmpManager(dataManager, configManager, networkManager);
SyslogManager syslogManager(dataManager, configManager, networkManager);
MqttManager mqttManager(dataManager, configManager, networkManager);

// Serial-Kommandozeile fuer den Fall, dass das Geraet nur per USB, aber
// nicht per Netzwerk erreichbar ist. Bewusst dasselbe Vertrauensmodell wie
// der bestehende BOOT-Taster-Werksreset (physischer USB-Zugriff =
// vertrauenswuerdig, kein Web-Passwort noetig) - anders als dieser aber
// NICHT pauschal destruktiv: nur "reset"/"reset all" loeschen etwas, alle
// anderen Kommandos aendern gezielt nur Netzwerk-Felder. Anders als
// Sensormeter WLAN hat dieses Geraet zwei Interfaces (LAN + optionales
// WLAN), daher brauchen "dhcp"/"ip" ein Interface-Argument - siehe
// docs/entscheidungen.md.
//
// Kommandos (jeweils + Enter):
//   dhcp <lan|wlan>                Interface auf DHCP umstellen, statische
//                                  IP/Maske/Gateway/DNS loeschen, neu starten
//   ip <lan|wlan> <ip> <maske> <gateway> [dns]
//                                  statische IP setzen, neu starten. Anders
//                                  als die Einstellungsseite OHNE
//                                  Ping-Kollisionspruefung - bewusst einfach
//                                  gehalten, siehe docs/entscheidungen.md
//   wifi <ssid> <passwort>        neue WLAN-Zugangsdaten setzen, neu starten
//                                  (setzt wlanPendingTest, damit der erste
//                                  Verbindungsversuch nur kurz statt 5 Min.
//                                  abgewartet wird, bevor auf reine
//                                  LAN-Nutzung zurueckgefallen wird)
//   status                        aktuellen Zustand ausgeben (LAN, WLAN,
//                                  IP, Signal, beide Sensoren, Heap,
//                                  Laufzeit) - liest nur, aendert nichts,
//                                  kein Neustart
//   dump                          aktuelle config.xml als XML ausgeben,
//                                  eingerahmt von BEGIN/END-Markern
//   upload                        wartet auf eingefuegte XML-Zeilen (z.B.
//                                  Ausgabe von "dump" zurueckgepastet),
//                                  Abschluss mit einer Zeile
//                                  "-----END CONFIG-----"; bei gueltigem
//                                  XML wird gespeichert und neu gestartet,
//                                  bei ungueltigem XML passiert nichts
//   reset                         Werksreset nur der Einstellungen, neu
//                                  starten (7-Tage-Verlauf bleibt erhalten)
//   reset all                     Werksreset der Einstellungen UND Loeschen
//                                  des 7-Tage-Verlaufs, neu starten
void handleSerialCommands() {
  static String line;
  static bool uploadMode = false;
  static String uploadBuffer;

  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c != '\n') {
      line += c;
      continue;
    }
    line.trim();

    if (uploadMode) {
      if (line == "-----END CONFIG-----") {
        uploadMode = false;
        if (configManager.importXml(uploadBuffer)) {
          configManager.save();
          Serial.println("[SERIAL] Konfiguration importiert, starte neu...");
          delay(300);
          ESP.restart();
        } else {
          Serial.println("[SERIAL] Import fehlgeschlagen: ungueltiges XML, keine Aenderung.");
        }
        uploadBuffer = "";
      } else if (line != "-----BEGIN CONFIG-----") {
        uploadBuffer += line;
        uploadBuffer += "\n";
      }
      line = "";
      continue;
    }

    String cmd = line;
    String args;
    int sp = line.indexOf(' ');
    if (sp >= 0) {
      cmd = line.substring(0, sp);
      args = line.substring(sp + 1);
      args.trim();
    }

    if (cmd.equalsIgnoreCase("dhcp")) {
      String iface = args;
      iface.trim();
      if (!iface.equalsIgnoreCase("lan") && !iface.equalsIgnoreCase("wlan")) {
        Serial.println("[SERIAL] Nutzung: dhcp <lan|wlan>");
      } else {
        DeviceConfig cfg = configManager.getConfig();
        if (iface.equalsIgnoreCase("lan")) {
          cfg.lanDhcp = true;
          cfg.lanIp = "";
          cfg.lanMask = "";
          cfg.lanGateway = "";
          cfg.lanDns = "";
        } else {
          cfg.wlanDhcp = true;
          cfg.wlanIp = "";
          cfg.wlanMask = "";
          cfg.wlanGateway = "";
          cfg.wlanDns = "";
        }
        configManager.setConfig(cfg);
        Serial.println("[SERIAL] " + iface + " auf DHCP umgestellt, starte neu...");
        delay(300);
        ESP.restart();
      }

    } else if (cmd.equalsIgnoreCase("ip")) {
      String iface;
      String rest = args;
      int sp1 = rest.indexOf(' ');
      if (sp1 >= 0) {
        iface = rest.substring(0, sp1);
        rest = rest.substring(sp1 + 1);
        rest.trim();
      }
      String parts[4];
      int count = 0;
      while (rest.length() > 0 && count < 4) {
        int sp2 = rest.indexOf(' ');
        if (sp2 < 0) {
          parts[count++] = rest;
          rest = "";
        } else {
          parts[count++] = rest.substring(0, sp2);
          rest = rest.substring(sp2 + 1);
          rest.trim();
        }
      }
      IPAddress probe;
      if ((!iface.equalsIgnoreCase("lan") && !iface.equalsIgnoreCase("wlan")) || count < 3 ||
          !probe.fromString(parts[0]) || !probe.fromString(parts[1]) || !probe.fromString(parts[2])) {
        Serial.println("[SERIAL] Nutzung: ip <lan|wlan> <adresse> <maske> <gateway> [dns]");
      } else {
        DeviceConfig cfg = configManager.getConfig();
        if (iface.equalsIgnoreCase("lan")) {
          cfg.lanDhcp = false;
          cfg.lanIp = parts[0];
          cfg.lanMask = parts[1];
          cfg.lanGateway = parts[2];
          cfg.lanDns = (count >= 4) ? parts[3] : "";
        } else {
          cfg.wlanDhcp = false;
          cfg.wlanIp = parts[0];
          cfg.wlanMask = parts[1];
          cfg.wlanGateway = parts[2];
          cfg.wlanDns = (count >= 4) ? parts[3] : "";
        }
        configManager.setConfig(cfg);
        Serial.println("[SERIAL] Statische IP (" + iface + ") gesetzt, starte neu...");
        delay(300);
        ESP.restart();
      }

    } else if (cmd.equalsIgnoreCase("wifi")) {
      int sp3 = args.indexOf(' ');
      if (sp3 < 0 || args.substring(0, sp3).length() == 0) {
        Serial.println("[SERIAL] Nutzung: wifi <ssid> <passwort>");
      } else {
        DeviceConfig cfg = configManager.getConfig();
        cfg.wlanSsid = args.substring(0, sp3);
        cfg.wlanPsk = args.substring(sp3 + 1);
        cfg.wlanPsk.trim();
        cfg.wlanPendingTest = true;
        configManager.setConfig(cfg);
        Serial.println("[SERIAL] WLAN-Zugangsdaten gesetzt, starte neu...");
        delay(300);
        ESP.restart();
      }

    } else if (cmd.equalsIgnoreCase("status")) {
      DeviceConfig cfg = configManager.getConfig();
      SensorReading sensor1 = dataManager.getSensor1();
      Serial.println("[SERIAL] --- Status ---");
      Serial.print("Zustand: ");
      Serial.println(toString(dataManager.getSystemState()));
      Serial.print("LAN: ");
      Serial.println(networkManager.isLanUp() ? "verbunden" : (networkManager.isLanLinkUp() ? "Link ohne IP" : "kein Link"));
      Serial.print("LAN-IP: ");
      Serial.println(networkManager.getLanIp());
      Serial.print("LAN-Modus: ");
      Serial.println(cfg.lanDhcp ? "DHCP" : "statisch");
      Serial.print("WLAN: ");
      if (networkManager.isUsingFallbackWlan()) {
        Serial.println("Fallback-Access-Point \"installer\"");
      } else if (networkManager.isWlanUp()) {
        Serial.print("verbunden mit ");
        Serial.println(cfg.wlanSsid);
      } else {
        Serial.println("nicht verbunden");
      }
      Serial.print("WLAN-IP: ");
      Serial.println(networkManager.getWlanIp());
      Serial.print("WLAN-Modus: ");
      Serial.println(cfg.wlanDhcp ? "DHCP" : "statisch");
      Serial.print("WLAN-Signal: ");
      Serial.print(networkManager.getWlanRssi());
      Serial.println(" dBm");
      Serial.print("Sensor 1: ");
      if (sensor1.valid) {
        Serial.print(sensor1.temperature, 1);
        Serial.print(" C / ");
        Serial.print(sensor1.humidity, 1);
        Serial.println(" %");
      } else {
        Serial.println("kein gueltiger Messwert");
      }
      if (cfg.sensor2Enabled) {
        SensorReading sensor2 = dataManager.getSensor2();
        Serial.print("Sensor 2 (" + cfg.sensor2Name + "): ");
        if (sensor2.valid) {
          Serial.print(sensor2.temperature, 1);
          Serial.print(" C / ");
          Serial.print(sensor2.humidity, 1);
          Serial.println(" %");
        } else {
          Serial.println("kein gueltiger Messwert");
        }
      }
      Serial.print("Freier Heap: ");
      Serial.print(ESP.getFreeHeap() / 1024);
      Serial.println(" kB");
      Serial.print("Laufzeit: ");
      Serial.print((unsigned long)(esp_timer_get_time() / 1000000ULL));
      Serial.println(" s");
      Serial.println("[SERIAL] --- Ende Status ---");

    } else if (cmd.equalsIgnoreCase("dump")) {
      Serial.println("-----BEGIN CONFIG-----");
      Serial.println(configManager.exportXml());
      Serial.println("-----END CONFIG-----");

    } else if (cmd.equalsIgnoreCase("upload")) {
      uploadMode = true;
      uploadBuffer = "";
      Serial.println(
          "[SERIAL] Warte auf XML-Zeilen, Abschluss mit einer Zeile \"-----END CONFIG-----\"");

    } else if (cmd.equalsIgnoreCase("reset")) {
      bool full = args.equalsIgnoreCase("all");
      configManager.setConfig(DeviceConfig());
      if (full) {
        LittleFS.remove("/history.csv");
        Serial.println("[SERIAL] Werksreset: Einstellungen und Verlauf geloescht, starte neu...");
      } else {
        Serial.println("[SERIAL] Werksreset: Einstellungen auf Standard zurueckgesetzt, starte neu...");
      }
      delay(300);
      ESP.restart();
    }

    line = "";
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.print("=== Sensormeter ");
  Serial.print(DEVICE_FIRMWARE_VERSION);
  Serial.println(" ===");
  Serial.println("[SERIAL] Kommandos: dhcp <lan|wlan>, ip <lan|wlan> ..., wifi, status, dump, upload, reset[ all] (+ Enter)");

  dataManager.begin();
  dataManager.setSystemState(SystemState::BOOT);

  storageManager.begin();
  dataManager.loadRingbuffer();
  configManager.begin();
  timeManager.begin();
  sensorManager.begin();
  brandingManager.begin();
  displayManager.begin();
  syslogManager.begin();
  mqttManager.begin();

  networkManager.begin();  // setzt Zustand auf INIT, dann NETWORK_CHECK
  webServerManager.begin();  // async - kein eigener loop()-Aufruf noetig
  snmpManager.begin();
}

void loop() {
  handleSerialCommands();
  networkManager.loop();
  timeManager.loop();
  sensorManager.loop();
  displayManager.loop();
  snmpManager.loop();
  syslogManager.loop();
  mqttManager.loop();

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
