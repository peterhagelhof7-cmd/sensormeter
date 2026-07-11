#pragma once

#include <Arduino.h>
#include "BrandingManager.h"
#include "ConfigManager.h"
#include "DataManager.h"
#include "NetworkManager.h"

// OLED-Anzeige (Pflichtenheft-Task "DisplayTask"): SSD1306 128x64 I2C auf
// IO32 (SCL)/IO33 (SDA), Adresse 0x3C. Rotierende Infoseiten alle 10s
// (Lastenheft Abschnitt 11: Systemname / IPs / Uhrzeit / Sensorwerte /
// Status LAN+WLAN, ergaenzt um eine WLAN-Signal-Seite). Alle Seiten
// horizontal+vertikal zentriert mit fester, groesserer Schrift - zu lange
// Zeilen laufen waagerecht durch statt geschrumpft zu werden (siehe
// drawScrollingLine()). Waehrend des Bootens (BOOT/INIT/NETWORK_CHECK)
// Systemname + Systemtyp + Countdown 100->0 bis das Netzwerk bereit ist. Im
// Fallback-Access-Point ("installer", siehe NetworkManager) stattdessen
// ausschliesslich die eigene IP - das ist der einzige Wert, den man zum
// Einrichten braucht, die Seitenrotation waere hier nur ablenkend.

class DisplayManager {
 public:
  DisplayManager(DataManager& dataManager, ConfigManager& configManager, NetworkManager& networkManager,
                 BrandingManager& brandingManager);

  void begin();
  void loop();

 private:
  DataManager& _data;
  ConfigManager& _config;
  NetworkManager& _network;
  BrandingManager& _branding;

  bool _initialized = false;

  unsigned long _lastPageSwitchMillis = 0;
  int _currentPage = 0;
  // Seite 6 (Branding) ist nur Teil der Rotation, wenn tatsaechlich ein
  // Vendor-Name oder ein Logo konfiguriert ist (siehe pageCount()) - im
  // unkonfigurierten Default-Fall erscheint dadurch keine leere
  // Zusatzseite in der Rotation.
  static const int BASE_PAGE_COUNT = 6;
  int pageCount() const { return _branding.isActive() ? BASE_PAGE_COUNT + 1 : BASE_PAGE_COUNT; }

  unsigned long _lastCountdownTickMillis = 0;
  int _countdownValue = 100;

  // Horizontal+vertikal zentriert, feste groessere Schrift (Groesse 2) -
  // einheitlich auf allen Screens. Zeilen, die dabei nicht auf einmal
  // passen (z.B. eine lange WLAN-SSID), laufen waagerecht durch statt
  // geschrumpft zu werden - siehe drawScrollingLine().
  void drawLines(const String lines[], int count);
  // progress: 0.0 (Start) bis 1.0 (Ende) - vom Aufrufer berechnet, damit
  // sowohl "einmal durchlaufen und am Ende halten" (rotierende Seiten,
  // synchron zum Seitenwechsel-Timer) als auch "dauerhaft wiederholen"
  // (Fallback-Seite, keine Wechsel-Deadline) denselben Zeichencode nutzen
  // koennen.
  void drawScrollingLine(const String& text, int y, int size, float progress);
  void drawBootScreen();
  void drawPage(int page);
  void drawSystemNamePage();
  void drawIpsPage();
  void drawTimePage();
  void drawSensorsPage();
  void drawStatusPage();
  void drawSignalPage();
  void drawBrandingPage();
  void drawFallbackIpPage();
};
