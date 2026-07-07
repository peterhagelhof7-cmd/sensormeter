# Sensormeter (WT32-ETH01)

ESP32-basierter Umweltsensor (Temperatur/Luftfeuchte) mit kabelgebundenem
Ethernet (WT32-ETH01, LAN8720), OLED-Anzeige, Webserver, SNMP v1 (read-only)
und Syslog-Versand. Zwei Varianten: **Sensormeter** (1 interner Sensor) und
**Sensormeter PRO** (zusätzlich 1 externer Sensor über RJ45).

## Dokumentation

| Datei | Inhalt |
|---|---|
| [docs/lastenheft.txt](docs/lastenheft.txt) | Fachliche Anforderungen: Webseite, Einstellungen, SNMP-OIDs, Netzwerklogik, Zustandsmodell |
| [docs/pflichtenheft.txt](docs/pflichtenheft.txt) | Technische Umsetzung: FreeRTOS-Tasks, Softwaremodule, Speicherlayout, Fehlerbehandlung |
| [docs/verdrahtungsschema-v1.1.pdf](docs/verdrahtungsschema-v1.1.pdf) | Aktuelles, korrigiertes Verdrahtungsschema (Pinbelegung WT32-ETH01, Display, DHT11, RJ45-Modularanschluss) |
| [docs/flash-vorbereitung.pdf](docs/flash-vorbereitung.pdf) | Schritt-für-Schritt-Anleitung zum Flash-bereit-Machen (Boot-Modus, Verkabelung zum Flash-PC) |
| [docs/pinout-wt32-eth01-v1.4.txt](docs/pinout-wt32-eth01-v1.4.txt) | Rohes Pinout-Referenzblatt des Boards laut Datenblatt |
| [docs/implementierungsplan.html](docs/implementierungsplan.html) | Visueller Implementierungsplan: Reihenfolge P0–P8 vom Prototyp zur vollständigen Firmware (lokal im Browser öffnen) |
| [docs/stueckliste.md](docs/stueckliste.md) | Bauteile pro Gerät + einmaliges Flash-Werkzeug |
| [docs/entscheidungen.md](docs/entscheidungen.md) | Entscheidungsprotokoll: Verkabelungsstand, SNMP-Version, Zabbix-Scope, Repo-Kuration |

## Firmware

`firmware/` ist ein PlatformIO-Projekt (Board `esp32dev`, Framework Arduino).
Aktueller Stand: **P0 — Grundgerüst & Zustandsmodell** (siehe
[docs/implementierungsplan.html](docs/implementierungsplan.html)).

```
cd firmware
cp include/config.h.example include/config.h
pio run              # bauen
pio run --target upload   # flashen (Board am Debug-Port angeschlossen, siehe flash-vorbereitung.pdf)
pio device monitor   # seriellen Log ansehen (115200 Baud)
```

Enthalten in P0:
- Modulgerüst: `DataManager`, `ConfigManager`, `NetworkManager`, `TimeManager`, `StorageManager`
- Boot-Zustandsautomat (`BOOT → INIT → NETWORK_CHECK → RUN_NORMAL / FALLBACK_MODE`), siehe `include/SystemState.h`
- Ethernet-Bring-up per DHCP (treibt den Zustandsautomaten mit echten Link-/IP-Events)
- Korrigierte Pinbelegung v1.1 zentral in `include/pins.h`

Bewusst noch nicht enthalten (folgt in P1–P8, siehe Implementierungsplan):
statische IP, WLAN-Fallback "installer", NTP, `config.xml`-Persistenz,
Sensor-Auslesung, Display, Webserver, SNMP v1, Syslog.

## Stand der Verdrahtung

**`docs/verdrahtungsschema-v1.1.pdf` ist der einzig gültige Verdrahtungsstand.**
Er korrigiert zwei Fehler einer früheren Version:

- Display-I2C liegt auf **IO32/IO33** (nicht IO21/IO22 – die sind fest mit
  dem Ethernet-PHY verdrahtet)
- RJ45-Pin 5 liegt auf **IO15** (nicht IO4 – das kollidierte mit dem
  internen DHT11-Datenpin)

Ältere Verkabelungsnotizen aus der ursprünglichen Materialsammlung
(`RJ45 GPIO.txt`, `RJ45 i2c.txt`, `RJ45 Dht.txt`, `RJ45 buchse.txt`,
`verdrathung-2.0.txt`) widersprechen diesem Stand und wurden **bewusst nicht**
in dieses Repo übernommen.

## Nicht enthalten (bewusst)

Aus der ursprünglichen Materialsammlung (liegt lokal außerhalb dieses Repos)
wurden nicht übernommen: Produktfotos, Datenblatt-PDF, Fritzing-/Visio-Dateien,
3D-Druckvorlagen, HTML-Diagramm-Exporte, das ESP32-Fachbuch (E-Book) sowie ein
früher Code-Prototyp (`wt32-eth01-dht11`, PlatformIO). Der Prototyp deckt das
Lastenheft/Pflichtenheft noch nicht ab (kein OLED, kein NTP, kein
`config.xml`/LittleFS, kein zweiter Sensor, kein WLAN-Fallback, SNMP v2c statt
v1) und dient aktuell nur als Referenz, nicht als Basis.
