# Strombedarf & Stromversorgung

Strombudget für ein Sensormeter-Gerät, damit ein passendes Netzteil gewählt
werden kann. Unterschieden wird zwischen **Durchschnitt** (bestimmt die
Wärmeentwicklung/Dauerlast) und **Spitze** (bestimmt, wie kräftig das
Netzteil kurzzeitig sein muss, damit die Spannung nicht einbricht).

## Strombudget pro Komponente (bei 3,3–5V)

| Komponente | Ø-Strom | Spitzenstrom | Quelle |
|---|---|---|---|
| WT32-ETH01 (ESP32 + LAN8720) | 80 mA | ≥ 500 mA | Herstellerdatenblatt (`WT32-ETH01_datasheet_V1.1`), Tabelle 1: "Operating current: Average 80mA" / "Power supply current: Min 500mA" |
| DHT11 (intern) | ~0,3 mA | ~2,5 mA | Standard-Datenblattwerte; durch 60s-Abfragetakt (Lastenheft) liegt der DHT die meiste Zeit im Standby (<0,1 mA), Spitze nur während der ~20ms-Messung |
| DHT22 (extern, nur PRO) | ~0,5 mA | ~2,5 mA | Standard-Datenblattwerte, gleiche Logik wie DHT11 |
| OLED SSD1306, 0,96", 128×64 | 12–24 mA | ~24 mA | Herstellerangabe aus der Materialsammlung: "0,04W normal / 0,08W Vollbild" bei 3,3V → 12–24 mA |
| Pull-up/-down-Widerstände (10k/4,7k) | < 1 mA | < 1 mA | vernachlässigbar |

## Gesamtbedarf pro Gerät

| Variante | Ø-Strom (Dauerlast) | Spitzenstrom |
|---|---|---|
| **Sensormeter** (1 Sensor) | ~95–105 mA | ~530 mA |
| **Sensormeter PRO** (2 Sensoren) | ~96–106 mA | ~535 mA |

Der Spitzenwert wird praktisch komplett vom WT32-ETH01 selbst bestimmt
(WLAN-Sendebursts, Flash-Schreibzugriffe beim Speichern von `config.xml`
oder OTA-Updates) – DHT11/DHT22/OLED tragen nur wenige zusätzliche mA bei.

## Empfohlene Stromversorgung

**5V-USB-Netzteil, mindestens 1A (1000 mA).**

Begründung:
- Das WT32-ETH01-Datenblatt selbst verlangt schon mindestens 500 mA
  Versorgungsfähigkeit für das nackte Modul (siehe `flash-vorbereitung.pdf`,
  die dort dasselbe Minimum nennt).
- 1A statt der bloßen 500-mA-Mindestangabe gibt Reserve gegen
  Spannungsabfall durch dünne/billige USB-Kabel und lässt Luft für künftige
  RJ45-Erweiterungen (z. B. ein Relais-Modul, siehe `stueckliste.md`).
- Ein gutes, ausreichend dickes USB-Kabel ist wichtiger als die
  Netzteil-Nennleistung allein – dünne Kabel erzeugen bei 500 mA+ spürbaren
  Spannungsabfall (siehe auch `flash-vorbereitung.pdf`: "häufigste
  Fehlerursache ist eine fehlende/schwache Masseverbindung").
- Bei Bedarf für mehrere Geräte reicht ein handelsübliches
  5V/2A-USB-Netzteil pro Standort locker; eine gemeinsame Mehrfachsteckdose
  mit mehreren einzelnen 5V/1A-Steckernetzteilen ist ebenso geeignet.

Nicht verwenden: der Ausgang eines USB-Seriell-Adapters (zu schwach,
siehe `flash-vorbereitung.pdf`) oder ein reiner Daten-USB-Port ohne
Ladefunktion (kann auf 500 mA begrenzt sein).
