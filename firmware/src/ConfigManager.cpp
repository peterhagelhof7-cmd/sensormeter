#include "ConfigManager.h"

void ConfigManager::begin() {
  // _config bleibt auf den in DeviceConfig deklarierten Defaultwerten.
  Serial.println("[CONFIG] Defaults geladen (LittleFS-Persistenz folgt in P2)");
}
