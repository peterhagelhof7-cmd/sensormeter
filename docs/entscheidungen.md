# Entscheidungsprotokoll

Kurzes Log für Design-/Scope-Entscheidungen inkl. Begründung, damit sie
nachvollziehbar bleiben.

## 2026-07-08 — Webserver-Designentscheidungen (P5)

Mehrere kleinere, aber wichtige Entscheidungen beim Umsetzen der Webserver-Phase:

- **Systemtyp nicht mehr manuell einstellbar**: Lastenheft 5.1 sagt "Systemtyp
  wird definiert, sobald 2. Sensor aktiv ist" - das ist ein abgeleiteter
  Wert, kein Formularfeld (Abschnitt 6, die Einstellungsseite, listet
  "Systemtyp" auch konsequent nicht auf). `ConfigManager` berechnet
  `systemType` jetzt immer aus `sensor2Enabled` (`"Sensormeter"` /
  `"Sensormeter PRO"`), unabhaengig davon was im XML-Import steht.
- **HTTP-Auth mit festem Benutzernamen "admin"**: Das Lastenheft definiert nur
  ein Passwort fuer die Einstellungsseite, keinen Benutzernamen. Die
  Browser-Login-Abfrage verlangt technisch trotzdem einen (beliebigen)
  Benutzernamen - fest auf "admin" gesetzt und als Hinweistext im
  Auth-Dialog (Realm) angezeigt.
- **TLS-Zertifikatspruefung deaktiviert fuer OTA/GitHub** (`setInsecure()`):
  Root-CA-Pins muessten bei Zertifikatsrotation gepflegt werden, was fuer
  ein selten aktualisiertes Hobby-Geraet leicht bricht. Ausgleich: alle
  OTA-Endpunkte sind hinter demselben Passwort wie die Einstellungsseite -
  ein Angreifer muesste den Admin-Zugang bereits haben, um ueberhaupt ein
  Update auszuloesen.
- **WLAN-Scan und OTA-Flash sind bewusst blockierend**, obwohl der
  Webserver sonst asynchron ist (Pflichtenheft 8: "non-blocking"). Beides
  sind admin-ausgeloeste Einzelaktionen, keine Dauerbetrieb-Anfragen - ein
  kurzzeitiges Blockieren waehrend eines WLAN-Scans oder eines Firmware-
  Downloads ist unkritisch, das Geraet macht ohnehin nichts anderes
  Sinnvolles waehrend eines Reboots.
- **Chart.js wird per CDN geladen**, nicht in den Flash eingebettet - betrifft
  nur den Browser des Admins, nicht das Geraet selbst, und faellt damit
  nicht unter das "keine Cloud"-Prinzip (das sich auf die Datenverarbeitung
  des Geraets bezieht).
- **Flash-Budget wird eng**: Nach P5 (ESPAsyncWebServer + AsyncTCP +
  ArduinoJson + WiFiClientSecure/HTTPClient fuer OTA) liegt der
  Flash-Verbrauch bei 89,1 % (1.167.525 / 1.310.720 Byte), RAM bei 15,7 %.
  Für P6 (SNMP v1) und P7 (Syslog) bleiben nur noch ca. 140 KB. SNMP v1 ist
  ein kleiner Read-Only-Agent, sollte passen - falls nicht, muesste ggf. eine
  OTA-Komponente (z. B. der direkte GitHub-Fetch, der lokale Upload allein
  wuerde reichen) wieder entfernt werden.

## 2026-07-08 — Display-Auslegung: Boot-Countdown-Tempo & automatische Schriftgröße (P4)

Zwei Stellen im Lastenheft sind für die Umsetzung zu knapp spezifiziert:

- **Boot-Countdown "100 → 0 bis das Netzwerk bereit ist"**: keine Zeitbasis
  angegeben. Auslegung: 1 Tick/Sekunde. Bei den bis zu 5 Minuten, die
  `NETWORK_CHECK` laut P1 dauern darf, erreicht der Countdown 0 oft vor
  Netzwerkbereitschaft und bleibt dann auf 0 stehen ("wir warten noch") statt
  eine falsche Restzeit zu suggerieren.
- **"größtmögliche Schrift sodass IP ohne Scrollen passt"**: gilt wörtlich
  nur für die IP-Seite. Auslegung: pro Seite wird anhand der längsten Zeile
  automatisch Schriftgröße 2 oder 1 gewählt (`DisplayManager::drawLines`),
  damit das Prinzip einheitlich für alle Seiten gilt, nicht nur für IPs.

## 2026-07-07 — Verdrahtungsstand v1.2 (korrigiert) ist verbindlich

Frühe Verkabelungsnotizen (`RJ45 GPIO.txt`, `RJ45 i2c.txt`, `RJ45 Dht.txt`,
`RJ45 buchse.txt`, `verdrathung-2.0.txt`) legten RJ45-Pin 5 auf IO4 (Kollision
mit dem internen DHT11-Datenpin) und das Display auf IO21/IO22 (Kollision mit
dem Ethernet-PHY). `verdrahtungsschema-v1.2.pdf` ist die bereinigte, in sich
konsistente Fassung (siehe Änderungshistorie im Dokument für v1.0→v1.1→v1.2)
und die einzige gültige Referenz; ältere Notizen wurden bewusst nicht ins
Repo übernommen.

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

## 2026-07-08 — tinyxml2 vendort statt per lib_deps (P2)

Für das in `config.xml` geforderte XML-Parsing/-Schreiben wird tinyxml2
genutzt. Zwei Standardwege sind gescheitert bzw. ungünstig: Der
PlatformIO-Registry-Fork `sepastian/tinyxml2` lässt sich unter Windows nicht
installieren (enthält einen kaputten Symlink auf einen absoluten Pfad). Ein
direkter Git-Checkout des Original-Repos baut zwar, bringt aber `contrib/`
und die Testsuite `xmltest.cpp` mit und bläht den Flash-Verbrauch um ca.
220 KB auf (63,7 % → 82,7 %) – nicht tragbar, da P3–P8 noch deutlich mehr
Bibliotheken (DHT, SSD1306, AsyncWebServer, SNMP) brauchen. Lösung: nur
`tinyxml2.cpp`/`tinyxml2.h` (zzgl. Lizenzhinweis) manuell nach
`firmware/lib/tinyxml2/` kopiert. Effekt: nur noch ca. +21 KB Flash für die
tatsächlich genutzte XML-Funktionalität (siehe `lib/tinyxml2/README.md` für
Herkunft/Update-Hinweis).

## 2026-07-08 — Repo-Kuration

In diesem Repo wird nur die Kern-Dokumentation versioniert (Lastenheft,
Pflichtenheft, aktuelles Verdrahtungsschema, Flash-Anleitung,
Pinout-Referenz, Implementierungsplan, Stückliste). Produktfotos,
Datenblatt-PDF, Fritzing-/Visio-Dateien, 3D-Druckvorlagen,
HTML-Diagramm-Exporte und das ESP32-Fachbuch bleiben lokal außerhalb des
Repos.
