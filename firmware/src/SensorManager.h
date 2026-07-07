#pragma once

#include <Arduino.h>
#include "ConfigManager.h"
#include "DataManager.h"

// Sensor-Auslesung (Pflichtenheft-Task "SensorTask", hier kooperativ wie
// NetworkManager/TimeManager - siehe docs/entscheidungen.md). DHT11 intern
// ist immer aktiv, DHT22 extern nur wenn per ConfigManager aktiviert
// (Sensormeter PRO). Takt: 60s (Lastenheft: "Sensoren sollen nur 1x je
// Minute gelesen werden"). Stuendliche Werte des internen Sensors wandern
// zusaetzlich in den 7-Tage-Ringpuffer des DataManager (Web-Graph, P5).

class SensorManager {
 public:
  SensorManager(DataManager& dataManager, ConfigManager& configManager);

  void begin();
  void loop();

 private:
  DataManager& _data;
  ConfigManager& _config;

  unsigned long _lastReadMillis = 0;
  long _lastRecordedHour = -1;

  void readInternalSensor();
  void readExternalSensorIfEnabled();
};
