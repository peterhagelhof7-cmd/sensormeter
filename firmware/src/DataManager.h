#pragma once

#include <Arduino.h>
#include <freertos/semphr.h>
#include "SystemState.h"

// Zentrale Datenhaltung (Pflichtenheft 3.1): Sensorwerte, Systemstatus,
// Zeitstempel, 7-Tage-Ringpuffer. Thread-safe fuer den Zugriff aus mehreren
// Tasks (Sensor/Network/Web/Display/Syslog), die in spaeteren Phasen dazukommen.

struct HourValue {
  time_t timestamp = 0;
  float temperature = NAN;
  float humidity = NAN;
};

struct SensorReading {
  float temperature = NAN;
  float humidity = NAN;
  bool valid = false;
  unsigned long lastReadMillis = 0;
};

struct LogEntry {
  time_t timestamp = 0;
  String message;
  int severity = 6;      // Syslog-Konvention: 3 = Error, 6 = Informational
  unsigned long sequence = 0;  // fortlaufend, damit SyslogManager (P7) neue
                                // Eintraege erkennen kann, ohne zu pollen
                                // "welcher war der letzte Text"
};

class DataManager {
 public:
  static const size_t RINGBUFFER_SIZE = 168;  // 7 Tage * 24 Stunden
  static const size_t LOG_CAPACITY = 5;       // Lastenheft 5.3: "letzte 5 Meldungen"

  void begin();

  SystemState getSystemState();
  void setSystemState(SystemState state);

  SensorReading getSensor1();
  void setSensor1(const SensorReading& reading);

  SensorReading getSensor2();
  void setSensor2(const SensorReading& reading);

  // Ringpuffer-Speicher ist ab P0 reserviert; Befuellung folgt in P3 (Sensorik)
  // bzw. Anzeige im Graph in P5 (Webserver).
  void pushHourValue(const HourValue& value);
  size_t getRingbuffer(HourValue* out, size_t maxCount);

  // Lokales Ereignisprotokoll (Lastenheft 5.3 "Syslog-Ansicht": letzte 5
  // Meldungen). Wird von SyslogManager (P7) zusaetzlich per UDP versendet -
  // dieselbe Quelle fuer beides. severity folgt der Syslog-Konvention
  // (3 = Error, 6 = Informational).
  void pushLogEntry(const String& message, int severity = 6);
  size_t getLogEntries(LogEntry* out, size_t maxCount);  // neueste zuerst

  // Fuer SyslogManager: liefert nur Eintraege mit sequence > afterSequence
  // (chronologisch), damit Fehler-Events nicht doppelt oder verpasst werden.
  size_t getLogEntriesAfter(unsigned long afterSequence, LogEntry* out, size_t maxCount);

 private:
  SemaphoreHandle_t _mutex = nullptr;

  SystemState _systemState = SystemState::BOOT;
  SensorReading _sensor1;
  SensorReading _sensor2;

  HourValue _ringbuffer[RINGBUFFER_SIZE];
  size_t _ringbufferCount = 0;
  size_t _ringbufferNextIndex = 0;

  LogEntry _log[LOG_CAPACITY];
  size_t _logCount = 0;
  size_t _logNextIndex = 0;
  unsigned long _logSequenceCounter = 0;
};
