#pragma once

#include <Arduino.h>
#include "ConfigManager.h"
#include "DataManager.h"

// Automatische Erkennung des RJ45-Steckmoduls (portiert aus
// sensormeter-poe/repo/firmware/src/SensorDetector.h - siehe dessen
// docs/lastenheft.txt Abschnitt 15, Pflichtenheft 3.8, sowie
// docs/entscheidungen.md "Portierungs-Kandidaten aus sensormeter-poe
// geprüft"). Laeuft einmal beim Boot (waehrend des Countdowns, parallel zur
// Netzwerk-Wartezeit - siehe main.cpp) sowie erneut auf Anfrage ueber die
// Einstellungsseite ("Erkennung neu starten"). Erkennt NUR die Modul-
// KATEGORIE (DHT-Sensor / I2C-Sensor inkl. konkretem Chip bei bekannter
// Adresse / kein Sensor) - AUSDRUECKLICH NICHT DHT-11 vs. DHT-22
// (unzuverlaessig) und NICHT das Vorhandensein eines Relais-Moduls (kein
// auswertbares Rueck-Signal).

enum class ModuleType {
  NONE,
  DHT_SENSOR,
  I2C_SENSOR_KNOWN,
  I2C_SENSOR_UNKNOWN,
};

class SensorDetector {
 public:
  SensorDetector(DataManager& dataManager, ConfigManager& configManager);

  void begin();

  // Fuehrt den eigentlichen Scan durch (I2C-Scan, bei Fehlschlag DHT-
  // Leseversuch). Setzt bei einem Treffer automatisch cfg.sensor2Enabled=true
  // (falls noch nicht gesetzt); setzt es NICHT automatisch auf false, wenn
  // nichts gefunden wird - ein manuell aktivierter Schalter soll durch einen
  // fehlgeschlagenen/verpassten Scan nicht stillschweigend wieder
  // deaktiviert werden.
  void runDetection();

  ModuleType detectedType() const { return _detectedType; }
  // Menschenlesbare Beschreibung fuer Einstellungsseite/Log, z.B.
  // "I2C-Sensor (BME280)", "I2C-Sensor (unbekannt, 0x42)", "DHT-Sensor",
  // "Kein Sensor / Relais".
  String detectedDescription() const;
  // Roher Chipname aus KNOWN_CHIPS (z.B. "BME280", "AHT20/AHT21"), nur bei
  // I2C_SENSOR_KNOWN gueltig, sonst "" - fuer SensorManager, um den
  // passenden I2C-Lesepfad zu waehlen (siehe readExternalSensorIfEnabled()).
  String detectedChipName() const { return _detectedChipName; }
  // Gefundene I2C-Adresse, nur bei I2C_SENSOR_KNOWN/_UNKNOWN gueltig -
  // fuer SensorManager, da z.B. BME280 zwei moegliche Adressen hat
  // (0x76/0x77, siehe SDO-Pin).
  uint8_t detectedI2cAddress() const { return _detectedI2cAddress; }

 private:
  DataManager& _data;
  ConfigManager& _config;

  ModuleType _detectedType = ModuleType::NONE;
  String _detectedChipName;     // nur bei I2C_SENSOR_KNOWN
  uint8_t _detectedI2cAddress = 0;  // nur bei I2C_SENSOR_KNOWN/_UNKNOWN

  // Liefert bei bekannter Adresse den Chipnamen, sonst "" (erweiterbare
  // Tabelle).
  static String knownChipName(uint8_t address);
};
