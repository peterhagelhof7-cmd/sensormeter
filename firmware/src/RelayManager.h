#pragma once

#include <Arduino.h>
#include "ConfigManager.h"
#include "DataManager.h"

// Relais/Aktor-Steuerung (portiert aus
// sensormeter-poe/repo/firmware/src/RelayManager.h - siehe dessen
// docs/lastenheft.txt Abschnitt 16.2, Pflichtenheft 3.9, sowie
// docs/entscheidungen.md "Portierungs-Kandidaten aus sensormeter-poe
// geprüft"): treibt RJ45 Pin 6 (Steuersignal, active LOW) an, liest
// optional RJ45 Pin 7 (Status-Feedback) zurueck. Nur wirksam, wenn
// cfg.relayEnabled gesetzt ist (Einstellungsseite "Relais (Aktor) aktiv")
// - unabhaengig von "Sensor 2 aktiv", da die RJ45-Pins sich nicht
// ueberschneiden (siehe pins.h). Startet nach jedem Boot sicherheitshalber
// immer mit AUS (kein persistierter Schaltzustand) - ein wieder
// angelaufenes Geraet soll nicht unerwartet einen zuvor eingeschalteten
// Verbraucher stumm reaktivieren. Einziger Schreibpfad fuer den Zustand
// (setOn()), gemeinsam genutzt von Weboberflaeche, REST-API (/api/relay)
// und MqttManager (command_topic).

class RelayManager {
 public:
  RelayManager(DataManager& dataManager, ConfigManager& configManager);

  void begin();

  // Ohne Wirkung, wenn cfg.relayEnabled == false (Sicherheitsnetz - Aufrufer
  // sollten den Schalter in der jeweiligen Oberflaeche ohnehin ausblenden).
  void setOn(bool on);
  bool isOn() const { return _relayOn; }

  // true, wenn das Modul eine Feedback-Leitung liefert (RJ45 Pin 7 aktiv,
  // active LOW wie das Steuersignal) - rein informativ, kein Einfluss auf
  // isOn() (das ist der zuletzt KOMMANDIERTE Zustand, nicht der gemessene).
  bool feedbackOn() const;

 private:
  DataManager& _data;
  ConfigManager& _config;
  bool _relayOn = false;
};
