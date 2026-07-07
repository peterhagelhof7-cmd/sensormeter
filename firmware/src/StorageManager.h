#pragma once

// LittleFS-Zugriff (config.xml, values.csv). Volle Persistenzlogik folgt in
// P2 (Konfiguration) / P3 (Ringpuffer-Export). P0 stellt nur sicher, dass
// das Dateisystem gemountet werden kann.

class StorageManager {
 public:
  bool begin();  // true = LittleFS erfolgreich gemountet
};
