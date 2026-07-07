#include "TimeManager.h"

#include <Arduino.h>

void TimeManager::begin() {
  Serial.println("[TIME] Grundgeruest bereit (NTP-Logik folgt in P1)");
}

void TimeManager::loop() {
  // TODO (P1): NTP-Sync 60s nach Boot, alle 5h, nach Link-Up; Sommerzeit;
  // Fehlerkette NTP-Fail -> DHCP-Test -> Restore.
}
