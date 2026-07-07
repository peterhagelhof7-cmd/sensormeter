# Entscheidungsprotokoll

Kurzes Log für Design-/Scope-Entscheidungen inkl. Begründung, damit sie
nachvollziehbar bleiben.

## 2026-07-08 — Zabbix-Template auf neue OIDs aktualisiert

Der Prototyp-Template (`zabbix-template-wt32-eth01-dht11.yaml`, lokal, nicht
im Repo) nutzte die alten SNMPv2c-OIDs (`.1.3.6.1.4.1.55555.x`) und deckte
nur einen einzelnen DHT11 ab. Neues Template
(`docs/zabbix-template-sensormeter.yaml`) auf die in P6 implementierte
OID-Struktur (`.1.3.6.1.4.1.99999.x`) umgestellt und um Sensor 2, Netzwerk-
Felder (LAN/WLAN-IP, RSSI) und einen "freier Heap niedrig"-Trigger erweitert.
`status.uptime` ist als Zabbix-`TEXT`-Item angelegt (nicht `UNSIGNED`), da
der Wert ein echtes SNMP-TimeTicks ist (Zentisekunden, `addTimestampHandler`)
und Zabbix das als formatierte Zeitspanne anzeigt, nicht als Rohsekunden.

## 2026-07-08 — SNMP-Designentscheidungen (P6)

- **SNMP Community fehlte als Konfigurationsfeld**: `neu 2.txt` listet
  "SNMP Community" explizit als config.xml-Feld, das war in `ConfigManager`
  bisher nicht abgebildet. Ergänzt: `snmpCommunity` (Default `"public"`),
  persistiert als `<snmp community=".../>`, einstellbar auf der
  Einstellungsseite.
- **"Read-only" ist durch Konstruktion erzwungen, nicht nur Konvention**:
  Die Bibliothek (`0neblock/SNMP_Agent`, schon im Prototyp bewaehrt)
  unterstuetzt grundsaetzlich auch SET-Requests. Da im Code nirgends
  `isSettable=true` gesetzt wird, lehnt der Agent jeden SET strukturell ab -
  unabhaengig von der Community. Erfuellt "SNMP v1 READ ONLY" robuster als
  ein reiner Community-Vergleich.
- **Version wird nicht erzwungen, sondern spiegelt die Anfrage**: Die
  Bibliothek antwortet automatisch in der Version des eingehenden Requests
  (v1 oder v2c) statt eine feste Version zu erzwingen. Das erfuellt "SNMP
  v1" fuer v1-Clients, ohne v2c-Clients (z. B. die meisten Monitoring-Tools)
  auszuschliessen.
- **Werte werden alle 5s aktualisiert, nicht pro GET neu berechnet**
  (Pflichtenheft 7: "polling optimized, no continuous refresh") - die
  Bibliothek liest bei jedem GET live von einem Zeiger, der periodisch in
  `SNMPManager::loop()` aufgefrischt wird.
- **Uptime als SNMP-TimeTicks** (Hundertstelsekunden) statt Sekunden - das
  ist die SNMP-uebliche Konvention (`sysUpTime`), damit Standard-Tools wie
  Zabbix den Wert korrekt interpretieren.

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
- **WLAN-Scan ist bewusst blockierend**, obwohl der Webserver sonst
  asynchron ist (Pflichtenheft 8: "non-blocking"). Das ist eine
  admin-ausgeloeste Einzelaktion, keine Dauerbetrieb-Anfrage - ein
  kurzzeitiges Blockieren waehrend eines WLAN-Scans ist unkritisch.
- **Chart.js wird per CDN geladen**, nicht in den Flash eingebettet - betrifft
  nur den Browser des Admins, nicht das Geraet selbst, und faellt damit
  nicht unter das "keine Cloud"-Prinzip (das sich auf die Datenverarbeitung
  des Geraets bezieht).

## 2026-07-08 — OTA-Versionscheck/GitHub-Direktinstall wieder entfernt (P5-Korrektur)

Direkt nach dem ersten P5-Build stellte sich heraus, dass der HTTPS-Client
(`WiFiClientSecure`/`HTTPClient`, fuer den GitHub-Versionscheck und den
Direktinstall aus einem Release) allein rund 168 KB Flash kostet
(89,1 % → 76,2 % nach Entfernen). Bei nur noch ~140 KB Reserve fuer P6
(SNMP v1) und P7 (Syslog) war das Risiko zu hoch, dass eine der beiden
letzten Phasen nicht mehr passt. Entscheidung: GitHub-Versionscheck und
Direktinstall komplett entfernt (`OtaManager` hat jetzt nur noch die
lokalen `Update.h`-Funktionen fuer den `.bin`-Upload). Update-Weg ist damit
ausschliesslich der Upload-Button auf der Einstellungsseite - das war ohnehin
schon vorgesehen (siehe OTA-Eintrag weiter unten), nur die
GitHub-Variante entfaellt. Der damit gegenstandslose "Repo oeffentlich
machen"-Grund (kein Auth-Token fuer die GitHub-API noetig) bleibt als
Randnotiz bestehen, das Repo muss deswegen nicht wieder privat werden.

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
