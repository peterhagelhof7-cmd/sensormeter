#include "ContactManager.h"

#include "pins.h"

ContactManager::ContactManager(DataManager& dataManager, ConfigManager& configManager)
    : _data(dataManager), _config(configManager) {}

void ContactManager::begin() {
  // Unabhaengig vom aktuellen pin5Mode gesetzt - harmlos im Sensor-Modus,
  // da die DHT-Bibliothek den Pin bei jedem Leseversuch ohnehin selbst
  // umkonfiguriert (siehe SensorManager/SensorDetector).
  pinMode(PIN_RJ45_PIN5_RESERVE, INPUT_PULLUP);
  Serial.println("[CONTACT] Grundgeruest bereit");
}

void ContactManager::loop() {
  const DeviceConfig& cfg = _config.getConfig();
  if (cfg.pin5Mode != "contact") return;

  bool closed = digitalRead(PIN_RJ45_PIN5_RESERVE) == LOW;
  if (_stateKnown && closed == _closed) return;

  _closed = closed;
  if (_stateKnown) {
    _data.pushLogEntry(cfg.contactName + ": " + stateText());
  }
  _stateKnown = true;
}

String ContactManager::stateText() const {
  const DeviceConfig& cfg = _config.getConfig();
  return _closed ? cfg.contactMessageClosed : cfg.contactMessageOpen;
}
