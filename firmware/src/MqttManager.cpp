#include "MqttManager.h"

#include <ArduinoJson.h>

namespace {
const unsigned long RECONNECT_INTERVAL_MS = 5000;
}  // namespace

MqttManager::MqttManager(DataManager& dataManager, ConfigManager& configManager, NetworkManager& networkManager)
    : _data(dataManager), _config(configManager), _network(networkManager), _client(_transport) {}

void MqttManager::begin() {
  Serial.println("[MQTT] Grundgeruest bereit");
}

bool MqttManager::mqttEnabled() const {
  const DeviceConfig& cfg = _config.getConfig();
  return cfg.mqttEnabled && cfg.mqttServer.length() > 0;
}

String MqttManager::topicPrefix() const {
  return NetworkManager::sanitizeHostname(_config.getConfig().systemName);
}

void MqttManager::ensureConnected() {
  const DeviceConfig& cfg = _config.getConfig();

  // Server/Port koennten sich seit dem letzten setServer()-Aufruf geaendert
  // haben (Einstellungsseite) - PubSubClient uebernimmt neue Werte erst
  // nach dem naechsten setServer()-Aufruf, daher hier bei jedem
  // Verbindungsversuch neu setzen (billig, kein Netzwerkzugriff).
  _client.setServer(cfg.mqttServer.c_str(), cfg.mqttPort);

  unsigned long now = millis();
  if (now - _lastReconnectAttemptMillis < RECONNECT_INTERVAL_MS) return;
  _lastReconnectAttemptMillis = now;

  String clientId = "sensormeter-" + topicPrefix();
  bool ok;
  if (cfg.mqttUser.length() > 0) {
    ok = _client.connect(clientId.c_str(), cfg.mqttUser.c_str(), cfg.mqttPassword.c_str());
  } else {
    ok = _client.connect(clientId.c_str());
  }

  if (ok) {
    Serial.println("[MQTT] Verbunden mit Broker " + cfg.mqttServer);
    _discoverySent = false;  // nach jedem (Re-)Connect neu ankuendigen -
                              // guenstig genug, kein Persistenz-Aufwand noetig
  }
}

void MqttManager::publishDiscovery() {
  String prefix = topicPrefix();
  const DeviceConfig& cfg = _config.getConfig();
  String stateTopic = prefix + "/state";

  // Gemeinsamer "device"-Block, damit Home Assistant alle Entities einem
  // Geraet zuordnet statt loser Sensoren.
  auto publishSensorDiscovery = [&](const char* key, const char* name, const char* deviceClass,
                                     const char* unit, const char* valueTemplate) {
    JsonDocument doc;
    doc["name"] = name;
    doc["device_class"] = deviceClass;
    doc["unit_of_measurement"] = unit;
    doc["state_topic"] = stateTopic;
    doc["value_template"] = valueTemplate;
    doc["unique_id"] = prefix + "_" + key;
    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = prefix;
    device["name"] = cfg.systemName;
    device["manufacturer"] = "Sensormeter-Familie";
    device["model"] = cfg.systemType;

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + prefix + "/" + key + "/config";
    _client.publish(topic.c_str(), payload.c_str(), true);  // retain=true
  };

  publishSensorDiscovery("temperature1", "Temperatur (intern)", "temperature", "°C",
                          "{{ value_json.temperature1 }}");
  publishSensorDiscovery("humidity1", "Luftfeuchte (intern)", "humidity", "%", "{{ value_json.humidity1 }}");

  if (cfg.sensor2Enabled) {
    String label2 = cfg.sensor2Name.length() > 0 ? cfg.sensor2Name : String("extern");
    publishSensorDiscovery("temperature2", ("Temperatur (" + label2 + ")").c_str(), "temperature", "°C",
                            "{{ value_json.temperature2 }}");
    publishSensorDiscovery("humidity2", ("Luftfeuchte (" + label2 + ")").c_str(), "humidity", "%",
                            "{{ value_json.humidity2 }}");
  }

  _discoverySent = true;
  Serial.println("[MQTT] Discovery-Payloads gesendet");
}

void MqttManager::publishState() {
  SensorReading s1 = _data.getSensor1();
  if (!s1.valid) return;

  const DeviceConfig& cfg = _config.getConfig();

  JsonDocument doc;
  doc["temperature1"] = serialized(String(s1.temperature, 1));
  doc["humidity1"] = serialized(String(s1.humidity, 1));

  if (cfg.sensor2Enabled) {
    SensorReading s2 = _data.getSensor2();
    if (s2.valid) {
      doc["temperature2"] = serialized(String(s2.temperature, 1));
      doc["humidity2"] = serialized(String(s2.humidity, 1));
    }
  }

  String payload;
  serializeJson(doc, payload);
  String topic = topicPrefix() + "/state";
  _client.publish(topic.c_str(), payload.c_str());
}

void MqttManager::loop() {
  if (!mqttEnabled() || !(_network.isLanUp() || _network.isWlanUp())) return;

  if (!_client.connected()) {
    ensureConnected();
    return;  // im selben Tick noch nicht weitermachen, erst naechster loop()
  }
  _client.loop();

  if (!_discoverySent) {
    publishDiscovery();
  }

  // State-Update bei jedem Sensorzyklus (Aenderung von Sensor 1's
  // lastReadMillis) - gleiches Erkennungsmuster wie SyslogManager::loop().
  unsigned long currentReadMillis = _data.getSensor1().lastReadMillis;
  if (currentReadMillis != 0 && currentReadMillis != _lastSensorReadMillisSeen) {
    _lastSensorReadMillisSeen = currentReadMillis;
    publishState();
  }
}
