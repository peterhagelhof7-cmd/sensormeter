#include "NetworkManager.h"

#include "pins.h"
#include <ETH.h>  // muss nach pins.h stehen: ETH.h definiert dieselben Makronamen
                  // (ETH_PHY_ADDR, ETH_PHY_POWER, ...) nur als Fallback via #ifndef

NetworkManager* NetworkManager::_instance = nullptr;

// "Netzwerk-Check": solange kein Interface eine IP hat, warten wir laut
// Lastenheft 5 Minuten, bevor auf das Recovery-WLAN "installer" umgeschaltet
// wird (siehe docs/entscheidungen.md).
static const unsigned long NETWORK_CHECK_TIMEOUT_MS = 5UL * 60UL * 1000UL;
static const unsigned long FALLBACK_RETRY_INTERVAL_MS = 30UL * 1000UL;
static const char* FALLBACK_WLAN_SSID = "installer";
static const char* FALLBACK_WLAN_PSK = "installer";

NetworkManager::NetworkManager(DataManager& dataManager, ConfigManager& configManager)
    : _data(dataManager), _config(configManager) {
  _instance = this;
}

void NetworkManager::onNetworkEvent(WiFiEvent_t event) {
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
      Serial.print("[NET] LAN-IP erhalten: ");
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

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[NET] WLAN verbunden");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("[NET] WLAN-IP erhalten: ");
      Serial.println(WiFi.localIP());
      _instance->_wlanGotIp = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("[NET] WLAN-Verbindung verloren");
      _instance->_wlanGotIp = false;
      break;

    default:
      break;
  }
}

void NetworkManager::applyLanConfig() {
  const DeviceConfig& cfg = _config.getConfig();
  if (cfg.lanDhcp) return;  // Default nach ETH.begin() ist bereits DHCP

  IPAddress ip, mask, gateway;
  if (ip.fromString(cfg.lanIp) && mask.fromString(cfg.lanMask) && gateway.fromString(cfg.lanGateway)) {
    ETH.config(ip, gateway, mask);
    Serial.println("[NET] LAN: statische IP angewendet");
  } else {
    Serial.println("[NET] LAN: statische IP konfiguriert, aber ungueltig -> bleibe bei DHCP");
  }
}

void NetworkManager::applyWlanConfig() {
  const DeviceConfig& cfg = _config.getConfig();
  if (cfg.wlanDhcp) return;

  IPAddress ip, mask, gateway;
  if (ip.fromString(cfg.wlanIp) && mask.fromString(cfg.wlanMask) && gateway.fromString(cfg.wlanGateway)) {
    WiFi.config(ip, gateway, mask);
    Serial.println("[NET] WLAN: statische IP angewendet");
  } else {
    Serial.println("[NET] WLAN: statische IP konfiguriert, aber ungueltig -> bleibe bei DHCP");
  }
}

bool NetworkManager::hasStaticConfig() const {
  const DeviceConfig& cfg = _config.getConfig();
  return !cfg.lanDhcp || !cfg.wlanDhcp;
}

void NetworkManager::beginDhcpFallbackTest() {
  Serial.println("[NET] NTP nicht erreichbar -> DHCP-Test auf statisch konfigurierten Interfaces");
  const DeviceConfig& cfg = _config.getConfig();
  if (!cfg.lanDhcp) ETH.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
  if (!cfg.wlanDhcp) WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
}

void NetworkManager::restoreConfiguredAddresses() {
  Serial.println("[NET] DHCP-Test erfolglos -> gesetzte IP-Konfiguration wiederherstellen");
  applyLanConfig();
  applyWlanConfig();
}

void NetworkManager::begin() {
  _data.setSystemState(SystemState::INIT);
  Serial.println("[NET] Init: Ethernet + ggf. WLAN");

  WiFi.onEvent(onNetworkEvent);

  ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_TYPE, ETH_CLK_MODE);
  applyLanConfig();

  const DeviceConfig& cfg = _config.getConfig();
  _wlanConfigured = cfg.wlanSsid.length() > 0;
  if (_wlanConfigured) {
    WiFi.mode(WIFI_MODE_STA);
    WiFi.begin(cfg.wlanSsid.c_str(), cfg.wlanPsk.c_str());
    applyWlanConfig();
    Serial.println("[NET] WLAN-Verbindungsversuch gestartet (konfiguriertes Netz)");
  }

  _data.setSystemState(SystemState::NETWORK_CHECK);
  _networkCheckStartedMillis = millis();
}

void NetworkManager::loop() {
  SystemState state = _data.getSystemState();

  if (state == SystemState::NETWORK_CHECK) {
    if (networkOk()) {
      _data.setSystemState(SystemState::RUN_NORMAL);
    } else if (millis() - _networkCheckStartedMillis > NETWORK_CHECK_TIMEOUT_MS) {
      Serial.println("[NET] Kein Netzwerk nach 5 Minuten -> FALLBACK_MODE (Recovery-WLAN \"installer\")");
      WiFi.mode(WIFI_MODE_STA);
      WiFi.disconnect();
      WiFi.begin(FALLBACK_WLAN_SSID, FALLBACK_WLAN_PSK);
      _inFallbackWlan = true;
      _lastFallbackJoinAttemptMillis = millis();
      _data.setSystemState(SystemState::FALLBACK_MODE);
    }
  } else if (state == SystemState::FALLBACK_MODE) {
    if (networkOk()) {
      _data.setSystemState(SystemState::RUN_NORMAL);
    } else if (millis() - _lastFallbackJoinAttemptMillis > FALLBACK_RETRY_INTERVAL_MS) {
      WiFi.begin(FALLBACK_WLAN_SSID, FALLBACK_WLAN_PSK);
      _lastFallbackJoinAttemptMillis = millis();
    }
  } else if (state == SystemState::RUN_NORMAL && !networkOk()) {
    Serial.println("[NET] Verbindung verloren -> NETWORK_CHECK");
    _inFallbackWlan = false;
    _data.setSystemState(SystemState::NETWORK_CHECK);
    _networkCheckStartedMillis = millis();
  }
}
