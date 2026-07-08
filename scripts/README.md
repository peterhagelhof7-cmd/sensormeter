# scripts/

## flash-sensormeter.ps1 / flash-sensormeter.cmd

Richtet auf einem beliebigen Windows-PC alles ein, was für einen
Flash-Vorgang nötig ist, und flasht anschließend:

1. Python installieren (falls nicht vorhanden, über winget)
2. Git installieren (falls nicht vorhanden, über winget)
3. PlatformIO installieren (falls nicht vorhanden, über pip)
4. Repo klonen (falls noch nicht vorhanden) bzw. aktualisieren
   (`git pull`, nur wenn keine lokalen Änderungen vorliegen)
5. `firmware/include/config.h` aus der Vorlage anlegen (nur falls sie fehlt –
   eine vorhandene wird nie überschrieben)
6. Firmware bauen (`pio run`)
7. Firmware flashen (`pio run --target upload`)

**Voraussetzung:** Board am USB-Seriell-Adapter (Debug-Burning-Schnittstelle,
nicht am 20-Pin-Hauptheader!) angeschlossen, siehe
[`../docs/flash-vorbereitung.pdf`](../docs/flash-vorbereitung.pdf).

### Nutzung

Nur diese eine Datei `flash-sensormeter.ps1` (oder zusätzlich die `.cmd` zum
Doppelklicken) auf den Ziel-PC kopieren und ausführen – das Repo muss dafür
noch **nicht** vorher geklont sein, das Skript erledigt das selbst.

**Per Doppelklick:** `flash-sensormeter.cmd` – öffnet ein Konsolenfenster,
das nach Abschluss offen bleibt (zum Lesen von Meldungen/Fehlern).

**Per PowerShell:**

```powershell
.\flash-sensormeter.ps1                      # Standardablauf
.\flash-sensormeter.ps1 -Port COM5           # fester COM-Port
.\flash-sensormeter.ps1 -SkipUpload          # nur bauen, nicht flashen
.\flash-sensormeter.ps1 -RepoPath C:\Projekte\sensormeter
```

Falls PowerShell die Ausführung wegen der Execution Policy verweigert:

```powershell
powershell -ExecutionPolicy Bypass -File .\flash-sensormeter.ps1
```

## ../firmware/tools/simulate_json_load.cpp

Siehe [`../firmware/tools/README.md`](../firmware/tools/README.md) –
natives Testprogramm zur Heap-Last-Simulation der REST-API, kein
Setup-/Flash-Werkzeug.
