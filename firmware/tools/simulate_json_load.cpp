// Native (Desktop-)Simulation der Speicherlast der REST-API-JSON-Antworten.
// Baut dieselben JsonDocument-Strukturen wie firmware/src/WebServerManager.cpp
// mit realistischen Beispielwerten nach. ArduinoJson v7's memoryUsage() ist
// deprecated (liefert immer 0), daher wird operator new/delete
// instrumentiert, um die tatsaechlich vom Heap angeforderten Bytes zu
// zaehlen (kumulativ + Spitzenwert waehrend des Aufbaus) - ein Host-Proxy,
// keine ESP32-exakte Messung (andere malloc-Implementierung), aber echte
// Zaehlwerte statt einer reinen Schaetzung. Ergebnisse siehe docs/systemlast.md.
//
// Bauen (z.B. mit MinGW-w64 g++ oder jedem anderen C++17-Compiler):
//   g++ -std=c++17 -O2 -static -I <pfad-zu-ArduinoJson>/src simulate_json_load.cpp -o simulate_json_load
//
// <pfad-zu-ArduinoJson> = z.B. firmware/.pio/libdeps/wt32-eth01/ArduinoJson
// (wird beim ersten "pio run" automatisch heruntergeladen).

#include <ArduinoJson.h>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>

static size_t g_current = 0;
static size_t g_peak = 0;
static size_t g_totalAllocated = 0;

void* operator new(size_t size) {
  void* raw = std::malloc(size + sizeof(size_t));
  if (!raw) throw std::bad_alloc();
  *static_cast<size_t*>(raw) = size;
  g_current += size;
  g_totalAllocated += size;
  if (g_current > g_peak) g_peak = g_current;
  return static_cast<char*>(raw) + sizeof(size_t);
}

void operator delete(void* ptr) noexcept {
  if (!ptr) return;
  char* raw = static_cast<char*>(ptr) - sizeof(size_t);
  size_t size = *reinterpret_cast<size_t*>(raw);
  g_current -= size;
  std::free(raw);
}
void operator delete(void* ptr, size_t) noexcept { operator delete(ptr); }

template <typename Func>
static void report(const char* endpoint, Func buildAndSerialize) {
  g_current = 0;
  g_peak = 0;
  g_totalAllocated = 0;

  std::string out = buildAndSerialize();

  printf("%-14s Heap-Spitze=%6zu B   Heap-Summe=%6zu B   response=%5zu B\n", endpoint, g_peak, g_totalAllocated,
         out.size());
}

int main() {
  printf("Simulierte REST-API-Speicherlast (Heap-Instrumentierung + reale Antwortgroessen)\n");
  printf("---------------------------------------------------------------------------------\n");

  report("/api/status", []() -> std::string {
    JsonDocument doc;
    doc["systemName"] = "Sensormeter";
    doc["systemType"] = "Sensormeter PRO";
    doc["firmwareVersion"] = "0.1.0-p7";
    doc["uptimeSeconds"] = 123456UL;
    doc["freeHeap"] = 234567UL;
    doc["chipTemperatureC"] = 45.3f;
    doc["timeSynced"] = true;
    doc["time"] = 1751234567UL;
    std::string out;
    serializeJson(doc, out);
    return out;
  });

  report("/api/sensors", []() -> std::string {
    JsonDocument doc;
    JsonObject s1 = doc["sensor1"].to<JsonObject>();
    s1["name"] = "Intern";
    s1["valid"] = true;
    s1["temperature"] = 21.4f;
    s1["humidity"] = 45.0f;
    JsonObject s2 = doc["sensor2"].to<JsonObject>();
    s2["name"] = "Extern";
    s2["valid"] = true;
    s2["temperature"] = 19.8f;
    s2["humidity"] = 55.0f;
    std::string out;
    serializeJson(doc, out);
    return out;
  });

  report("/api/network", []() -> std::string {
    JsonDocument doc;
    doc["lanUp"] = true;
    doc["lanLinkUp"] = true;
    doc["lanIp"] = "192.168.1.50";
    doc["lanGateway"] = "192.168.1.1";
    doc["lanDns"] = "192.168.1.1";
    doc["lanMac"] = "AA:BB:CC:DD:EE:FF";
    doc["wlanUp"] = false;
    doc["wlanIp"] = "0.0.0.0";
    doc["wlanGateway"] = "0.0.0.0";
    doc["wlanDns"] = "0.0.0.0";
    doc["wlanMac"] = "11:22:33:44:55:66";
    doc["wlanSsid"] = "";
    doc["wlanRssi"] = 0;
    doc["usingFallbackWlan"] = false;
    std::string out;
    serializeJson(doc, out);
    return out;
  });

  report("/api/config", []() -> std::string {
    JsonDocument doc;
    doc["systemName"] = "Sensormeter";
    doc["systemType"] = "Sensormeter PRO";
    doc["lanDhcp"] = true;
    doc["lanIp"] = "";
    doc["lanMask"] = "";
    doc["lanGateway"] = "";
    doc["wlanDhcp"] = true;
    doc["wlanSsid"] = "MeinHeimnetz";
    doc["wlanIp"] = "";
    doc["wlanMask"] = "";
    doc["wlanGateway"] = "";
    doc["sensor2Enabled"] = true;
    doc["sensor2Name"] = "Extern";
    doc["syslogServer"] = "192.168.1.20";
    doc["snmpCommunity"] = "public";
    std::string out;
    serializeJson(doc, out);
    return out;
  });

  report("/api/logs", []() -> std::string {
    JsonDocument doc;
    JsonArray arr = doc["entries"].to<JsonArray>();
    const char* messages[5] = {
        "Sensor intern: Fehler beim Lesen des DHT11",
        "Netzwerk: kein Link nach 5 Minuten, wechsle auf Recovery-WLAN \"installer\"",
        "NTP: 5 Minuten nicht erreichbar, DHCP-Test gestartet",
        "Einstellungen gespeichert (Reboot noetig fuer Netzwerk-/SNMP-Aenderungen)",
        "OTA (lokaler Upload) erfolgreich, Neustart"};
    for (int i = 0; i < 5; i++) {
      JsonObject o = arr.add<JsonObject>();
      o["time"] = "08.07. 14:32:10";
      o["message"] = messages[i];
    }
    std::string out;
    serializeJson(doc, out);
    return out;
  });

  report("/api/graph", []() -> std::string {
    // Worst Case: voll befuellter 7-Tage-Ringpuffer (168 Stundenwerte)
    JsonDocument doc;
    JsonArray labels = doc["labels"].to<JsonArray>();
    JsonArray temps = doc["temperature"].to<JsonArray>();
    JsonArray hums = doc["humidity"].to<JsonArray>();

    for (int i = 0; i < 168; i++) {
      char buf[6];
      snprintf(buf, sizeof(buf), "%02d:%02d", i % 24, (i * 17) % 60);
      labels.add(std::string(buf));
      temps.add(21.3f + (i % 10) * 0.1f);
      hums.add(45.0f + (i % 20) * 0.1f);
    }
    std::string out;
    serializeJson(doc, out);
    return out;
  });

  return 0;
}
