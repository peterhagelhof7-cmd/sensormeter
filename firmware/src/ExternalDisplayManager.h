#pragma once

#include <Arduino.h>
#include "BrandingManager.h"
#include "ConfigManager.h"
#include "DataManager.h"
#include "NetworkManager.h"

// Optionales externes Anzeige-Modul (Kategorie 1, aber kein Sensor): SH1107
// 1,5" 128x128 I2C an Adresse 0x3D (0x3C ist vom internen SSD1306 belegt,
// siehe DisplayManager) - siehe sensormeter-family/repo/module-design/
// sh1107-display-modul.md fuer die Hardwareseite. Rein additiv: zeigt
// dieselben Infoseiten wie das interne Display (Systemname/-typ, IPs,
// Uhrzeit, Sensorwerte, Status, WLAN-Signal, optional Branding), unabhaengig
// mit derselben 10s-Rotation, nur auf groesserer Flaeche - eigene, von
// DisplayManager unabhaengige Zeitbasis statt eines synchronisierten
// Zustands, das ist fuer eine reine Zusatzanzeige einfacher und robust
// genug. Bewusst OHNE Boot-Countdown-Seite und Fallback-AP-Sonderseite -
// diese sind an den Boot-/Reset-Ablauf des Geraets gebunden und bleiben
// Aufgabe des internen Displays; das externe Modul ist reine Zusatzanzeige
// fuer den Normalbetrieb (siehe „Bekannte Einschraenkungen" im
// Modul-Dokument). Fehlt das Modul, bleibt begin() erfolglos und loop()
// ist ein no-op - identisches Verhalten zum internen Display bei
// fehlendem Chip.

class ExternalDisplayManager {
 public:
  ExternalDisplayManager(DataManager& dataManager, ConfigManager& configManager,
                         NetworkManager& networkManager, BrandingManager& brandingManager);

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
  static const int BASE_PAGE_COUNT = 6;
  int pageCount() const { return _branding.isActive() ? BASE_PAGE_COUNT + 1 : BASE_PAGE_COUNT; }

  void drawLines(const String lines[], int count);
  void drawScrollingLine(const String& text, int y, int size, float progress);
  void drawPage(int page);
  void drawSystemNamePage();
  void drawIpsPage();
  void drawTimePage();
  void drawSensorsPage();
  void drawStatusPage();
  void drawSignalPage();
  void drawBrandingPage();
};
