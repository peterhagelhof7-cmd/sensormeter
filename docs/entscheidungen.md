# Entscheidungsprotokoll

Kurzes Log für Design-/Scope-Entscheidungen inkl. Begründung, damit sie
nachvollziehbar bleiben.

## 2026-07-07 — Verdrahtungsstand v1.1 (korrigiert) ist verbindlich

Ältere Verkabelungsnotizen (`RJ45 GPIO.txt`, `RJ45 i2c.txt`, `RJ45 Dht.txt`,
`RJ45 buchse.txt`, `verdrathung-2.0.txt`) legten RJ45-Pin 5 auf IO4 (Kollision
mit dem internen DHT11-Datenpin) und das Display auf IO21/IO22 (Kollision mit
dem Ethernet-PHY). `verdrahtungsschema-v1.1.pdf` behebt beides (Display →
IO32/IO33, RJ45 Pin 5 → IO15). Ab sofort einzige gültige Referenz; ältere
Notizen wurden bewusst nicht ins Repo übernommen.

## 2026-07-08 — SNMP v1 (read-only), nicht v2c

Das Lastenheft fordert explizit SNMP v1 read-only mit fester OID-Struktur
(`.1.3.6.1.4.1.99999.x`). Der vorhandene Code-Prototyp nutzt SNMPv2c mit
anderen OIDs (`.1.3.6.1.4.1.55555.x`). Bei der Neuimplementierung (Phase P6)
wird das nicht übernommen, sondern durch SNMP v1 gemäß Spezifikation ersetzt.

## 2026-07-08 — Zabbix-Integration bleibt im Scope (zusätzlich, optional)

Weder Lastenheft noch Pflichtenheft erwähnen Zabbix; gefordert ist nur SNMP v1
+ Syslog. Der Prototyp hatte aber bereits ein fertiges Zabbix-Template
(`zabbix-template-wt32-eth01-dht11.yaml`) inkl. Doku. Entscheidung: Zabbix
wird als zusätzliches, optionales Monitoring-Feature weitergeführt (Template
wird in Phase P6 an die neue SNMP-v1-OID-Struktur angepasst), ist aber kein
Kernbestandteil des Lastenhefts.

## 2026-07-08 — OTA-Versionscheck via GitHub Releases; Repo öffentlich

Pflichtenheft Abschnitt 11 sieht OTA als spätere optionale Erweiterung vor;
die von P0 genutzte Standard-Partitionstabelle hat bereits zwei OTA-fähige
App-Partitionen (`ota_0`/`ota_1`), keine Anpassung am Flash-Layout nötig.

Umsetzung (Details folgen in Phase P5, Einstellungsseite): Gerät fragt
periodisch `api.github.com/repos/.../releases/latest` ab und zeigt einen
Hinweis an, wenn eine neuere Version verfügbar ist. Zusätzlich gibt es dort
einen Update-Button mit zwei Wegen, beide über `Update.h` (ESP32-OTA in die
freie `ota_1`/`ota_0`-Partition, danach Reboot):
- **Direkt aus dem GitHub-Release**: lädt das `.bin`-Asset der aktuellsten
  Release herunter und flasht es.
- **Lokaler Upload**: `.bin`-Datei manuell über ein Formular hochladen und
  flashen (funktioniert auch ohne Internetzugang, z. B. bei
  Eigenbau-Zwischenständen, die noch kein Release sind).

Die GitHub-API verlangt für private Repos einen
Zugangstoken, der sonst im Geräte-Flash läge und bei physischem Zugriff
auslesbar wäre. Da echte Zugangsdaten (WLAN-PSK, Syslog-IP) ohnehin nie im
Quellcode landen, sondern zur Laufzeit in `config.xml` auf LittleFS gespeichert
werden (siehe `.gitignore`-Kommentar), spricht nichts dagegen, das Repo
öffentlich zu machen — das Repo wurde daher auf **öffentlich** gestellt, ein
Token ist damit nicht nötig.

## 2026-07-08 — Auslegung "Fallback-WLAN installer" (P1)

Das Lastenheft beschreibt den Fallback nur knapp ("nach 5 Minuten nach der
SSID installer mit dem PSK installer suchen"). Auslegung für die Umsetzung:
LAN (DHCP oder statisch) und ein konfiguriertes WLAN laufen parallel als
gleichwertige Interfaces. Nur wenn **keines** von beiden innerhalb von
5 Minuten eine IP bekommt, wechselt der WLAN-Client (STA) auf das
Recovery-Netz `installer`/`installer` — d. h. das Gerät joint ein vom
Nutzer bereitgestelltes Netz mit diesem Namen (z. B. Hotspot vom Handy), um
per DHCP wieder erreichbar zu sein, nicht umgekehrt ein eigener Access
Point. Sobald irgendein Interface wieder eine IP hat (auch das
Recovery-Netz), gilt der Netzwerk-Zustand als „OK".

## 2026-07-08 — Kooperatives Single-Loop-Scheduling statt separater FreeRTOS-Tasks

Pflichtenheft Abschnitt 2.1 listet sechs FreeRTOS-Tasks (Sensor/Network/
NTP/WebServer/Display/Syslog). P0/P1 setzen das stattdessen als
kooperative Module um: ein gemeinsamer `loop()` ruft `networkManager.loop()`
und `timeManager.loop()` im 50-ms-Takt auf. Bei den hier relevanten
Zeitskalen (Sekunden bis Stunden) ist das ausreichend und deutlich
einfacher zu synchronisieren (ein DataManager-Mutex statt sechs
nebenläufiger Tasks). Falls Display/Webserver/Sensorik in späteren Phasen
blockierende Wartezeiten mit sich bringen, die den gemeinsamen Loop spürbar
verzögern, wird das an der Stelle auf echte FreeRTOS-Tasks umgestellt.

## 2026-07-08 — Repo-Kuration

In diesem Repo wird nur die Kern-Dokumentation versioniert (Lastenheft,
Pflichtenheft, aktuelles Verdrahtungsschema, Flash-Anleitung,
Pinout-Referenz, Implementierungsplan, Stückliste). Produktfotos,
Datenblatt-PDF, Fritzing-/Visio-Dateien, 3D-Druckvorlagen,
HTML-Diagramm-Exporte und das ESP32-Fachbuch bleiben lokal außerhalb des
Repos.
