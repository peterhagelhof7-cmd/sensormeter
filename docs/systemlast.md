# Systemlast (CPU, RAM, Flash)

Einordnung gegenüber den Performance-Zielwerten aus
[`docs/pflichtenheft.txt`](pflichtenheft.txt) Abschnitt 8:

> CPU load < 40% · RAM usage < 60% · Sensor polling minimal (60s) ·
> Webserver non-blocking (Async)

Drei Datenquellen, klar getrennt nach Belastbarkeit:

1. **Gemessen** – reale `pio run`-Build-Ausgaben (Flash/RAM), festgehalten
   bei jeder Phase.
2. **Simuliert** – ein natives Testprogramm baut dieselben JSON-Antworten
   wie die Firmware nach und zählt echte Heap-Allokationen (Host-Proxy,
   siehe Einschränkung unten).
3. **Abgeschätzt** – CPU-Zeitbudget pro Loop-Tick, hergeleitet aus bekannten
   Bibliotheks-Timings (DHT-Protokoll, I2C-Taktrate). Keine Messung auf
   echter Hardware, aber nachvollziehbare Rechnung statt Bauchgefühl.

## 1. Flash/RAM-Wachstum je Phase (gemessen)

| Phase | Flash | RAM | Zuwachs Flash | Anlass |
|---|---|---|---|---|
| P0 | 61,6 % (807.625 B) | 14,0 % | – | Grundgerüst, Boot-Zustandsautomat |
| P1 | 63,7 % (834.361 B) | 14,6 % | +26.736 B | Netzwerk (LAN/WLAN/Fallback), NTP |
| P2 | 65,3 % (855.701 B) | 14,6 % | +21.340 B | config.xml-Persistenz (tinyxml2, vendort) |
| P3 | 65,6 % (860.381 B) | 14,6 % | +4.680 B | DHT11/DHT22, Ringpuffer |
| P4 | 68,7 % (899.869 B) | 15,1 % | +39.488 B | OLED SSD1306 (Adafruit GFX + SSD1306) |
| P5 | 76,2 % (999.313 B) | 15,3 % | +99.444 B | Webserver (ESPAsyncWebServer/AsyncTCP/ArduinoJson) — **ohne** HTTPS-Client (der hätte allein +168 KB gekostet, siehe `entscheidungen.md`) |
| P6 | 80,1 % (1.049.805 B) | 15,9 % | +50.492 B | SNMP v1 Agent |
| P7 + Chip-Temp | 80,4 % (1.053.697 B) | 16,0 % | +3.892 B | Syslog (UDP) + ESP32-Chip-Temperatur |

**Reserve:** 1.310.720 − 1.053.697 = **257.023 Byte Flash frei (19,6 %)**,
52.280 von 327.680 Byte statisches RAM belegt (16,0 %) → 275.400 Byte für
Heap + Stack zur Laufzeit. Beide Werte stammen direkt aus `pio run` und sind
keine Schätzung.

## 2. Heap-Last der REST-API (simuliert)

Gemessen mit [`firmware/tools/simulate_json_load.cpp`](../firmware/tools/simulate_json_load.cpp)
(natives Programm, ArduinoJson v7, `operator new`/`delete` instrumentiert,
kompiliert mit MinGW-w64 g++ auf einem 64-bit-Host):

| Endpunkt | Heap-Spitze | Heap-Summe | Antwortgröße |
|---|---|---|---|
| `/api/status` | 362 B | 454 B | 189 B |
| `/api/sensors` | 362 B | 454 B | 149 B |
| `/api/network` | 722 B | 935 B | 294 B |
| `/api/config` | 722 B | 935 B | 299 B |
| `/api/logs` (5 Einträge) | 1.442 B | 1.896 B | 497 B |
| `/api/graph` (168 Punkte, voller 7-Tage-Ringpuffer, Worst Case) | 5.762 B | 7.658 B | 2.997 B |

**Einschränkung:** 64-bit-Host-Messung, keine ESP32-Messung. ArduinoJson v7
speichert intern u. a. Zeiger, die auf 32-bit-ESP32 nur halb so groß sind
(4 statt 8 Byte) – reale ESP32-Werte dürften also eher **niedriger** liegen
als hier gemessen, nicht höher. Selbst der Worst-Case-Endpunkt
(`/api/graph`) bleibt mit ~5,8 KB Spitzenlast weit unter der verfügbaren
Heap-Reserve (~275 KB statisch + üblicherweise weitere ~50-100 KB, die vom
Netzwerk-/WLAN-Stack zur Laufzeit noch nicht belegt sind).

## 3. CPU-Zeitbudget pro Loop-Tick (abgeschätzt)

Die Hauptschleife läuft alle 50 ms (`delay(50)` in `main.cpp`), also ~20
Takte/Sekunde. Kosten pro Vorgang, hergeleitet aus bekannten
Bibliotheks-Timings:

| Vorgang | Kosten | Takt | Ø-Last |
|---|---|---|---|
| Basis-Checks aller Manager (Zustands-/Zeitvergleiche) | < 1 ms | jeder Tick (50 ms) | < 2 % |
| DHT11/DHT22-Auslesung (blockierend, Protokoll-Timing) | ~20–25 ms | alle 60 s | ~0,04 % |
| OLED-Seitenwechsel (volles I2C-Frame, 400 kHz) | ~20–25 ms | alle 10 s (Normalbetrieb) | ~0,22 % |
| OLED-Boot-Countdown | ~20–25 ms | alle 1 s (nur waehrend Boot/Netzwerk-Check) | ~2,2 % |

**Ø-CPU-Last im Normalbetrieb: ~1–3 %** — weit unter dem 40-%-Zielwert.

### Fund unterwegs: Boot-Display redrawte bei jedem Tick, nicht nur bei Aenderung

Beim Durchrechnen fiel auf, dass `DisplayManager::loop()` den Boot-Screen
bisher bei **jedem** 50-ms-Tick neu gezeichnet hat, obwohl sich die
Countdown-Zahl nur 1×/Sekunde ändert. Bei ~20–25 ms pro vollem I2C-Frame
wären das rechnerisch **~40–50 % CPU-Last allein fürs Display**, und zwar
über die gesamte Dauer von `BOOT`/`INIT`/`NETWORK_CHECK` — im
Fallback-Fall laut P1 bis zu 5 Minuten (siehe `entscheidungen.md`,
Netzwerk-Check-Timeout). Das hätte den 40-%-Zielwert in genau der Phase
verletzt, in der ein Nutzer am ehesten aufs Display schaut, um den
Boot-Fortschritt zu verfolgen.

**Behoben** (noch vor dem ersten Hardware-Test): Der Boot-Screen wird jetzt
nur noch neu gezeichnet, wenn sich die Countdown-Zahl tatsächlich ändert
(1×/Sekunde) — reduziert die Last in dieser Phase auf ~2,2 %, siehe Tabelle
oben.

## Fazit

| Zielwert (Pflichtenheft 8) | Rechnerischer Stand |
|---|---|
| CPU load < 40 % | ~1–3 % im Normalbetrieb, ~2–3 % während Boot/Netzwerk-Check (nach Fix) |
| RAM usage < 60 % | 16,0 % statisch + max. ~6 KB Heap-Spitze pro Request → weit darunter |
| Sensor polling minimal (60s) | Umgesetzt (`SensorManager`, `SyslogManager`-Report folgt demselben Takt) |
| Webserver non-blocking (Async) | Umgesetzt (`ESPAsyncWebServer`); bewusste Ausnahmen (WLAN-Scan, OTA-Flash) sind admin-ausgelöste Einzelaktionen, siehe `entscheidungen.md` |

Alle vier Zielwerte werden nach dieser Rechnung komfortabel eingehalten.
Eine echte Messung auf Hardware (z. B. `esp_get_free_heap_size()` im
Dauerbetrieb, `FreeRTOS`-Task-Laufzeiten) bleibt trotzdem sinnvoll, sobald
ein Board zur Verfügung steht – diese Rechnung ersetzt keinen realen
Langzeittest, sie schließt nur die Lücke, bis einer möglich ist.
