# Stückliste (BOM)

## Pro Gerät

| Bauteil | Menge | Hinweis |
|---|---|---|
| WT32-ETH01 v1.4 (ESP32 + LAN8720) | 1 | Hauptmodul |
| DHT11, 3-Draht-Modul | 1 | intern, Sensor 1, Data → IO4 |
| DHT22, 3-Draht-Modul | 0–1 | extern, Sensor 2 (nur Sensormeter PRO), über RJ45 |
| OLED SSD1306, 0,96", 128×64, I2C | 1 | SCL → IO32, SDA → IO33 |
| RJ45-Buchse, 8P8C, geschirmt | 1 | Modularanschluss für Sensor 2 / künftige Erweiterungen |
| Pull-up-Widerstand 10 kΩ | 1 | intern DHT11-Data (IO4) → 3.3V |
| Pull-up-Widerstand 4,7 kΩ | 2–4 | extern DHT22-Data (RJ45-Modulseite); bei I2C-Modul zusätzlich SDA + SCL |
| Pull-down-Widerstand 10 kΩ | 1 | IO12 (RJ45 Pin 8) beim Boot LOW halten (Boot-Strapping-Pin) |
| Gehäuse / Grundplatte | 1 | 3D-gedruckt, siehe `grundplatte-v1-druckvorlage.png` (lokal, nicht im Repo) |
| Netzteil 5V, ≥ 500 mA | 1 | über USB-Buchse (Typ B/C) |

## Werkzeug (einmalig, nicht pro Gerät)

| Werkzeug | Hinweis |
|---|---|
| USB-Seriell-Adapter (CH340/CP2102, 3,3V-Pegel) | Zum Flashen, siehe `flash-vorbereitung.pdf` |
| 4–6 Dupont-Kabel (female–female) | Verbindung Adapter ↔ Debug-Burning-Schnittstelle |

## Mögliche künftige RJ45-Module (nicht Teil des aktuellen Scopes)

Laut ursprünglichen Notizen als Erweiterungsideen genannt, aber weder im
Lastenheft noch Pflichtenheft gefordert: Türkontakt, Relais-Modul, CO₂-Sensor.
