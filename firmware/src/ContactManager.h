#pragma once

#include <Arduino.h>
#include "ConfigManager.h"
#include "DataManager.h"

// Tuerkontakt/Reed-Kontakt an RJ45 Pin 5 (Kategorie-2-Direktmodul, siehe
// sensormeter-family/repo/module-design/README.md) - teilt sich den Pin mit
// dem DHT22-Sensormodul (Sensor 2); cfg.pin5Mode legt fest, welche Belegung
// aktiv ist (Einstellungsseite, Abschnitt "Sensoren", rein manuelle Wahl -
// keine Auto-Erkennung, siehe module-design/README.md "Bekannte Einschraenkung
// der Auto-Erkennung fuer einen kuenftigen Tuerkontakt"). Eigener, binaerer
// Datenpfad statt Wiederverwendung von Sensor 2 (Temperatur/Feuchte passt
// nicht). Pull-up sitzt auf dem Modul - das Geraet nutzt INPUT_PULLUP nur als
// Rueckfallebene, falls kein Modul gesteckt ist (liest dann dauerhaft
// "geschlossen", elektrisch nicht von einem offenen Kontakt unterscheidbar).

class ContactManager {
 public:
  ContactManager(DataManager& dataManager, ConfigManager& configManager);

  void begin();
  // Liest den Pin nur, wenn cfg.pin5Mode == "contact"; protokolliert
  // Zustandswechsel per pushLogEntry.
  void loop();

  bool isClosed() const { return _closed; }
  // Konfigurierter Meldungstext fuer den jeweils aktuellen Zustand
  // (contactMessageOpen/contactMessageClosed).
  String stateText() const;

 private:
  DataManager& _data;
  ConfigManager& _config;
  bool _closed = false;
  bool _stateKnown = false;
};
