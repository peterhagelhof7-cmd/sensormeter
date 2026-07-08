# tools/

Entwickler-Werkzeuge, die nicht mit auf das Gerät geflasht werden.

## simulate_json_load.cpp

Native (Desktop-)Simulation der Heap-Last der REST-API-JSON-Antworten aus
`WebServerManager`. Baut dieselben `JsonDocument`-Strukturen mit
realistischen Beispielwerten nach und zaehlt die tatsaechlich vom Heap
angeforderten Bytes (Spitze + Summe) sowie die serialisierte Antwortgroesse.
Ergebnisse und Einordnung: [`docs/systemlast.md`](../../docs/systemlast.md).

Bauen (beliebiger C++17-Compiler, hier MinGW-w64 als Beispiel):

```
g++ -std=c++17 -O2 -static -I ../.pio/libdeps/wt32-eth01/ArduinoJson/src simulate_json_load.cpp -o simulate_json_load
./simulate_json_load
```

Der Include-Pfad zeigt auf die von PlatformIO bereits heruntergeladene
ArduinoJson-Bibliothek (entsteht automatisch nach dem ersten `pio run` im
`firmware/`-Verzeichnis).
