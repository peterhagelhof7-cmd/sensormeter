# PRTG-Integration

Das Board beantwortet SNMP-v1/v2c-GET-Anfragen direkt (siehe
[docs/entscheidungen.md](entscheidungen.md)) – PRTG kann es daher wie jedes
andere SNMP-Gerät abfragen, ganz ohne zusätzliche Software auf dem Board
oder einem Gateway. SNMP ist read-only: PRTG kann nichts am Gerät verändern.

Dieses Dokument ergänzt das bereits vorhandene [Zabbix-Template](ZABBIX.md)
um ein PRTG-**Geräte-Template** (`.odt`-Datei) für die Auto-Discovery-
Funktion von PRTG – das ist das PRTG-Äquivalent zum Zabbix-Template, auch
wenn das Dateiformat und der Import-Weg anders funktionieren.

## OIDs (Lastenheft Abschnitt 7, Basis `.1.3.6.1.4.1.99999`)

Identische OID-Tabelle wie im Zabbix-Template, siehe [ZABBIX.md](ZABBIX.md#oids-lastenheft-abschnitt-7-basis-1361419999).
Temperatur/Luftfeuchte kommen als Ganzzahl ×10 vom Gerät – das
PRTG-Template rechnet das über die eingebaute Skalierung der
"SNMP Custom"-Sensoren (Divisor 10) automatisch zurück in Dezimalwerte,
kein manueller Nachbearbeitungsschritt nötig.

## Template importieren

PRTG-Geräte-Templates liegen als `.odt`-Datei im Ordner
`devicetemplates` der PRTG-Installation (Standard:
`C:\Program Files (x86)\PRTG Network Monitor\devicetemplates\`) – anders
als beim Zabbix-Import gibt es keinen "Datei hochladen"-Dialog in der
PRTG-Weboberfläche.

1. Datei [`prtg-template-sensormeter.odt`](prtg-template-sensormeter.odt)
   in den `devicetemplates`-Ordner des PRTG-Servers kopieren (per RDP/
   Dateifreigabe auf den Server, auf dem PRTG läuft)
2. In der PRTG-Weboberfläche: **Setup → System Administration → Core &
   Probes** öffnen, keine Aktion nötig – neue Templates werden von PRTG
   automatisch beim nächsten Auto-Discovery-Lauf erkannt (kein Neustart
   des PRTG-Core-Dienstes nötig, kann aber ein bis zwei Minuten dauern)

## Gerät anlegen

1. Im gewünschten Probe/Ordner/Gruppe: **Add Device**
2. Name vergeben (z. B. "Sensor Wohnzimmer"), IPv4/DNS-Name des Boards
   eintragen
3. Bei **Device Template** den Punkt **"Automatic Device Identification
   (recommended)"** abwählen und stattdessen manuell
   **"Sensormeter (WT32-ETH01)"** auswählen (nur mit dieser expliziten
   Auswahl wird garantiert genau dieses Template verwendet statt einer
   generischen Auto-Erkennung)
4. Unter **SNMP Credentials** die Community eintragen (Default `public`,
   siehe Einstellungsseite des Geräts) – SNMP-Version **v2c** wählen
   (das Gerät antwortet unabhängig davon auch v1-Clients korrekt, siehe
   `docs/entscheidungen.md`), Port `161`
5. **Continue** → PRTG führt die Auto-Discovery aus und legt alle 15
   Sensoren aus dem Template an

## Nicht zutreffende Sensoren entfernen

Das Template ist bewusst identisch für alle drei SNMP-Agent-Projekte der
Familie (Sensormeter, Sensormeter WLAN, Sensormeter PoE) – die Firmware
liefert für nicht vorhandene Zweige einfach keine Antwort statt eines
falschen Werts (siehe `docs/entscheidungen.md`), betroffene Sensoren
gehen dann auf "Down"/"Unusual" statt einen falschen Wert zu zeigen.
Nach dem Anlegen ggf. manuell löschen:

- **"Sensor 2: ..."** (3 Sensoren) – nur relevant, wenn Sensor 2
  (Sensormeter PRO) auf der Einstellungsseite aktiviert ist

## Mitgelieferte Sensoren

| Sensor | PRTG-Sensortyp | Skalierung |
|---|---|---|
| Ping | Ping | – |
| System: Name / Firmwareversion / Systemtyp | SNMP Custom String | – |
| Netzwerk: LAN-IP / WLAN-IP | SNMP Custom String | – |
| Netzwerk: WLAN-Signalstärke | SNMP Custom (dBm) | ÷1 |
| Sensor 1/2: Name | SNMP Custom String | – |
| Sensor 1/2: Temperatur | SNMP Custom (°C) | ÷10 |
| Sensor 1/2: Luftfeuchtigkeit | SNMP Custom (%) | ÷10 |
| Status: Uptime | SNMP Custom (Sekunden) | ÷100 (TimeTicks sind Zentisekunden) |
| Status: Freier Heap | SNMP Custom (Bytes) | ÷1 |

## Warnschwellwerte

Das Template legt bewusst **keine** Limits/Trigger vorkonfiguriert an
(anders als das Zabbix-Template mit seinen Host-Makros) – PRTGs
Geräte-Templates unterstützen keine nachträglich pro Host anpassbaren
Platzhalter-Schwellwerte wie Zabbix' `{$MACRO}`-Syntax. Schwellwerte
stattdessen nach dem Import direkt am jeweiligen Sensor unter
**Channels → Limits** setzen (empfohlen: Sensor-Temperatur
Ober-/Untergrenze, Luftfeuchtigkeit-Obergrenze, Freier Heap
Untergrenze ~20000 Bytes – siehe die Zabbix-Trigger-Vorschläge in
[ZABBIX.md](ZABBIX.md) als Orientierung).

## Mehrere Boards

Jedes Board bekommt in PRTG ein eigenes Gerät mit demselben Template,
nur mit unterschiedlicher IP-Adresse. Der Systemname (Einstellungsseite)
hilft, die Boards wiederzuerkennen (taucht auch als eigener Sensorwert
"System: Name" auf).

## Testen ohne PRTG

Mit Net-SNMP-Tools (`apt install snmp` unter Linux):

```
snmpget -v1 -c public <board-ip> .1.3.6.1.4.1.99999.3.2.0
snmpget -v2c -c public <board-ip> .1.3.6.1.4.1.99999.3.3.0
```

## Technischer Hintergrund zum Template

Das Geräte-Template-Dateiformat (`.odt`) ist von Paessler nicht offiziell
dokumentiert ("this is internal information currently... may change with
every new version" laut Paessler-Support). Dieses Template wurde daher
nicht aus der Dokumentation abgeleitet, sondern anhand eines echten,
veröffentlichten PRTG-Templates für einen vergleichbaren ESP32-basierten
SNMP-Umweltsensor nachgebaut und verifiziert (wohlgeformtes XML, korrekte
`kind`-Werte `snmpcustom`/`snmpcustomstring`/`ping`, funktionierende
Skalierung über `factord`). Vor dem produktiven Einsatz trotzdem einmal
testweise importieren und die Sensorwerte gegen `snmpget` (siehe oben)
gegenprüfen.
