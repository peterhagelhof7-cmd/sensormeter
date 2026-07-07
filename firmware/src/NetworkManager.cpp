#include "NetworkManager.h"

#include "pins.h"
#include <ETH.h>  // muss nach pins.h stehen: ETH.h definiert dieselben Makronamen
                  // (ETH_PHY_ADDR, ETH_PHY_POWER, ...) nur als Fallback via #ifndef

NetworkManager* NetworkManager::_instance = nullptr;

// Netzwerk-Check-Timeout, bevor P0 in FALLBACK_MODE wechselt. Nur ein
// Platzhalter fuer den Zustandsautomaten - der eigentliche WLAN-Fallback
// "installer" nach 5 Minuten (siehe Lastenheft) wird erst in P1 gebaut.
static const unsigned long NETWORK_CHECK_TIMEOUT_MS = 10000;

NetworkManager::NetworkManager(DataManager& dataManager) : _data(dataManager) {
  _instance = this;
}

void NetworkManager::onEthEvent(WiFiEvent_t event) {
  if (!_instance) return;

  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("[NET] Ethernet gestartet");
      // TODO (P2): Hostname aus ConfigManager (systemName) statt Platzhalter
      ETH.setHostname("sensormeter");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("[NET] Ethernet-Kabel verbunden");
      _instance->_ethConnected = true;
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("[NET] IP-Adresse erhalten: ");
      Serial.println(ETH.localIP());
      _instance->_ethGotIp = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("[NET] Ethernet-Kabel getrennt");
      _instance->_ethConnected = false;
      _instance->_ethGotIp = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("[NET] Ethernet gestoppt");
      _instance->_ethConnected = false;
      _instance->_ethGotIp = false;
      break;
    default:
      break;
  }
}

void NetworkManager::begin() {
  _data.setSystemState(SystemState::INIT);
  Serial.println("[NET] Init: Ethernet (DHCP)");

  WiFi.onEvent(onEthEvent);
  ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_TYPE, ETH_CLK_MODE);

  _data.setSystemState(SystemState::NETWORK_CHECK);
  _networkCheckStartedMillis = millis();
}

void NetworkManager::loop() {
  SystemState state = _data.getSystemState();

  if (state == SystemState::NETWORK_CHECK) {
    if (_ethGotIp) {
      _data.setSystemState(SystemState::RUN_NORMAL);
    } else if (millis() - _networkCheckStartedMillis > NETWORK_CHECK_TIMEOUT_MS) {
      // TODO (P1): hier WLAN-Fallback "installer"/"installer" starten statt
      // nur zu loggen.
      Serial.println("[NET] Kein Ethernet-Link -> FALLBACK_MODE (WLAN-Fallback folgt in P1)");
      _data.setSystemState(SystemState::FALLBACK_MODE);
    }
  } else if (state == SystemState::RUN_NORMAL && !_ethGotIp) {
    Serial.println("[NET] Verbindung verloren -> NETWORK_CHECK");
    _data.setSystemState(SystemState::NETWORK_CHECK);
    _networkCheckStartedMillis = millis();
  }
}
