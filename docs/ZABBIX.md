# Zabbix-Integration

Das Board beantwortet SNMP-v1/v2c-GET-Anfragen direkt (siehe
[docs/entscheidungen.md](entscheidungen.md)) – Zabbix kann es also wie jedes
andere SNMP-Gerät abfragen, ganz ohne zusätzliche Software auf dem Board oder
einem Gateway. SNMP ist read-only: Zabbix kann nichts am Gerät verändern.

## OIDs (Lastenheft Abschnitt 7, Basis `.1.3.6.1.4.1.99999`)

| OID | Bedeutung | Typ |
|---|---|---|
| `.1.99999.1.1.0` | Systemname | String |
| `.1.99999.1.2.0` | Firmwareversion | String |
| `.1.99999.1.3.0` | Systemtyp ("Sensormeter" / "Sensormeter PRO") | String |
| `.1.99999.2.1.0` | LAN-IP | String |
| `.1.99999.2.2.0` | WLAN-IP | String |
| `.1.99999.2.3.0` | WLAN-Signalstärke | Integer, dBm |
| `.1.99999.3.1.0` | Sensor 1 Name | String |
| `.1.99999.3.2.0` | Sensor 1 Temperatur | Integer, x10 (235 = 23.5 °C) |
| `.1.99999.3.3.0` | Sensor 1 Luftfeuchte | Integer, x10 |
| `.1.99999.4.1.0` | Sensor 2 Name (nur PRO) | String |
| `.1.99999.4.2.0` | Sensor 2 Temperatur (nur PRO) | Integer, x10 |
| `.1.99999.4.3.0` | Sensor 2 Luftfeuchte (nur PRO) | Integer, x10 |
| `.1.99999.5.1.0` | Uptime | TimeTicks (Zentisekunden) |
| `.1.99999.5.2.0` | Freier Heap | Gauge32, Bytes |

(OIDs oben mit `.1.3.6.1.4.1` abgekürzt.) Community-String ist auf der
Einstellungsseite des Geräts konfigurierbar (Default `public`).

## Template importieren

1. In Zabbix: **Data collection → Templates → Import**
2. Datei [`zabbix-template-sensormeter.yaml`](zabbix-template-sensormeter.yaml) auswählen
3. Import bestätigen

## Host anlegen

1. **Data collection → Hosts → Create host**
2. Name vergeben (z. B. "Sensor Wohnzimmer")
3. Template **"Sensormeter (WT32-ETH01)"** zuweisen
4. Interface hinzufügen: Typ **SNMP**, IP-Adresse des Boards, Port `161`,
   SNMP-Version **SNMPv2** (das Gerät antwortet unabhängig davon auch v1-Clients
   korrekt, siehe `docs/entscheidungen.md`), Community `public` (oder dein
   eigener Wert)
5. Falls du die Community auf der Einstellungsseite des Geräts geändert hast:
   Host-Makro `{$SNMP_COMMUNITY}` im Host auf denselben Wert setzen

## Mehrere Boards

Jedes Board bekommt in Zabbix einen eigenen Host mit dem gleichen Template,
nur mit unterschiedlicher IP-Adresse im SNMP-Interface. Der Systemname
(Einstellungsseite) hilft dir, die Boards in Zabbix wiederzuerkennen (taucht
auch als eigener Item-Wert "System: Name" auf).

## Mitgelieferte Trigger

Schwellwerte über Host-Makros anpassbar (`{$TEMP_MAX_C}`,
`{$HUMIDITY_MAX_PERCENT}`, `{$HEAP_MIN_BYTES}`):

- Temperatur (Sensor 1 / Sensor 2) zu hoch
- Luftfeuchtigkeit (Sensor 1 / Sensor 2) zu hoch
- Freier Heap niedrig
- Keine Daten seit 10 Minuten (Board offline oder Sensor defekt)

Die Sensor-2-Trigger sind nur relevant, wenn Sensor 2 (Sensormeter PRO) auf
der Einstellungsseite aktiviert ist – sonst liefert das Gerät dafür
durchgehend `0`, die Trigger lösen dann nicht aus.

## Testen ohne Zabbix

Mit Net-SNMP-Tools (`apt install snmp` unter Linux):

```
snmpget -v1 -c public <board-ip> .1.3.6.1.4.1.99999.3.2.0
snmpget -v2c -c public <board-ip> .1.3.6.1.4.1.99999.3.3.0
```
