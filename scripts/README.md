# scripts/

## flash.ps1 / flash.cmd

Richtet auf einem beliebigen Windows-PC alles ein, was für einen
Flash-Vorgang nötig ist, und flasht anschließend eines von vier
Sensormeter-Schwesterprojekten:

1. **Projekt wählen** – interaktiv (1-4-Menü) oder per `-Project
   sensormeter|wlan|display|poe`:
   - **Sensormeter** (WT32-ETH01, Ethernet + bis zu 2 Sensoren)
   - **Sensormeter WLAN** (generisches ESP32-WROOM-32-DevKit, WLAN-only)
   - **Sensormeter Display** (ESP32-Touchdisplay, SNMP-Client)
   - **Sensormeter PoE** (Waveshare ESP32-S3-ETH, PoE optional, Relais)
2. Python installieren (falls nicht vorhanden, über winget)
3. Git installieren (falls nicht vorhanden, über winget)
4. PlatformIO installieren (falls nicht vorhanden, über pip)
5. Das gewählte Repo klonen (falls noch nicht vorhanden) bzw. aktualisieren
   (`git pull`, nur wenn keine lokalen Änderungen vorliegen)
6. `firmware/include/config.h` aus der Vorlage anlegen, falls das gewählte
   Projekt eine braucht und sie noch fehlt (eine vorhandene wird nie
   überschrieben; Sensormeter Display braucht keine)
7. Firmware bauen (`pio run`)
8. Firmware flashen (`pio run --target upload`)

Dieses Skript liegt identisch in allen vier Repos (`scripts/flash.ps1`) –
unabhängig davon, welches Projekt gerade lokal ausgecheckt ist, lässt sich
darüber jedes der vier flashen (die jeweils anderen werden bei Bedarf
automatisch in einen Unterordner neben dem Skript geklont).

**Voraussetzung für Sensormeter:** Board am USB-Seriell-Adapter
(Debug-Burning-Schnittstelle, nicht am 20-Pin-Hauptheader!) angeschlossen,
siehe [`../docs/flash-vorbereitung.pdf`](../docs/flash-vorbereitung.pdf).

### Nutzung

Nur diese eine Datei `flash.ps1` (oder zusätzlich die `.cmd` zum
Doppelklicken) auf den Ziel-PC kopieren und ausführen – das jeweilige Repo
muss dafür noch **nicht** vorher geklont sein, das Skript erledigt das
selbst.

**Per Doppelklick:** `flash.cmd` – öffnet ein Konsolenfenster, fragt nach
dem gewünschten Projekt und bleibt nach Abschluss offen (zum Lesen von
Meldungen/Fehlern).

**Per PowerShell:**

```powershell
.\flash.ps1                                  # fragt interaktiv nach dem Projekt
.\flash.ps1 -Project sensormeter             # Projekt direkt angeben
.\flash.ps1 -Project wlan -Port COM5         # fester COM-Port
.\flash.ps1 -Project display -SkipUpload     # nur bauen, nicht flashen
.\flash.ps1 -Project sensormeter -RepoPath C:\Projekte\sensormeter
```

Falls PowerShell die Ausführung wegen der Execution Policy verweigert:

```powershell
powershell -ExecutionPolicy Bypass -File .\flash.ps1
```

**Geplant, noch nicht umgesetzt:** Mac-Unterstützung, ausdrücklich nur für
Apple-Silicon-Macs (ARM, kein Intel-Mac) - siehe
[`../docs/entscheidungen.md`](../docs/entscheidungen.md) für offene Fragen
zur Umsetzung.

## convert-logo.ps1

Konvertiert ein Anbieter-Logo (beliebiges Bildformat) in das fürs
Anbieter-Branding-Feature kompatible Rohformat – liegt identisch in allen
vier Repos, analog zu `flash.ps1`. Fragt zuerst (interaktiv oder per
`-Display sensormeter|wlan|poe|display|custom`), für welches Display
konvertiert werden soll, und reduziert Auflösung **und Farbtiefe**
konsequent auf das, was das jeweilige Display tatsächlich darstellen kann:

- **Sensormeter / Sensormeter WLAN** (OLED SSD1306, 128×64) und
  **Sensormeter PoE** (OLED SH1107, 128×128): 1-Bit-Monochrom, exakt
  Breite/8 × Höhe Byte, MSB-zuerst je Zeile – identisches Format zu
  `BrandingManager::LOGO_BYTES` (aktuell nur bei Sensormeter WLAN
  implementiert, siehe dessen `docs/entscheidungen.md`; für Sensormeter
  und Sensormeter PoE bereits vorbereitet, falls das Feature dorthin
  portiert wird).
- **Sensormeter Display** (TFT ST7789P3, 240×320): RGB565, 2 Byte/Pixel,
  Little-Endian – **experimentell**, da dieses Projekt noch keine
  Branding-Firmware hat, die das Format konsumiert.

Das Quellbild wird seitenverhältnistreu eingepasst (nicht verzerrt) und
zentriert mit der Padding-Farbe (Default Schwarz) aufgefüllt.

```powershell
.\convert-logo.ps1 -InputPath .\firmenlogo.png                # fragt interaktiv nach dem Display
.\convert-logo.ps1 -InputPath .\firmenlogo.png -Display wlan  # 128x64, 1bpp, direkt hochladbar
.\convert-logo.ps1 -InputPath .\firmenlogo.png -Display custom -Width 96 -Height 48 -ColorMode mono
```

Das Ergebnis (`<Name>-<Display>-<Breite>x<Höhe>.bin`) lässt sich bei
Sensormeter WLAN direkt über die Einstellungsseite
(„Anbieter-Branding → Logo hochladen") hochladen.

## ../firmware/tools/simulate_json_load.cpp

Siehe [`../firmware/tools/README.md`](../firmware/tools/README.md) –
natives Testprogramm zur Heap-Last-Simulation der REST-API, kein
Setup-/Flash-Werkzeug.
