# Entscheidungsprotokoll

Kurzes Log für Design-/Scope-Entscheidungen inkl. Begründung, damit sie
nachvollziehbar bleiben.

## 2026-07-08 — Flash-Setup-Skript: funktionale statt reine PATH-Pruefung

`scripts/flash-sensormeter.ps1` richtet Python/Git/PlatformIO automatisch
ein und flasht danach. Zwei echte Bugs beim Testen gefunden und behoben,
bevor das Skript verteilt wurde:

- **Falsch-positive Erkennung**: Windows legt standardmaessig einen
  `python`-Store-Alias auf PATH, der fuer `Get-Command python` wie eine
  echte Installation aussieht, aber nur ein Verweis auf den Microsoft
  Store ist. Loesung: Erkennung ruft `<tool> --version` tatsaechlich auf
  und prueft die Ausgabe gegen ein Muster (`^Python \d` usw.), statt nur
  PATH-Praesenz zu pruefen.
- **winget-Exitcode ist kein verlaesslicher Erfolgsindikator**: Ist ein
  Paket bereits installiert und aktuell, liefert `winget install` einen
  Nicht-Null-Exitcode ("kein Update verfuegbar"), obwohl das kein Fehler
  ist. Loesung: Erfolg/Misserfolg wird ausschliesslich ueber den
  funktionalen Vorher/Nachher-Test entschieden, nicht ueber den
  winget-Exitcode.

Getestet (auf diesem Rechner, mit `-SkipUpload`): bestehender Checkout mit
lokalen Aenderungen (kein `git pull`, wie gewuenscht) sowie kompletter
Frisch-Klon inkl. Bibliotheks-Download und Build - beide erfolgreich. Der
eigentliche `pio run --target upload`-Schritt ist **ungetestet**, da kein
Board angeschlossen war.

## 2026-07-08 — Systemlast-Analyse deckt Display-Ineffizienz auf, behoben

Beim Aufstellen der Systemlast-Rechnung (`docs/systemlast.md`) fiel auf,
dass `DisplayManager::loop()` den Boot-Countdown-Screen bei jedem
50-ms-Tick neu gezeichnet hat statt nur bei tatsaechlicher
Wertaenderung (1x/s) - bei ~20-25ms pro vollem I2C-Frame waeren das
rechnerisch ~40-50% CPU-Last allein fuers Display waehrend der bis zu
5 Minuten dauernden `NETWORK_CHECK`-Phase gewesen, nah am bzw. ueber dem
Pflichtenheft-Zielwert "CPU load < 40%". Noch vor dem ersten
Hardware-Test behoben: Neuzeichnen nur noch bei tatsaechlicher
Countdown-Aenderung. Details/Rechnung siehe `docs/systemlast.md`.

## 2026-07-08 — ESP32-Chip-Temperatur auf der Hauptseite (nachtraeglich, ausserhalb Lastenheft)

Auf Wunsch ergaenzt: `temperatureRead()` (Arduino-ESP32-Core) liest den
internen Temperatursensor des ESP32-Chips aus, angezeigt im "System"-Block
der Hauptseite und in `/api/status` als `chipTemperatureC`. Wichtiger
Unterschied zu den DHT-Sensorwerten: Das ist die **Chip-/Die-Temperatur**
(durch Eigenerwaermung der Elektronik typischerweise deutlich hoeher als die
Umgebungstemperatur), keine kalibrierte Umgebungsmessung - laut
Core-Kommentar sogar "undocumented". Rein informativ (z. B. um grobe
Selbsterwaermung/thermische Probleme zu erkennen), kein Ersatz fuer die
DHT11/DHT22-Werte.

## 2026-07-08 — Syslog-Designentscheidungen (P7)

- **Fehler-Events laufen ueber dasselbe Ereignisprotokoll wie die
  Webseiten-Tabelle** (`DataManager::pushLogEntry`), nicht ueber einen
  separaten Mechanismus. Neu: ein `severity`-Feld (3 = Error, 6 =
  Informational) und eine fortlaufende `sequence`-Nummer pro Eintrag,
  damit `SyslogManager` per Polling (jeden Loop-Tick) zuverlaessig genau
  die neuen Eintraege erkennt und versendet, ohne ein Observer-/
  Callback-Pattern einzufuehren (die C-Funktionszeiger-Callbacks der
  SNMP-Bibliothek in P6 haben schon gezeigt, dass so etwas hier eher
  Komplexitaet als Nutzen bringt).
- **Statusreport-Trigger folgt dem tatsaechlichen Sensorzyklus**, nicht
  einem eigenen 60s-Timer: `SyslogManager::loop()` beobachtet
  `DataManager::getSensor1().lastReadMillis` und sendet den Report, sobald
  sich dieser Wert aendert. Das erfuellt "bei jedem Sensorzyklus"
  (Pflichtenheft) exakt, ohne dass zwei unabhaengige 60s-Timer
  auseinanderdriften koennten.
- **Nachrichtenformat**: minimal an RFC 5424 angelehnter Rahmen (PRI +
  Version + Platzhalter), der eigentliche Inhalt folgt dem in Lastenheft
  Abschnitt 9 vorgegebenen Pipe-Format. ISO-Zeitstempel ueber
  `strftime("%Y-%m-%dT%H:%M:%S%z")` - liefert `+0200` statt `+02:00` (ISO
  8601 Basic-Format statt Extended-Format), fuer ein internes Monitoring-
  Format ausreichend genau.
- **Syslog-Versand komplett deaktiviert, wenn `syslogServer` auf `0.0.0.0`
  steht** (Config-Default) - kein UDP-Traffic, solange niemand einen Server
  eingetragen hat.

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

## 2026-07-09 — Gefixt: DNS-Server bei statischer LAN-/WLAN-IP fehlte komplett

Beim analogen Fix im Sensormeter-WLAN-Projekt aufgefallen (dort zuerst durch
eine Nutzerfrage entdeckt) und hier verifiziert: `NetworkManager::
applyLanConfig()`/`applyWlanConfig()` riefen bisher nur `ETH.config(ip,
gateway, mask)` bzw. `WiFi.config(ip, gateway, mask)` auf (3-Parameter-Form).
Die tatsächliche Signatur ist `config(ip, gateway, subnet, dns1=0.0.0.0,
dns2=0.0.0.0)`, und sowohl `ETH.cpp::config()` als auch
`WiFiGeneric.cpp::set_esp_interface_dns()` setzen einen DNS-Server nur,
wenn die übergebene Adresse ungleich `0.0.0.0` ist. Ohne explizite Angabe
blieb bei statischer IP auf **beiden** Interfaces also gar kein DNS-Server
konfiguriert - Hostnamensauflösung (z. B. für einen künftigen
NTP-/HTTPS-Hostnamen) wäre fehlgeschlagen. Bei DHCP (Default) betrifft das
nicht, da der DNS-Server automatisch vom Router mitkommt.

**Fix:** Neue Konfigurationsfelder `lanDns`/`wlanDns` (Web-Formular,
`ConfigManager`, XML-Schema `<lan dns="...">`/`<wlan dns="...">`) - bei
leerem/ungültigem Wert wird jeweils das zugehörige Gateway als DNS-Server
verwendet (funktioniert bei den meisten Routern), sonst der eingetragene
Server. Beide Interfaces (LAN und WLAN) sind damit vollständig über die
Weboberfläche konfigurierbar.

## 2026-07-09 — Flash-Skript vereinheitlicht (`flash.ps1` statt `flash-sensormeter.ps1`)

`scripts/flash-sensormeter.ps1` wurde zu `scripts/flash.ps1` verallgemeinert:
das Skript fragt jetzt zuerst (interaktiv oder per `-Project
sensormeter|wlan|display`), welches der drei Sensormeter-Schwesterprojekte
geflasht werden soll, statt fest an ein Repo gebunden zu sein. Danach
identischer Ablauf wie zuvor (Python/Git/PlatformIO-Setup, Klonen/Pull,
config.h, Bauen, Flashen). Das Skript liegt jetzt identisch in allen drei
Repos - egal welches Projekt lokal ausgecheckt ist, lassen sich darüber alle
drei einrichten. Grund: alle drei Skripte waren bis auf Repo-URL,
Ordnernamen und ein paar Hinweistexte ohnehin fast wortgleich; eine einzige
gepflegte Version reduziert Drift zwischen den drei Kopien.

## Versionierung

Bisher war `DEVICE_FIRMWARE_VERSION` an die zuletzt abgeschlossene
Implementierungsphase gekoppelt (`0.1.0-p0` … `0.1.0-p7`) - kein Schema mit
klarer Fortsetzung, sobald alle Phasen umgesetzt sind, und es gab nie einen
Git-Tag oder eine GitHub-Release, obwohl die Weboberfläche bereits einen
Link auf "Releases auf GitHub" zeigt.

**Umstellung auf Semantic Versioning**, analog zu Sensormeter WLAN und
Sensormeter Display (dort zuerst eingeführt): aktueller Stand auf
**`0.9.0-rc1`** (Beta) gesetzt - alle Kernfunktionen aus dem Lastenheft sind
umgesetzt (P0-P7), aber nicht vollständig auf echter Hardware verifiziert
(siehe "Noch offen" weiter oben), daher Release-Candidate-Status statt
`1.0.0`.

Zusätzlich nachgezogen: die Firmware-Version fehlte bislang auf dem OLED
(Webserver und SNMP zeigten sie bereits) - jetzt als dritte Zeile auf der
Status-Seite (`DisplayManager::drawStatusPage()`) ergänzt.

Die Versionsnummer lebt weiterhin als einzige Quelle der Wahrheit in
`firmware/include/config.h(.example)`, zusätzlich in README vermerkt. Der
One-Pager (`docs/sensormeter-onepager.pdf`) hat kein eingechecktes
HTML-Quelldokument mehr im Repo und wurde daher hier nicht mit
aktualisiert - offener Punkt für eine spätere Runde.

**Noch nicht Teil dieser Änderung** (separates Thema, wie bei den beiden
Schwesterprojekten): tatsächliche Git-Tags + GitHub-Releases mit
`.bin`-Artefakt pro Version.

## Kalibrierkorrektur je Sensor + Webdesign an Sensormeter Display angepasst

Nutzerwunsch: fester Korrekturwert (°C/%, positiv oder negativ) für beide
Sensoren, der auch die SNMP-gemeldeten Werte erfasst; zusätzlich das
Webdesign an das inzwischen überarbeitete Sensormeter-Display-Projekt
angleichen.

### Korrektur direkt in SensorManager angewendet - wirkt automatisch auch auf SNMP
`DeviceConfig` bekam vier neue Felder (`sensor1TempOffset`/`sensor1HumOffset`/
`sensor2TempOffset`/`sensor2HumOffset`, persistiert im `<sensors>`-Element
von `config.xml`). Die Korrektur wird in `SensorManager::readInternalSensor()`/
`readExternalSensorIfEnabled()` NACH der Plausibilitätsprüfung auf den
Rohmesswert angewendet, bevor `DataManager::setSensor1()/setSensor2()`
aufgerufen wird. Da `SNMPManager::refreshValues()` denselben
`DataManager::getSensor1()/getSensor2()` liest wie die Weboberfläche und die
Stundenwerte-Aufzeichnung, propagiert die Korrektur automatisch überallhin -
kein separater Eingriff in `SNMPManager` nötig (einziger Grund, warum das
nicht reicht, wäre ein Wunsch nach einer vom lokalen Wert unabhängigen
"SNMP-only"-Korrektur, der hier nicht vorliegt). Luftfeuchte wird nach der
Korrektur auf [0, 100] geklemmt, Temperatur bewusst nicht.

### Webdesign: kein Lastenheft-Konflikt, da 20pt/Schwarz-Weiß nur Stilentscheidung war
Vor der Umstellung geprüft: das bisherige schwarz/weiße 20pt-Design (siehe
"Webserver-Designentscheidungen (P5)" oben) ist in `lastenheft.txt`
Abschnitt 5 nirgends als Anforderung festgehalten, nur die anzuzeigenden
DATEN sind spezifiziert - kein Zielkonflikt. `buildPageShell()` wurde auf
die Palette aus dem Sensormeter-Display-Projekt umgestellt (Navy-Banner
`#0f1f3d`, Orange-Akzent `#c8622a`, warmes Creme `#f2f0e9` für
Tabellenköpfe, Kartenrahmen `#e4e1d8`, Systemschriftart statt generischem
`sans-serif`) - dabei bewusst NUR das CSS geändert, alle HTML-Klassennamen
(`.block`/`.row`/`label`/...) unverändert gelassen, um keine der
zahlreichen HTML-Bau-Stellen in `buildMainPageBody()`/`buildSettingsPageBody()`
anfassen zu müssen. Chart.js-Linienfarben von hartcodiertem `red`/`blue`
auf `#a63d2e`/`#2a5ba0` (dieselben Töne wie im Sensormeter-Display-Graph)
geändert.

Mit `pio run` gebaut (erfolgreich, Flash 80,7 % / 1.058.225 B, RAM 16,0 % /
52.328 B). Nicht geflasht - kein WT32-ETH01-Board angeschlossen (nur das
Sensormeter-Display-Board war über USB verfügbar).

### Nachtrag: v0.9.0-rc2 + Doku aktualisiert

Nach der Kalibrierkorrektur+Webdesign-Änderung oben: Version auf
`0.9.0-rc2` (Beta) gesetzt (`config.h`/`config.h.example`, README).
`lastenheft.txt` Abschnitt 6 ("Sensoren") um die neuen
Kalibrierkorrektur-Felder ergänzt, Abschnitt 7 (SNMP) um den Hinweis, dass
die gemeldeten Werte bereits korrigiert sind. `pflichtenheft.txt` Abschnitt
5.2 korrigiert - dort stand noch "HTML + CSS (dark mode)", was seit der
Umstellung auf die Sensormeter-Display-Palette nicht mehr stimmt.

**Bewusst nicht angefasst:** `docs/sensormeter-onepager.pdf` hat kein
eingechecktes HTML-Quelldokument im Repo (anders als beim
Sensormeter-Display-Projekt, wo die HTML-Quelle aus der Git-Historie
wiederherstellbar war) - dieser Nachpflege-Rückstand bestand schon vor
dieser Änderung (siehe "Versionierung" oben) und wurde hier nicht
nachgeholt, da eine Neuerstellung ohne Vorlage ein eigenständiges Vorhaben
wäre, kein Teil dieser Doku-Aktualisierung.

Mit `pio run` neu gebaut zur Verifikation nach den Doku-Änderungen (Code
selbst unverändert seit dem letzten Build).

## Vier Verbesserungen aus Sensormeter WLAN übernommen

Sensormeter WLAN durchlief einen ausführlichen Testzyklus auf echter
Hardware und bekam dabei mehrere Fixes/Features, die auf denselben
Modul-Mustern beruhen wie hier (`NetworkManager`, `WebServerManager`,
`DisplayManager`). Auf Nutzerwunsch geprüft und - soweit die
Dual-Interface-Architektur (LAN **und** WLAN, nicht WLAN-only) es zulässt
- übernommen. Ausdrücklich **nicht** übernommen: der dortige
BOOT-Taster als Bedienelement (siehe eigener Abschnitt unten - hier aus
Hardware-Gründen nicht möglich).

### Fallback-Access-Point ist jetzt ein echter SoftAP

Bislang identisches Verhalten zum ursprünglichen Sensormeter-WLAN-Bug:
`NetworkManager` versuchte im Fallback-Fall per `WiFi.begin("installer",
"installer")` einem **bestehenden** Netz beizutreten, statt selbst eines
aufzuspannen - Widerspruch zu `lastenheft.txt` Abschnitt 8/12 ("eigener
Access Point"). Jetzt `WiFi.softAPConfig(192.168.4.1, 192.168.4.1,
255.255.255.0)` + `WiFi.softAP("installer", "installer")`, nur eigene IP +
Subnetzmaske, kein Gateway/DNS. Betrifft ausschließlich den WLAN-Zweig -
LAN bleibt unabhängig davon aktiv und hat weiterhin Vorrang; der Fallback-
AP startet nur, wenn nach 5 Minuten **weder** LAN **noch** WLAN eine IP
haben (`networkOk() = ethGotIp || wlanGotIp || apActive`).

Dazu passend: neuer Button „Verbinden & testen (Neustart)“ auf der
Einstellungsseite (nur innerhalb des WLAN-Formulars, nicht für LAN
relevant) - speichert SSID/PSK, setzt `DeviceConfig::wlanPendingTest`,
startet sofort neu. `NetworkManager::begin()` liest das Flag, löscht es
sofort wieder (gilt nur für einen Boot-Versuch) und wartet dann nur 30s
statt der regulären 5 Minuten, bevor es bei Misserfolg zurück in den
Fallback-AP fällt.

### WLAN-Scan blockierte den Async-Webserver-Task

Derselbe Bug wie bei Sensormeter WLAN: `WiFi.scanNetworks()` (blockierend,
mehrere Sekunden) direkt im AsyncWebServer-Request-Handler aufgerufen -
kann den Async-TCP-Task blockieren und zum Absturz führen, besonders
kritisch während das Gerät selbst als Fallback-AP läuft und ein Client
verbunden ist. Fix: `WiFi.scanNetworks(true)` (asynchron), `/api/wifi/scan`
liefert je nach `WiFi.scanComplete()`-Status `{"status":"started"|"running"|"done"}`
zurück, die Einstellungsseite pollt das per JavaScript alle 1,5s.

### Werksreset ergänzt

Neue Buttons im Bereich „Konfiguration“ der Einstellungsseite: „nur
Einstellungen“ setzt `config.xml` per `ConfigManager::setConfig
(DeviceConfig())` auf Defaults zurück, „Einstellungen + Daten“ löscht
zusätzlich `/history.csv`. Beide mit Sicherheitsabfrage, danach
automatischer Neustart (`/api/factory-reset`, Formularfeld
`scope=settings|all`). Gab es bei Sensormeter bisher gar nicht (auch nicht
vor der WLAN-Portierung).

### OLED-Anzeige überarbeitet: zentriert, feste größere Schrift, Scroll statt Schrumpfen

Wie bei Sensormeter WLAN, hier zusätzlich vereinfacht durch das bereits
vorhandene echte `systemType`-Feld (bei Sensormeter WLAN musste dafür erst
eine feste Konstante erfunden werden):

- Boot-Countdown zeigt jetzt 3 Zeilen (Systemname / Systemtyp / Countdown +
  "warte") statt bisher 2, jede Zeile mit eigener größtmöglicher
  Schriftgröße.
- Alle rotierenden Seiten sind jetzt horizontal UND vertikal zentriert
  (vorher linksbündig) und nutzen einheitlich eine feste, bewusst größere
  Schriftgröße (2), statt sie an die jeweils längste Zeile anzupassen.
  Läuft eine Zeile dabei über (z.B. eine lange WLAN-SSID), läuft sie
  waagerecht durch statt die Schrift für alle Zeilen zu schrumpfen -
  synchronisiert auf den 10s-Seitenwechsel-Timer.
  Nebenbei denselben latenten Absturzpfad behoben wie bei Sensormeter
  WLAN: die alte Schriftgrößen-Berechnung rundete bei sehr langen Zeilen
  per Integer-Division auf Größe 0 herunter und hätte automatisch
  umgebrochen.
- Neue sechste Seite "WLAN-Signal" (RSSI in dBm, nur relevant wenn das
  optionale WLAN tatsächlich verbunden oder der Fallback-AP aktiv ist).
- Neue dedizierte Fallback-Seite: zeigt ausschließlich "Fallback aktiv" +
  die eigene AP-IP, keine Seitenrotation, solange der Access Point läuft.

### BOOT-Taster NICHT übertragbar (Hardware-Grund)

Bei Sensormeter WLAN dient der ohnehin vorhandene BOOT-Taster (GPIO0)
zusätzlich als Bedienelement (Tipp = nächste Seite, Halten = Werksreset
mit Fail-Safe). Bei WT32-ETH01 ist GPIO0 fest als Takteingang für den
Ethernet-PHY verdrahtet (`ETH_CLOCK_GPIO0_IN`, siehe `pins.h` -
`ETH_CLK_MODE`), nicht als freier Taster verfügbar. Eine Portierung würde
einen zusätzlichen physischen Taster an einem freien GPIO plus
entsprechende Verkabelung erfordern - eine Hardware-Änderung, keine reine
Software-Portierung. Nicht umgesetzt in dieser Runde; bei Bedarf für eine
spätere Hardware-Revision vormerken.

Mit `pio run` gebaut und verifiziert (erfolgreich, Flash 83,7 % /
1.096.549 B, RAM 16,6 % / 54.288 B). Nicht geflasht - kein WT32-ETH01-Board
angeschlossen.

## Sicherheits-Feature: Ping-Check vor statischer IP-Vergabe

Beim Speichern der Einstellungsseite wird jetzt vor dem Übernehmen einer
neu gesetzten statischen LAN- oder WLAN-IP per Ping geprüft, ob im
jeweiligen Netz bereits ein anderes Gerät unter dieser Adresse antwortet.
Ist das der Fall, werden alle Einstellungen dieser Seite verworfen und
eine Fehlerseite ("IP-Adresse belegt") angezeigt statt eine
Adresskollision im Netz zu riskieren.

- Bibliothek `marian-craciunescu/ESP32Ping` neu in `platformio.ini`
  ergänzt (bei Sensormeter Display bereits vorhanden, dort für die
  Ping-Zielüberwachung).
- `Ping.ping(ip, 1)` mit der Bibliotheks-Standardwartezeit von 1s -
  deutlich kürzer als der mehrsekündige `WiFi.scanNetworks()`-Blockierfall,
  der früher bei Sensormeter WLAN zum Watchdog-Reset führte (siehe oben) -
  daher unproblematisch innerhalb des synchronen `AsyncWebServerRequest`-
  Handlers.
- Der Check läuft pro Interface nur, wenn DHCP deaktiviert ist UND sich
  die eingetragene Adresse von der aktuell aktiven LAN-/WLAN-IP
  unterscheidet - vermeidet einen Ping bei jedem Speichern unveränderter
  Einstellungen (das Formular deckt alle Einstellungsblöcke gemeinsam ab)
  und verhindert einen falschen Alarm, wenn die eigene, bereits aktive
  Adresse erneut als statisch bestätigt wird.
- Admin-Guide (Abschnitt 4.2, Anhang) entsprechend ergänzt.

Mit `pio run` gebaut und verifiziert (erfolgreich, Flash 83,9 % /
1.099.633 B, RAM 16,6 % / 54.368 B). Nicht geflasht - kein WT32-ETH01-Board
angeschlossen.

## Werks-Zugangsdaten projektübergreifend geprüft

Auf Anfrage geprüft, ob die Werks-Zugangsdaten bei allen drei
Sensormeter-Projekten konsistent sind. Ergebnis: Sensormeter und
Sensormeter WLAN nutzten bereits beide `installer` als Web-Passwort-
Default. Abweichend war Sensormeter Display (`admin`) - dort auf
`installer` vereinheitlicht, siehe `sensormeter-display/docs/entscheidungen.md`.
Der Benutzername ist bei allen drei Projekten ohnehin fest `admin` (kein
eigenes Feld).

## Portierungs-Kandidaten aus sensormeter-poe geprüft (nicht umgesetzt)

Für das in Planung befindliche `sensormeter-poe` (ESP32-S3-ETH) wurden im
Lastenheft/Pflichtenheft vier neue Features festgelegt: BOOT-Taster-
Bedienung, automatische Sensor-2-Modul-Erkennung (I2C-Scan + DHT-Probe),
Relais/Aktor-Steuerung, MQTT/Home-Assistant-Anbindung. Auf Anfrage
geprüft, ob diese auch auf Sensormeter (WT32-ETH01) übertragbar wären -
**nur geprüft/dokumentiert, nicht implementiert.**

- **BOOT-Taster: nicht übertragbar.** GPIO0 ist beim WT32-ETH01 fest mit
  dem Ethernet-Takt verdrahtet (bereits an anderer Stelle in diesem Log
  dokumentiert) - unverändert.
- **Sensor-2-Auto-Erkennung: übertragbar.** Dieselbe RJ45/I2C-Bus-
  Architektur (Display + externer Sensor auf gemeinsamem Bus) existiert
  hier bereits identisch. Ressourcenkosten minimal (reine Scan-Logik).
- **Relais/Aktor: übertragbar, sogar naheliegend.** `pins.h` reserviert
  bereits seit der ursprünglichen Verdrahtung `PIN_RJ45_PIN6_RELAY_OUT`
  und `PIN_RJ45_PIN7_RELAY_FB` (Pin 6/7) - nur bisher nie in Software
  angesteuert. Ressourcenkosten minimal.
- **MQTT/Home Assistant: übertragbar, Flash ist der limitierende
  Faktor.** Empirisch getestet (PubSubClient-Bibliothek mit realistischer
  Nutzung - setServer/setCallback/connect/publish/subscribe/loop, danach
  wieder entfernt): **+16.124 Byte Flash, +120 Byte RAM** gegenüber dem
  Stand von diesem Log-Eintrag (84,3 % / 1.104.613 B Flash, 16,6 % /
  54.368 B RAM, siehe oben). Bei nur noch ~201 KB freiem Flash (RAM
  dagegen komfortabel bei 16,6 %) realistisch mit den nötigen
  Manager-Klassen (MqttManager, RelayManager, SensorDetector) und der
  Web-UI-Erweiterung auf ~30-50 KB Gesamtkosten geschätzt - passt noch
  klar in die Reserve, verbraucht aber spürbar davon (~15-25 %).
  Implementierungsdetail bei einer künftigen Umsetzung: MQTT müsste über
  das jeweils aktive Interface laufen (LAN priorisiert, WLAN-Fallback)
  statt fest über `WiFiClient` wie im Testaufbau.

Vollständige Feature-Beschreibung (Ablauf, Pin-Rollen, MQTT-Discovery-
Schema) siehe `sensormeter-poe/repo/docs/lastenheft.txt` Abschnitte
11/14-16.

## MQTT/Home-Assistant-Anbindung implementiert (Sensor-Rolle)

Der oben geprüfte Portierungs-Kandidat "MQTT/Home Assistant" wurde jetzt
umgesetzt - beschränkt auf die Sensor-Rolle (Discovery + State für
Temperatur/Luftfeuchte), ohne Relais/Aktor, analog zu Sensormeter WLAN.

- Neuer `MqttManager` (Architekturvorbild: `SyslogManager`), zusätzlich
  angelehnt an Sensormeter WLANs bereits bestehende MQTT-Umsetzung, aber
  um Sensor 2 erweitert (`DataManager::getSensor2()`, Discovery/State nur
  bei aktiviertem `sensor2Enabled`).
- Neue `DeviceConfig`-Felder `mqttEnabled`/`mqttServer`/`mqttPort`/
  `mqttUser`/`mqttPassword`, per `<mqtt .../>`-Element in `config.xml`
  persistiert (Schema-Erweiterung in `ConfigManager.h`/`.cpp`, siehe
  Admin-Guide Abschnitt 7.1). Topic-Präfix wird **nicht** gespeichert,
  sondern wie der mDNS-Hostname zur Laufzeit aus dem Systemnamen
  abgeleitet (`NetworkManager::sanitizeHostname`) - identisches Muster zu
  Sensormeter WLAN.
- Discovery-Topics `homeassistant/sensor/<prefix>/<key>/config` (retained,
  einmalig je Reconnect gesendet), State-Topic `<prefix>/state` (JSON,
  gesendet bei jedem neuen Sensorzyklus - gleiche Änderungserkennung wie
  `SyslogManager`, über `lastReadMillis`).
- **Zwei-Interface-Besonderheit gegenüber Sensormeter WLAN**: dieses
  Projekt hat zwei mögliche aktive Interfaces (LAN + WLAN), Sensormeter
  WLAN nur eines. Ein einfacher `WiFiClient` genügt trotzdem als Transport
  für `PubSubClient`: die hier genutzte alte Arduino-ESP32-Version
  (2.0.17) besitzt keine separate `EthernetClient`-Klasse -
  `WiFiClient` ist nur ein dünner Wrapper um lwIP-BSD-Sockets, die
  unabhängig vom konkreten Netzwerk-Interface funktionieren (lwIP
  entscheidet das Routing anhand der Ziel-IP, nicht die
  Applikationsschicht). **Nicht verifiziert**: welches Interface
  tatsächlich genutzt wird, wenn LAN und WLAN gleichzeitig eine IP haben -
  auf echter Hardware noch nicht getestet.
- Einstellungsseite (`WebServerManager.cpp`) um einen neuen Block "MQTT
  (Home Assistant)" ergänzt (Aktiv-Checkbox, Broker-Adresse, Port,
  Benutzername, Passwort), REST-API (`/api/config` GET/POST) um die
  fünf Felder erweitert. Admin-Guide (Abschnitt 4.2, neuer Abschnitt 6.3,
  config.xml-Schema in 7.1) und One-Pager entsprechend ergänzt.
- Bewusst **kein** Relais/Aktor über MQTT, obwohl `pins.h` die
  RJ45-Relaispins bereits reserviert (siehe Eintrag oben,
  "Portierungs-Kandidaten...") - dort nur als "naheliegend" geprüft, hier
  nicht beauftragt.

Mit `pio run` gebaut und verifiziert (erfolgreich, Flash 86,1 % /
1.128.645 B, RAM 16,7 % / 54.568 B - gegenüber dem Stand vor dieser
Änderung, 84,3 % / 1.104.613 B Flash, 16,6 % / 54.368 B RAM, ein Zuwachs
von +24.032 B Flash / +200 B RAM, nah an der oben geschätzten Spanne).
Nicht geflasht - kein WT32-ETH01-Board in dieser Session angeschlossen.

## `scripts/flash.sh`: Mac-/Linux-Unterstützung umgesetzt

Die zuvor hier vermerkte Mac-Unterstützung für `scripts/flash.ps1` ist
jetzt umgesetzt - als eigenständiges `scripts/flash.sh` statt einem
direkten `pwsh`-Wiederverwendungsversuch (portabler, keine PowerShell-
Installation auf dem Zielsystem nötig), zusätzlich gleich auch für Linux
statt nur macOS. Deckt bewusst NUR den Flash-Vorgang ab - kein
`convert-logo.sh`/`snmp-load.sh`-Äquivalent (auf Anfrage explizit
ausgeschlossen).

Gleicher Ablauf wie `flash.ps1` (Projekt wählen → Python/Git/PlatformIO
prüfen/installieren → Repo klonen/aktualisieren → `config.h` anlegen →
`pio run` → `pio run --target upload`), plattformspezifisch ersetzt:

- **Werkzeug-Installation**: Homebrew (`brew install python`/`git`) auf
  macOS statt winget - Homebrew selbst wird NICHT automatisch installiert
  (ein unbeaufsichtigtes `curl | bash` erschien zu riskant), das Skript
  bricht mit einem Verweis auf https://brew.sh ab, falls `brew` fehlt. Auf
  Linux wird der vorhandene Paketmanager erkannt (`apt`/`dnf`/`pacman`/
  `zypper`) und der jeweils passende Installationsbefehl verwendet.
- **Serielle Geräte** statt COM-Ports: `/dev/cu.*` (macOS),
  `/dev/ttyUSB*`/`/dev/ttyACM*` (Linux).
- **Apple-Silicon-Pflicht weiterhin durchgesetzt, jetzt im Code**: prüft
  `uname -m` unter Darwin und bricht bei `x86_64` (Intel-Mac) mit
  Fehlermeldung ab - identische Einschränkung wie ursprünglich hier
  vermerkt, nur jetzt tatsächlich erzwungen statt nur dokumentiert.
- **PEP-668-Fallback**: neuere Debian-/Ubuntu-Versionen markieren das
  System-Python als "externally-managed" und verweigern ein einfaches
  `pip install` - das Skript erkennt genau diesen Fehler und wiederholt
  nur für das `platformio`-Paket selbst mit `--break-system-packages`.
- **Projekt-Metadaten als `case`-Funktionen statt assoziativem Array**,
  damit das Skript auch unter macOS' Standard-`/bin/bash` 3.2 läuft (kein
  `declare -A` dort, erst ab Bash 4).

Gilt für alle vier Projekte gleichermaßen, `scripts/flash.sh` liegt
identisch in allen vier Repos (wie `flash.ps1`, siehe "Flash-Skript
vereinheitlicht" oben).

**Verifiziert:** `bash -n`-Syntaxprüfung sowie ein per Betriebssystem-
Erkennungs-Patch simulierter Linux-Testlauf gegen einen echten,
bestehenden Checkout (in dieser Windows/Git-Bash-Umgebung, da kein
Mac/Linux-Rechner verfügbar war) - komplette Ablauf-Kette (Tool-Erkennung,
`git pull`, `config.h`-Prüfung, `pio run`, `--skip-upload`-Exit,
interaktives Projekt-Menü inkl. Fehleingabe-Behandlung) erfolgreich
durchlaufen. Die echten macOS-arm64-/Linux-nativen Zweige (Homebrew-/
apt-Installation, `/dev/cu.*`-/`/dev/ttyUSB*`-Auflistung) sind NICHT auf
echter Mac-/Linux-Hardware getestet.

## `scripts/convert-logo.ps1`: Logo-Konverter fürs Anbieter-Branding-Feature

Auf Anfrage umgesetzt: gemeinsames PowerShell-Skript (identisch in allen
vier Repos, analog `flash.ps1`), das ein beliebiges Anbieter-Logo in das
fürs Branding-Feature (siehe `sensormeter-wlan/repo/docs/entscheidungen.md`
"Anbieter-Branding (Weisslabel) implementiert") kompatible Rohformat
konvertiert. Nutzt .NET `System.Drawing` (Windows PowerShell 5.1, keine
zusätzliche Installation nötig) statt einer externen Abhängigkeit.

- Fragt zuerst (interaktiv per Menü oder `-Display`-Parameter), für
  welches der vier Displays konvertiert werden soll, und reduziert dann
  **sowohl** Auflösung **als auch** Farbtiefe konsequent auf das, was das
  jeweilige Display tatsächlich darstellen kann: 1-Bit-Monochrom für die
  drei OLEDs (SSD1306 128×64 bei Sensormeter/Sensormeter WLAN, SH1107
  128×128 bei Sensormeter PoE), RGB565 für das Farb-TFT bei Sensormeter
  Display (dort experimentell markiert, da dessen Branding-Firmware noch
  nicht existiert).
- Seitenverhältnistreue Einpassung (nicht verzerrt) mit zentrierter,
  konfigurierbarer Padding-Farbe (Default Schwarz, passend zum
  OLED-Hintergrund) statt einfachem Strecken/Stauchen.
- **Gefundener und behobener Bug vor der Verteilung**: PowerShells
  `-shl`/`-shr`-Operatoren behalten den .NET-Typ des *linken* Operanden
  bei. `System.Drawing.Color.R`/`.G`/`.B` sind `System.Byte` - ein
  `Byte -shl 11` beim RGB565-Packen überlief dadurch stillschweigend
  (8-Bit-Wertebereich) statt auf `Int32` erweitert zu werden. Ein reines
  Weiß (255,255,255) ergab dadurch `0x00FF` statt der korrekten
  RGB565-Kodierung `0xFFFF` - im Test sichtbar als blau eingefärbter
  Hintergrund statt Weiß. Gefunden durch ein generiertes Test-Logo, das
  Ergebnis unabhängig per Python zurückdekodiert und mit Pillow als PNG
  gespeichert (zeigte den Fehler visuell sofort). Behoben durch explizite
  `[int]`-Casts vor jeder Schiebeoperation; die monochrome Konvertierung
  war von diesem Bug nicht betroffen (dortige Werte bleiben immer
  innerhalb des Byte-Wertebereichs, daher keine sichtbare Auswirkung,
  trotzdem zur Robustheit ebenfalls mit `[int]`-Cast versehen).
- Nach dem Fix für alle vier Display-Profile (inkl. `custom`-Modus mit
  freier Größe/Farbtiefe) end-to-end getestet: erzeugte `.bin`-Dateien
  haben exakt die erwartete Byte-Zahl, und der jeweilige Bildinhalt wurde
  unabhängig zurückdekodiert und pixelgenau mit dem Quellbild
  abgeglichen (Kreis/Rechteck-Testmuster, sowohl monochrom als auch
  farbig).

Kein `pio run`-Bezug (reines Offline-Werkzeug, keine Firmware-Änderung).

## Anbieter-Branding (Weisslabel) auf Sensormeter portiert

Auf Anfrage von Sensormeter WLAN (dort erste Umsetzung, siehe dessen
`docs/entscheidungen.md`) unverändert hierher portiert: neuer
`BrandingManager` (freier Anbietername + optionales 128x64/1bpp-Logo auf
LittleFS, kein PNG/JPEG-Decoder), eigene OLED-Rotationsseite (nur Teil der
Rotation, wenn tatsächlich konfiguriert), Web-Header-Banner, Logo-Upload
per Multipart (gleiches Tmp-Datei-Muster wie `ConfigManager::save()`),
Web-Auslieferung als on-the-fly synthetisierter 1-Bit-BMP unter
`/branding/logo.bmp`.

Identisches Format zu Sensormeter WLAN (beide nutzen dasselbe SSD1306
128x64) - `scripts/convert-logo.ps1 -Display sensormeter` erzeugt daher
exakt dieselbe Ausgabedatei wie `-Display wlan`. Der bei Sensormeter WLAN
gefundene LittleFS-`exists()`-Logquirk (siehe dessen `entscheidungen.md`)
wurde von Anfang an mit dem dortigen Fix (RAM-Cache statt wiederholtem
Flash-Zugriff) übernommen, tritt hier also gar nicht erst auf.

Bei dieser Gelegenheit auch die zuvor fehlende MQTT-Dokumentation
nachgezogen: `docs/lastenheft.txt` hatte trotz bereits umgesetztem MQTT
(siehe "MQTT/Home-Assistant-Anbindung implementiert" oben) noch keinen
eigenen Abschnitt - jetzt Abschnitt 14 (MQTT) und Abschnitt 15 (Branding)
ergänzt, `docs/pflichtenheft.txt` um 3.6 MqttManager/3.7 BrandingManager
sowie 4.3/4.4 (config.xml/branding-logo.bin) erweitert.

**Nicht auf echter Hardware getestet**: kein WT32-ETH01-Board in dieser
Session angeschlossen (wie schon bei der MQTT-Umsetzung). Verifiziert
wurde ausschließlich per `pio run` (erfolgreicher Build) sowie die
BMP-Konstruktionslogik selbst bereits unabhängig bei Sensormeter WLAN
(Python+Pillow-Nachbau, siehe dortiges Protokoll) - identischer Code hier
übernommen, nicht erneut separat verifiziert.

Mit `pio run` gebaut und verifiziert (Flash 86,6 % / 1.134.697 B, RAM
17,6 % / 57.768 B - gegenüber dem MQTT-Stand oben, 86,1 % / 1.128.645 B
Flash, 16,7 % / 54.568 B RAM, ein Zuwachs von +6.052 B Flash / +3.200 B
RAM, nahezu identisch zum bei Sensormeter WLAN gemessenen Zuwachs).

## Verdrahtungsplan interaktiv: docs/verdrahtungsplan.html ergänzt

Auf Anfrage, familienweit für alle vier Projekte. Anders als bei
Sensormeter PoE (dort schon als HTML/SVG vorhanden) existierte hier bisher
nur `docs/verdrahtungsschema-v1.2.pdf` - ein sorgfältig versioniertes,
mehrseitiges PDF mit vollständigem Änderungsprotokoll (v1.0→v1.1→v1.2),
aber naturgemäß ohne jede Interaktivität.

Entscheidung: **beide Dokumente behalten**, nicht das PDF ersetzen. Das
PDF bleibt die ausführliche, versionierte Referenz mit Änderungshistorie
(sieben Seiten, u. a. mit Pin-für-Pin-Übersicht aller 20 Header-Pins) -
das neue `docs/verdrahtungsplan.html` ist eine interaktive Kurzfassung
nach dem bei Sensormeter PoE etablierten Muster (Inline-SVG, 15
Draht-Pfade, je ein unsichtbarer breiterer "Hit"-Pfad dahinter fürs
Anklicken, Info-Zeile mit "Von → Nach" bei Klick). Pin-Daten für die
Kurzfassung direkt aus `verdrahtungsschema-v1.2.pdf` übernommen (Seiten
2/4/6 - USB-Buchse-Tabelle, RJ45-Modul-Zusammenfassung, Pin-Belegung),
keine neuen Zuordnungen erfunden.

README.md und die "Stand der Verdrahtung"-Passage ergänzt: das PDF bleibt
"der einzig gültige Verdrahtungsstand", die HTML-Kurzfassung wird als
Browser-Companion dazu benannt - beide müssen bei künftigen
Pin-Korrekturen synchron gehalten werden.

Getestet mit Headless Chrome (`--dump-dom` + synthetischer Klick per
`MouseEvent`/`dispatchEvent` auf einen der 15 Drähte): korrektes
Hervorheben, korrekter "Von → Nach"-Text, korrektes Dimmen der übrigen 14
Drähte. Kein Board nötig, rein clientseitiges HTML/JS ohne Firmware-Bezug.

## Architekturübersicht auf vier Familienmitglieder erweitert, Sensormeter PoE ergänzt

`docs/projektfamilie.html` (identische Kopie in allen fünf
Sensormeter-Repos) zeigte bisher nur drei Karten (Sensormeter, Sensormeter
WLAN, Sensormeter Display) - Sensormeter PoE fehlte komplett, obwohl es
seit einiger Zeit ein vollwertiges viertes Familienmitglied ist. Nutzer
wies darauf hin, dass im Family-Repo die Übersicht sogar ganz fehlte.

Diagramm neu aufgebaut: drei Karten oben (WLAN/Sensormeter/PoE, Sensormeter
als Ursprung in der Mitte, dünne durchgezogene Linien zu den beiden
Varianten), Sensormeter Display darunter zentriert mit gestrichelten
SNMP-Linien zu allen drei. Neue Akzentfarbe `--rust` (Blitz-Icon) für PoE
ergänzt - fehlte bisher in `projektfamilie.html` (existierte nur in
`implementierungsplan.html`, dort wiederum ohne "-strong"-Variante).

Positionierung nicht mehr aus dem alten 3-Karten-Layout übernommen,
sondern per Skript (`getBoundingClientRect()` auf einer Kopie mit
angehängtem Mess-Script, da echte Karteninhalte wegen Zeilenumbruch bei
PoE deutlich höher sind als angenommen - 372px statt der ursprünglich
angenommenen ~240px) neu vermessen, um Überlappung mit den gestrichelten
SNMP-Linien zu vermeiden.

`docs/projektfamilie-light.png`/`-dark.png` (statische Vorschaubilder fürs
GitHub-README, da GitHub keine CSS-Media-Queries im Markdown auswertet)
aus dem aktualisierten Inhalt neu gerendert. README-Text ("wie die drei
Sensormeter-Projekte") und Bild-Alt-Text ebenfalls auf vier Projekte
korrigiert.

Getestet: Headless-Chrome-Screenshots hell/dunkel, `getBoundingClientRect()`-
Vermessung aller vier Karten (keine Überlappung mehr). Rein statisches
HTML/CSS/SVG ohne Firmware-Bezug, kein Board nötig.

## Serial-Kommandozeile + Werksreset-Umfangsauswahl (Port aus Sensormeter WLAN), Version auf 0.9.0-rc4

Beide Features wurden zuerst in Sensormeter WLAN gebaut und auf echter
Hardware verifiziert, dann hierher portiert - analog zum Muster, das die
Versionierungs-Umstellung weiter oben schon vorgezeichnet hat ("analog zu
Sensormeter WLAN ... dort zuerst eingeführt").

**Serial-Kommandozeile** (`handleSerialCommands()` in `main.cpp`, Abschnitt
8 im Admin-Guide): `status`, `dhcp <lan|wlan>`, `ip <lan|wlan> <ip> <maske>
<gateway> [dns]`, `wifi <ssid> <passwort>`, `dump`/`upload`, `reset`/`reset
all`. Anders als bei Sensormeter WLAN (nur ein Interface) brauchen
`dhcp`/`ip` hier ein Interface-Argument, da dieses Gerät LAN und optionales
WLAN parallel betreibt - `wifi` bleibt WLAN-only (Ethernet hat kein
Zugangsdaten-Konzept). `status` gibt zusätzlich zum WLAN-only-Vorbild auch
den LAN-Status/-IP sowie Sensor 2 (falls `sensor2Enabled`) aus. Besonders
relevant hier, da dieses Board (anders als Sensormeter WLAN) keinen
BOOT-Taster als Werksreset-Weg hat (siehe oben, "Kein Taster-Bedienelement")
- die Serial-Kommandozeile ist damit der einzige Netzwerk-unabhängige Weg
für einen Werksreset.

**Werksreset-Umfangsauswahl** (`handleApiFactoryReset()` in
`WebServerManager.cpp`): ersetzt die bisherigen zwei Buttons ("nur
Einstellungen" / "Einstellungen + Daten") durch ein Auswahlmenü mit vier
Umfängen (Alles / Nur Konfiguration / Nur Messwerte / Nur
Anbieter-Branding), inkl. JS-Bestätigungsdialog, der den gewählten Umfang
nennt. Der alte alles-oder-Einstellungen-Reset hatte einen Bug: er löschte
nie die Logo-Datei, selbst beim vollständigen "Einstellungen + Daten"-
Reset - behoben, "Alles" und "Nur Anbieter-Branding" rufen jetzt
`_branding.deleteLogo()` auf. "Nur Konfiguration" bewahrt gezielt
`brandingVendorName`, damit ein reiner Konfigurations-Reset das Branding
nicht mit entfernt.

Die BOOT-Taster-Logik gibt es hier nicht (s.o.); die Serial-Kommandos
`reset`/`reset all` bleiben bewusst der einfachere, von der granularen
Web-Umfangsauswahl unabhängige Codepfad (voller `DeviceConfig()`-Reset ohne
Branding-Erhalt, keine Logo-Löschung) - wie schon bei Sensormeter WLAN.

Version auf **`0.9.0-rc4`** angehoben (vorher `0.9.0-rc2`/`-rc3`, je nach
Datei uneinheitlich gepflegt - jetzt in `config.h(.example)`, README und
Admin-Guide-Badge/OLED-Skizze konsistent), passend zum gemeinsamen Stand
aller vier Sensormeter-Projekte.

Nur per `pio run` gebaut (kein Board für Sensormeter in dieser Sitzung
angeschlossen), Build erfolgreich (Flash 87.2%, RAM 17.6%). Noch nicht auf
echter Hardware verifiziert - offener Punkt für eine spätere Runde, analog
zum bereits dokumentierten Hardware-Verifizierungs-Rückstand.

## Familien-Standardlogo automatisch provisioniert, außer eigenes Logo bereits vorhanden

Analog zu Sensormeter WLAN (dort zuerst umgesetzt und auf echter Hardware
verifiziert, siehe dessen `entscheidungen.md`): das Standard-Branding-Logo
(Tri-Orbit + Dial Mark, siehe sensormeter-family) musste bisher nach jedem
Flash manuell über die Einstellungsseite hochgeladen werden. Jetzt
automatisch: `BrandingManager::begin()` prüft zuerst `checkLogoOnDisk()`
wie bisher, ruft bei fehlendem Logo aber zusätzlich neu
`provisionDefaultLogo()` auf, das das in `DefaultLogo.h` eingebettete
Rohbild (128×64, 1bpp, exakt 1024 Byte) einmalig auf LittleFS schreibt -
Tmp-Datei-plus-Umbenennen-Muster wie beim regulären Web-Upload.

**Genau das erwartete Verhalten, kein Entweder-Oder:** Ist bereits ein
Logo vorhanden (eigenes hochgeladenes ODER schon einmal automatisch
provisioniertes), bleibt es unangetastet - `provisionDefaultLogo()` wird
dann gar nicht erst aufgerufen. Ein Kunden-eigenes Logo wird also nie
überschrieben, auch nicht bei einem erneuten Firmware-Flash (ein normaler
`pio run --target upload` rührt die LittleFS-Datenpartition ohnehin nicht
an).

`DefaultLogo.h` liegt im Repo (kein Klartext-Geheimnis, nur Bilddaten) und
wird aus der vorkonvertierten Datei in `sensormeter-family/logo/` generiert
- bei einem neuen Default-Logo einfach neu konvertieren und den
Array-Inhalt ersetzen, nicht von Hand editieren.

Nur per `pio run` gebaut (kein Board für Sensormeter in dieser Sitzung
angeschlossen) - beide Zweige (Logo fehlt → automatisch schreiben; Logo
vorhanden → nichts tun) nur per Code-Review verifiziert, nicht auf echter
Hardware getestet.

## Partitionstabelle auf `min_spiffs.csv` umgestellt (Flash-Reserve deutlich vergrößert)

Analog zu Sensormeter WLAN (dort zuerst umgesetzt und auf echter Hardware
verifiziert, siehe dessen `entscheidungen.md`): das implizite
`default.csv`-Schema reservierte eine Datenpartition, die genauso groß war
wie eine der beiden OTA-App-Partitionen (~1,31 MB), obwohl nur `config.xml`,
`history.csv` und optional ein 1 KB Branding-Logo tatsächlich genutzt
werden - bei 87,4 % Flash-Auslastung der mit Abstand größte ungenutzte
Block.

Umgestellt auf `min_spiffs.csv` (`board_build.partitions`, mitgeliefert im
Framework-Paket, kein eigenes File nötig): `app0`/`app1` je `0x1E0000`
(≈1,88 MB statt ≈1,31 MB), `spiffs` nur noch `0x20000` (128 KB statt
≈1,31 MB). Zusätzlich `board_build.filesystem = littlefs` ergänzt (fehlte
hier bisher, war schon bei Sensormeter WLAN gesetzt) - ohne das baut
`pio run --target uploadfs` fälschlich ein SPIFFS-Image statt eines
LittleFS-Images.

**Ergebnis: Flash-Auslastung von 87,4 % auf 58,2 % gefallen**
(1.145.021 von jetzt 1.966.080 statt vorher 1.310.720 Byte), RAM
unverändert (17,6 %).

Nur per `pio run` gebaut (kein Board angeschlossen) - der eigentliche
Flash-Vorgang inkl. der bei Sensormeter WLAN nötig gewordenen
Config-Sicherung/-Wiederherstellung (Partitionswechsel verschiebt die
LittleFS-Partition physisch) ist hier noch nicht auf echter Hardware
durchlaufen, sollte aber identisch funktionieren - siehe die vollständige
Beschreibung des Ablaufs in Sensormeter WLANs `entscheidungen.md`.

## Modulerkennung + Relais/Aktor aus sensormeter-poe nachgerüstet, MQTT um Aktor-Rolle erweitert

Setzt die weiter oben dokumentierte Prüfung ("Portierungs-Kandidaten aus
sensormeter-poe geprüft") um - beide dort als "übertragbar" eingestuften,
aber unimplementierten Kandidaten (Sensor-2-Auto-Erkennung, Relais/Aktor)
sind jetzt umgesetzt, dazu die MQTT-Anbindung (bisher nur Sensor-Rolle, s.
Eintrag "MQTT/Home-Assistant-Anbindung implementiert (Sensor-Rolle)") um
die Aktor-Rolle erweitert. Der seinerzeit dokumentierte limitierende
Faktor "Flash" ist inzwischen entschärft: die Partitionstabellen-Umstellung
weiter oben hat die Reserve von ~201 KB auf ~813 KB vergrößert, lange bevor
diese Erweiterung überhaupt nötig war.

**Neu `SensorDetector`** (Wort-für-Wort aus
`sensormeter-poe/repo/firmware/src/SensorDetector.h/.cpp` übernommen, nur
Kommentar-Querverweise auf sensormeter-poe-spezifische Abschnittsnummern
entfernt): I2C-Scan (7-Bit-Adressraum, Display-Adresse 0x3C ausgenommen)
gefolgt von einem DHT22-Leseversuch auf RJ45 Pin 5 bei Fehlschlag. Setzt
`sensor2Enabled` automatisch bei einem Treffer, nie automatisch zurück auf
false. Läuft synchron beim Boot (nach `ConfigManager::begin()`, vor
`displayManager.begin()` - identische Platzierung wie bei sensormeter-poe)
sowie erneut auf Anfrage über den Button "Erkennung neu starten"
(`/api/detect/rerun`).

**Neu `RelayManager`** (ebenfalls nahezu wortgleich übernommen): treibt
`PIN_RJ45_PIN6_RELAY_OUT` (active LOW), liest optional
`PIN_RJ45_PIN7_RELAY_FB` zurück. Startet nach jedem Boot sicherheitshalber
mit AUS (kein persistierter Schaltzustand). Einziger Schreibpfad
(`setOn()`) wird von Weboberfläche (`/api/relay`), REST-API und
`MqttManager` (`<prefix>/relay/set`) gemeinsam genutzt - identisch zum
sensormeter-poe-Vorbild.

**`MqttManager` um Aktor-Rolle erweitert**: neuer `RelayManager&`-Konstruktorparameter,
`subscribeCommandTopics()` (abonniert `<prefix>/relay/set` nur wenn
`relayEnabled`), `publishRelayState()` (retained, sofort bei Änderung -
unabhängig vom 60s-Sensorzyklus), Discovery-Payload für eine
`homeassistant/switch/...`-Entity. Statischer `onMqttMessage()`-Callback +
Instanz-Zeiger (`PubSubClient` kennt keine Member-Function-Callbacks) -
dasselbe Muster wie bei sensormeter-poe.

**`DeviceConfig`/`ConfigManager` um `relayEnabled` erweitert**: neues
`<aktor relayEnabled="false"/>`-Element in `config.xml`, identisches Schema
zu sensormeter-poe. Kein `mqttTopicPrefix`-Override (anders als
sensormeter-poe) - hier weiterhin bewusst nur aus `systemName` abgeleitet,
wie bereits bei der ursprünglichen MQTT-Sensor-Rolle entschieden.

**`WebServerManager`**: neuer Abschnitt "Aktor" auf der Einstellungsseite
(Checkbox "Relais (Aktor) aktiv", Zustandsanzeige, Schalt-Button), neue
Zeile "Erkannter Modultyp/Chip" + Button "Erkennung neu starten" im
Sensoren-Abschnitt, JS-Funktionen `rerunDetection()`/`refreshRelayState()`/
`toggleRelay()`, `/api/relay` (GET/POST) und `/api/detect/rerun` (POST).

**Bewusst NICHT übertragen: BOOT-Taster** (`ButtonManager`) - bleibt wie in
der ursprünglichen Prüfung festgestellt technisch unmöglich, GPIO0 ist beim
WT32-ETH01 fest als Ethernet-Takteingang verdrahtet (siehe Abschnitt "Kein
Taster-Bedienelement" im Admin-Guide).

Flash-Kosten empirisch gemessen: **+8.700 Byte** (58,2 % → 58,7 %,
1.145.021 → 1.153.721 Byte) gegenüber dem Stand nach der
Partitionstabellen-Umstellung - deutlich günstiger als die seinerzeitige
Schätzung "~30-50 KB Gesamtkosten" (die schätzte MQTT UND Relais/Aktor UND
Modulerkennung UND Web-UI zusammen von einer damals noch nicht
implementierten MQTT-Basis aus; hier kam nur noch Relais+Erkennung+Aktor-
Rolle zur bereits bestehenden MQTT-Sensor-Rolle hinzu). RAM praktisch
unverändert (17,6 % → 17,7 %).

Nur per `pio run` gebaut (kein Board für Sensormeter in dieser Sitzung
angeschlossen) - Modulerkennung, Relais-Ansteuerung und MQTT-Aktor-Rolle
sind damit nur per Code-Review verifiziert, nicht auf echter Hardware
getestet.

## 2026-07-12 — Türkontakt auf RJ45 Pin 5 (Modulwahl "Sensor/Kontakt")

Erste Firmware-Umsetzung des in `sensormeter-family/repo/module-design/
README.md` konzipierten Türkontakt-Moduls (Kategorie 2, teilt sich Pin 5
mit dem DHT22/Sensor 2, siehe dortige "Bekannte Einschränkung der
Auto-Erkennung" und "Wichtiger Unterschied zu Sensor 2"). Scope bewusst
schmal gehalten: nur die manuelle Modulwahl + Zustandsanzeige, kein
MQTT-/SNMP-Anschluss (folgt bei Bedarf separat, siehe dortige Doku zur
binären Datenpfad-Trennung von Sensor 2).

**Neues Konfigurationsfeld `pin5Mode`** (`"sensor"` | `"contact"`,
Default `"sensor"`) statt eines zweiten unabhängigen Häkchens - beide
Belegungen liegen elektrisch auf demselben Pin und schließen sich zwingend
aus. Neues XML-Element `<kontakt pin5Mode="sensor" name="Kontakt"
messageOpen="Offen" messageClosed="Geschlossen"/>`. `contactName` wird
serverseitig auf 20 Zeichen begrenzt (Vorgabe), `contactMessageOpen`/
`contactMessageClosed` zusätzlich per `maxlength=30` im Formular begrenzt
(eigene, moderate Ergänzung - nicht explizit gefordert, aber sinnvoll
gegen unbegrenzt lange Meldungstexte).

**Neue Klasse `ContactManager`** (analog `RelayManager`): `begin()` setzt
`INPUT_PULLUP` auf `PIN_RJ45_PIN5_RESERVE` unabhängig vom Modus (harmlos,
da die DHT-Bibliothek den Pin bei jedem Leseversuch ohnehin selbst
umkonfiguriert). `loop()` liest den Pin nur bei `pin5Mode == "contact"`,
erkennt Zustandswechsel und protokolliert sie per `pushLogEntry()`. Bewusst
kein Caching in `DataManager` (anders als Sensor 1/2) - `isClosed()`/
`stateText()` sind reine Getter auf den zuletzt in `loop()` gelesenen
Zustand, analog zu `RelayManager::feedbackOn()`, das ebenfalls ohne
zentrale Datenhaltung auskommt.

**Bestehende DHT-Lesepfade gegen `pin5Mode == "contact"` abgesichert**:
`SensorManager::readExternalSensorIfEnabled()` bricht zusätzlich zum
bisherigen `sensor2Enabled`-Check bei `pin5Mode != "sensor"` ab;
`SensorDetector::runDetection()` überspringt den DHT-Leseversuch auf Pin 5
komplett, wenn `pin5Mode == "contact"` (ein Leseversuch würde auf einem
Kontaktmodul ohnehin nur fehlschlagen, kostet aber Zeit während des
Boot-Countdowns).

**`WebServerManager`**: neues Auswahlfeld "Modultyp RJ45 Pin 5" im
Sensoren-Abschnitt der Einstellungsseite, blendet per JS
(`togglePin5Mode()`) entweder die bisherigen Sensor-2-Felder oder die
neuen Kontakt-Felder (Name, Meldung offen, Meldung geschlossen,
Zustandsanzeige) ein/aus - beide Blöcke bleiben im DOM, nur `display`
wird umgeschaltet, damit das Formular beim Absenden konsistent alle Werte
mitschickt. Neuer rein lesender Endpunkt `/api/contact` (GET, liefert
`mode`/`name`/`closed`/`stateText`) - anders als beim Relais kein POST
nötig, der Zustand kommt vom Modul, nicht von einer Nutzeraktion.

Flash-Kosten empirisch gemessen: **+5.452 Byte** (59,0 % gesamt,
1.153.721 → 1.159.173 Byte), RAM unverändert bei 17,7 %.

Nur per `pio run` gebaut (kein Board angeschlossen) - noch nicht auf
echter Hardware getestet. Offen: das zugehörige Hardware-Modul-Dokument
`sensormeter-family/repo/module-design/tuerkontakt-modul.md` existiert
noch nicht (nur als Absatz in `module-design/README.md` vorgemerkt) sowie
eine mögliche spätere MQTT-`binary_sensor`-Discovery/SNMP-OID-Anbindung.

## 2026-07-12 — Einstellungsseite neu gegliedert ("Externe Schnittstelle" nach Kategorie 1/2), Kontakt-Meldungstexte durch "Alarm bei" ersetzt

Vor der Firmware-Umsetzung zunächst in einem lokalen Mock-Webserver
(Python-Skript im Scratchpad, kein Repo-Bestandteil) visuell erprobt -
siehe Gesprächsverlauf. Zwei Nutzer-Feedbackrunden führten zur jetzigen
Form, danach 1:1 in die echte Firmware übernommen:

**Bug gefunden und behoben**: "Erkannter Modultyp/Chip" (deckt sowohl
Kategorie-1-I2C- als auch den DHT-Fallback-Erkennungsversuch ab) steckte
bisher im bedingt sichtbaren `pin5SensorFields`-Block und verschwand damit,
sobald Modultyp "Kontakt" gewählt war - obwohl die I2C-Erkennung (Pin 3/4)
komplett unabhängig vom Pin-5-Modultyp läuft und laut Durchschleif-Regel
(`sensormeter-family/repo/module-design/README.md`) ein Kategorie-1- UND
ein Kategorie-2-Modul gleichzeitig in derselben Kette stecken können.

**Neue Gliederung**: "Sensoren" enthält jetzt nur noch Sensor 1 (intern).
Neuer Abschnitt "Externe Schnittstelle" mit zwei optisch abgegrenzten
Unterabschnitten (CSS-Klasse `.subsection`, heller Hintergrund/Rahmen
innerhalb des Blocks): "Kategorie 1 – Bus-Modul (I2C)" (Erkennungsanzeige +
Button, jetzt immer sichtbar) und "Kategorie 2 – Direkt-Module" (Relais-
Checkbox/Zustand/Schalt-Button, gefolgt vom Pin-5-"Modultyp"-Pulldown mit
den bekannten bedingten Sensor-/Kontakt-Feldern). Der bisherige
eigenständige "Aktor"-Block entfällt (Inhalt zieht in die Kategorie-2-
Unterabschnitt um).

**`contactMessageOpen`/`contactMessageClosed` (freier Text) ersetzt durch
`contactAlarmAt`** (`"open"` | `"closed"` | `"change"`, Default `"open"`):
einfacher und eindeutiger für eine reine Zustandsmeldung als zwei frei
editierbare Meldungstexte. `"open"`/`"closed"` sind zustandsgetriggert
(Alarm bleibt sichtbar, solange der Zustand anhält), `"change"` ist
kantengetriggert (Alarm nur einmalig nach einem tatsächlichen Wechsel).
`ContactManager` bekommt dafür `_justChanged` (in `loop()` bei jedem
Zustandswechsel gesetzt) sowie `alarmActive()` (reiner Peek, keine
Seiteneffekte) und `acknowledgeChange()` (löscht `_justChanged` - wird
NUR von `WebServerManager::handleApiContactGet()` aufgerufen, NICHT von
der Hauptseite oder dem Serial-Status, damit deren reine Anzeige das
kantengetriggerte Flag nicht versehentlich vor dem eigentlichen
API-Abruf konsumiert). `stateText()` wurde durch `stateLabel()` (reiner
Text "Offen"/"Geschlossen", nicht konsumierend) ersetzt; die
Alarm-Kennzeichnung ("(Alarm)") wird jetzt von den Aufrufern selbst
angehängt.

`config.xml`: `<kontakt>` verliert `messageOpen`/`messageClosed`, bekommt
stattdessen `alarmAt`. Kein automatischer Migrationspfad für bestehende
`config.xml`-Dateien nötig, da das Feature erst in derselben Sitzung
eingeführt und noch nicht auf echter Hardware im Feld war (siehe voriger
Abschnitt).

Gleichzeitig auch **erstmals nach `sensormeter-poe` (Waveshare
ESP32-S3-ETH) portiert** (dort existierte pin5Mode/Kontakt bislang gar
nicht) - siehe dortige `docs/entscheidungen.md` für den vollständigen
Portierungs-Eintrag; hier nur der Vollständigkeit halber vermerkt, dass ab
jetzt beide Projekte identischen Code für `ConfigManager`/`ContactManager`/
`WebServerManager` in diesem Bereich verwenden (wie schon bei
SensorDetector/RelayManager zuvor).

Flash-Kosten (Sensormeter, WT32-ETH01) empirisch gemessen: **+1.328 Byte**
gegenüber dem vorherigen Kontakt-Stand (1.159.173 → 1.160.501 Byte, 59,0 %
gesamt unverändert auf Prozent-Ebene), RAM unverändert bei 17,7 %.

Nur per `pio run` gebaut (kein Board angeschlossen) - Restrukturierung und
`alarmAt`-Logik damit nur per Code-Review verifiziert, nicht auf echter
Hardware getestet.

## 2026-07-12 — Relais: optionales automatisches Schalten nach Sensor-/Kontakt-Bedingung

Vor der Firmware-Umsetzung wie zuvor zunächst im lokalen Mock-Webserver
erprobt (`evaluate_relay_auto()`), danach 1:1 übernommen. Ergänzt das
bisher rein manuelle Relais um eine optionale automatische Schaltbedingung
- das manuelle Schalten (`/api/relay` POST) bleibt unverändert möglich,
wird aber bei aktiver Automatik beim nächsten Durchlauf wieder
überschrieben (bewusst kein "Vorrang für die letzte Aktion" - siehe
Hinweistext auf der Einstellungsseite).

**Neue `DeviceConfig`-Felder** (Default `relayAutoMode = "off"` -
bestehende Installationen verhalten sich dadurch unverändert manuell):
`relayAutoMode` (`"off"`|`"sensor"`), `relayAutoSource`
(`"sensor1"`|`"sensor2"`|`"contact"`), `relayAutoValue`
(`"temp"`|`"humidity"`), `relayAutoCompare` (`"above"`|`"below"`),
`relayAutoThreshold` (float), `relayAutoContactState`
(`"open"`|`"closed"`). Alles gebündelt im bestehenden `<aktor>`-Element
(keine neue XML-Wurzel nötig, gehört logisch zum Relais).

**`RelayManager` bekommt neuen `ContactManager&`-Konstruktorparameter**
(Reihenfolge in `main.cpp` entsprechend umgestellt: `contactManager` jetzt
vor `relayManager` deklariert) sowie eine neue `loop()`-Methode, die bei
`relayAutoMode == "sensor"` jeden Durchlauf neu auswertet: Quelle
`"contact"` vergleicht `ContactManager::isClosed()` gegen
`relayAutoContactState`; Quelle `"sensor1"/"sensor2"` liest
`DataManager::getSensor1()/getSensor2()` und vergleicht `temperature`
oder `humidity` (je `relayAutoValue`) per `relayAutoCompare` gegen
`relayAutoThreshold`. Liefert die Quelle keinen gültigen Wert (Sensor
nicht aktiv, `pin5Mode` passt nicht, `reading.valid == false`), bleibt
der zuletzt kommandierte Zustand bewusst unverändert - kein
Aus-Fallback, um kein unbeabsichtigtes Abschalten bei einem vorübergehend
fehlenden Messwert auszulösen. Nutzt intern weiterhin `setOn()` (bereits
No-Op-sicher bei `relayEnabled == false` und bei unverändertem Zustand),
kein zusätzlicher Schreibpfad nötig.

**`WebServerManager`**: neues Auswahlfeld "Automatisch schalten"
(Nein/Sensor) direkt unter dem bestehenden Relais-Bereich in Kategorie 2,
blendet per JS (`toggleRelayAuto()`/`toggleRelayAutoSource()`) die
passenden Folgefelder ein (Quelle → je nach Quelle Wert/Bedingung/
Schwellenwert für Sensor 1/2, oder Zustand für Kontakt). `/api/relay`
liefert zusätzlich `auto` (bool), Zustandsanzeige ergänzt um
"(automatisch)", wenn aktiv.

Flash-Kosten (Sensormeter, WT32-ETH01) empirisch gemessen: **+6.680 Byte**
(59,0 % → 59,4 %, 1.160.501 → 1.167.181 Byte), RAM unverändert bei 17,7 %.

Nur per `pio run` gebaut (kein Board angeschlossen) - automatische
Schaltlogik damit nur per Code-Review verifiziert, nicht auf echter
Hardware getestet.

## Optionales externes Display-Steckmodul (SH1107, I2C 0x3D)

Teil der familienweiten Display-Standardisierung (siehe
sensormeter-poe/repo/docs/entscheidungen.md "Internes Display: SH1107 ->
SSD1306" und sensormeter-family/repo/module-design/
sh1107-display-modul.md): das dort intern entfernte SH1107 128x128 gibt
es jetzt hier wie bei Sensormeter PoE als optionales externes
RJ45-Steckmodul.

**`SensorDetector.cpp`**: `EXTERNAL_DISPLAY_I2C_ADDRESS = 0x3D` zusätzlich
zu `DISPLAY_I2C_ADDRESS = 0x3C` vom I2C-Scan ausgenommen. Ohne diese
Ausnahme hätte ein gestecktes externes Display zwei Probleme verursacht:
als „unbekannter I2C-Sensor" fälschlich `sensor2Enabled` gesetzt, UND
(da `0x3D` vor den meisten bekannten Sensor-Adressen im Scan-Bereich
0x08-0x77 liegt) den Scan abgebrochen, bevor ein tatsächlich dahinter
gestecktes Sensor-Modul mit höherer Adresse gefunden würde.

**Neue Klasse `ExternalDisplayManager`**: bewusst kein Umbau von
`DisplayManager`, sondern eine eigene, davon unabhängige Klasse, die
`Adafruit_SH1107` auf `0x3D` anspricht und dieselben Infoseiten
(Systemname/-typ, IPs, Uhrzeit, Sensorwerte, Status, WLAN-Signal,
optional Branding) mit eigener 10s-Rotation zeichnet - inhaltlich fast
identisch zum vorherigen SH1107-Code bei Sensormeter PoE vor dessen
Display-Umbau, nur ohne Boot-Countdown-Seite und Fallback-AP-Sonderseite
(beide bleiben Aufgabe des internen Displays, siehe Klassenkommentar).
Branding-Seite zeigt bewusst nur den Vendor-Namen als Text, kein
Logo-Bitmap - das gespeicherte Logo ist für die interne 128x64-Anzeige
formatiert und würde auf dem 128x128 großen externen Display verzerrt
dargestellt; ein eigenes Logoformat für dieses Modul ist noch nicht
umgesetzt. Kein gestecktes Modul -> `begin()` schlägt fehl, `loop()` ist
ein No-op, exakt wie beim internen Display bei fehlendem Chip.

`platformio.ini`: `Adafruit SH110X` neu in `lib_deps` (bisher nicht
benötigt, da dieses Projekt intern schon immer SSD1306 hatte).

Flash-Kosten: 59,9 % (1.176.821 von 1.966.080 Byte, gegenüber 59,4 % vor
dieser Änderung). RAM 17,8 % (58.184 von 327.680 Byte). Nur per `pio run`
gebaut (kein Board angeschlossen) - insbesondere die eigentliche
Display-Ansteuerung nicht auf echter Hardware mit gestecktem externem
Modul getestet.

## I2C-Lesepfad für Sensor 2 (BME280, AHT20/AHT21)

Schließt einen Teil der in `sensormeter-family/repo/module-design/
README.md` als „Firmware-Lücke" dokumentierten Lücke: `SensorDetector`
erkennt I2C-Chips zuverlässig, aber `SensorManager::
readExternalSensorIfEnabled()` hat „Sensor 2" bisher **immer** per
DHT-Protokoll auf Pin 5 gelesen, unabhängig vom tatsächlich erkannten
Chip - ein erkanntes BME280/AHT20 schaltete den Systemtyp automatisch auf
„PRO", der anschließende Leseversuch schlug aber immer fehl.

**Nur BME280 und AHT20/AHT21 bekommen einen echten I2C-Lesepfad** - beide
liefern Temperatur+Feuchte und passen damit unverändert ins bestehende
`SensorReading`-Datenmodell (SNMP-Zweig `.4.x`, MQTT-`sensor`-Discovery,
Web-UI-Sensorformular, CSV-Export). **BH1750** (Lux) und **CCS811**
(eCO₂/TVOC) bleiben bewusst ausgeklammert - beide Messgrößen passen nicht
in dieses Schema; das würde neue Datentypen quer durch `DataManager`,
SNMP, MQTT, Web-UI und CSV erfordern, ein eigenständiges, deutlich
größeres Vorhaben, keine Erweiterung von `SensorManager` allein. Werden
sie erkannt, liest `readExternalSensorIfEnabled()` jetzt bewusst NICHTS
(kein Leseversuch, kein Fehler-Log) statt weiterhin sinnlos einen
DHT-Leseversuch zu unternehmen.

**`SensorDetector`** bekommt zwei neue öffentliche Getter,
`detectedChipName()` und `detectedI2cAddress()` (beide vorher privat), da
`SensorManager` jetzt wissen muss, WELCHER Chip erkannt wurde, nicht nur
DASS einer erkannt wurde - dafür hält `SensorManager` jetzt eine
`SensorDetector&`-Referenz (Konstruktor-Parameter, `main.cpp`
entsprechend umgestellt: `sensorDetector` vor `sensorManager` deklariert).

**Nebenbei behobener Bug**: `readExternalSensorIfEnabled()` prüfte
bisher `cfg.pin5Mode != "sensor"` als ersten Gate - das blockierte auch
einen I2C-Lesepfad vollständig, wenn Pin 5 gleichzeitig als Kontakt-
Eingang konfiguriert war, obwohl I2C (SCL/SDA) und Pin 5 unterschiedliche,
unabhängige Pins sind (ein Kontakt-Modul auf Pin 5 UND ein I2C-Sensor auf
dem Bus können gleichzeitig gesteckt sein, siehe module-design/
README.md). Die I2C-Zweige prüfen `pin5Mode` jetzt gar nicht mehr; nur
der DHT-Fallback-Zweig (Pin 5) tut das weiterhin.

**BME280: Luftdruck wird gemessen, aber nicht ausgewertet** - der Chip
liefert ihn, `readExternalBme280()` liest ihn aber nicht, da
`SensorReading` keine dritte Messgröße kennt (siehe `bme280-modul.md`).
`Adafruit_BME280::begin(address, &Wire)` wird bei jedem 60s-Lesezyklus
erneut aufgerufen (kein zusätzlicher „bereits initialisiert"-Zustand
nötig, Kosten sind bei diesem Takt nicht spürbar) - macht einen
Adresswechsel zwischen `0x76`/`0x77` nach erneuter Erkennung robust ohne
Sonderfall.

`platformio.ini`: `Adafruit BME280 Library` und `Adafruit AHTX0` neu in
`lib_deps` (beide nutzen die bereits vorhandene `Adafruit Unified
Sensor`-Bibliothek).

Flash-Kosten: 60,2 % (1.184.265 von 1.966.080 Byte, gegenüber 59,9 % vor
dieser Änderung). RAM 17,8 % (58.296 von 327.680 Byte). Nur per `pio run`
gebaut (kein Board angeschlossen) - insbesondere kein echtes BME280/
AHT20-Modul zum Testen vorhanden.

## RJ45 Pin 8: 5V statt Reserve

Auf ausdrücklichen Beschluss: RJ45 Pin 8 der Modulbuchse trägt künftig
fest die 5V-Versorgungsschiene des Geräts, statt wie bisher als
„Reserve" auf einen GPIO herausgeführt zu sein. Grund: aktuell nutzt
kein einziges entworfenes Modul (BME280, BH1750, AHT20/21, BME280+CCS811,
DHT22, Türkontakt, Relais, externes SH1107-Display) Pin 8 überhaupt -
er wurde bislang nur 1:1 durchgeschleift. Eine feste 5V-Schiene macht ihn
für künftige Module nutzbar, die mehr als 3,3V brauchen (z. B. manche
Relais-Spulen, stromhungrigere Aktoren), ohne dass jedes betroffene
Modul einen eigenen Aufwärtswandler braucht.

**Sicherheitsrelevanter Unterschied zu Sensormeter PoE**: bei diesem
Projekt war Pin 8 bisher mit `IO12` verdrahtet - einem echten
Flash-Spannungs-Boot-Strapping-Pin des ESP32, der beim Boot LOW sein
muss (bereits mit 10kΩ-Pull-down abgesichert, siehe `stueckliste.md`).
5V direkt auf `IO12` zu legen hätte sowohl den Bootvorgang gestört als
auch riskiert, den Chip zu beschädigen (GPIOs sind nur bis ~3,6V
spezifiziert). Die Umstellung trennt deshalb beide Funktionen komplett:

- `IO12` bleibt weiterhin mit seinem 10kΩ-Pull-down nach GND boot-kritisch
  abgesichert - **aber rein platinenintern**, ohne jede Verbindung mehr
  zur RJ45-Buchse.
- RJ45 Pin 8 bekommt eine komplett neue, davon unabhängige Leiterbahn
  direkt zur 5V-Versorgungsschiene des Geräts (Board-Eingang, kein GPIO).

Bei Sensormeter PoE ist das unkritischer, da dessen Pin 8 (`GPIO19`)
ohnehin kein Boot-Strapping-Pin war (siehe dessen `docs/
entscheidungen.md`) - dort ist die Umstellung nur eine Leiterbahn von
GPIO19 weg, hin zur 5V-Schiene, ohne Sicherheits-Implikation.

**Umgesetzte Änderungen**:
- `firmware/include/pins.h`: `PIN_RJ45_PIN8_RESERVE` (bisher `12`)
  entfernt - Pin 8 hat keinen GPIO mehr, also nichts, was Firmware lesen
  oder schreiben könnte. Bestätigt per Grep, dass dieses Define nirgendwo
  im Code referenziert wurde (reine Reserve, nie genutzt).
- `docs/verdrahtungsplan.html`: Pin-8-Zeile/-Draht von `IO12`/„Reserve,
  Boot-Strapping" auf `5V`/„5V-Versorgung" umgestellt, neuer Warn-Hinweis
  zur 3,3V-Verwechslungsgefahr, neuer Hinweis dass `IO12` weiterhin intern
  boot-kritisch bleibt.
- `docs/stueckliste.md`: Pull-down-Zeile klargestellt, dass sie rein
  platinenintern ist.
- **Offen, nicht automatisiert nachziehbar**: `docs/
  verdrahtungsschema-v1.2.pdf` ist eine Binärdatei und muss von Hand auf
  v1.3 aktualisiert werden, um wieder mit `verdrahtungsplan.html`
  synchron zu sein (siehe dortiger Hinweis-Callout).
- **Offen, nicht verifizierbar ohne echte Platine**: wie viel Strom die
  5V-Schiene an Pin 8 tatsächlich liefern kann (abhängig vom
  USB-Netzteil und der restlichen Last), ist mangels vorhandenem Board
  nicht nachgemessen - vor dem Bestücken eines ersten 5V-Moduls prüfen.

Betrifft nur Dokumentation/`pins.h` - kein Modul nutzt Pin 8 aktuell,
daher keine funktionale Änderung am Verhalten bestehender Module. Siehe
`sensormeter-poe/repo/docs/entscheidungen.md` für die analoge Änderung
dort und `sensormeter-family/repo/module-design/README.md` für die
familienweite Pinbelegungstabelle.

## 2026-07-15 — Periodischer I2C-Rescan (1x/Minute), ohne Taktänderung

Ursprünglich angefragt: I2C-Bus auf 50 kHz begrenzen + vollständiger Scan
beim Start + 1x/Minute, asynchron zur Sensorabfrage. Nach Analyse auf den
Minuten-Rescan reduziert - die Integration mehrerer gleichzeitig
erkannter Module folgt später:

- **50 kHz verworfen (fürs Erste)**: der Bus-Takt wird im App-Code nie
  gesetzt, sondern von den Display-Bibliotheken pro Frame selbst
  reingedrückt (internes SSD1306 zwingt 400 kHz während des Transfers,
  externes SH1107 sogar 1 MHz - beide Konstruktor-Defaults, nicht
  Applikationscode). Ein globales `Wire.setClock(50000)` würde bei jedem
  Display-Refresh sofort überschrieben. Eine echte 50-kHz-Beschränkung
  bräuchte Änderungen an den `clkDuring`/`clkAfter`-Parametern in
  `DisplayManager.cpp`/`ExternalDisplayManager.cpp` und würde den
  Frame-Flush deutlich verlangsamen (Display wird wegen der
  Scroll-Animation praktisch jeden Loop-Tick neu gezeichnet, nicht nur
  alle 10s) - zurückgestellt, bis eine konkrete Motivation (z.B.
  Signalintegrität über längere RJ45-Modulkabel) das rechtfertigt.
- **"Vollständiger Scan beim Start" existierte bereits**: `SensorDetector::
  runDetection()` sweept schon den kompletten 7-Bit-Adressraum
  (0x08-0x77, Displays ausgenommen) beim Boot - bricht aber beim ersten
  Treffer ab (`SensorDetector.cpp`, Kommentar "genau ein Modul
  erwartet"). Diese Break-Logik bleibt vorerst unverändert - Mehrfach-
  Erkennung ist Teil der später geplanten Modul-Integration.
- **"Asynchron zur Sensorabfrage" umgesetzt als eigener Timer, nicht als
  echte Parallelität**: die Firmware hat keinen RTOS-Task-Scheduler und
  keinen async I2C-Treiber - `loop()` ist ein einziger sequenzieller
  Task. "Asynchron" heißt hier: eigener `millis()`-Timer mit 60000ms-
  Intervall (`SensorDetector::loop()`, analog zum bestehenden Muster in
  `SensorManager::loop()`), unabhängig vom 60s-Lesezyklus von
  `SensorManager` getaktet, aber weiterhin blockierend im selben
  Loop-Tick ausgeführt.

**Umgesetzte Änderungen** (`SensorDetector.h`/`.cpp`, `main.cpp`):
- I2C-Sweep aus `runDetection()` in eine neue private Methode
  `scanI2cBus()` extrahiert (liefert `bool`, ob ein Treffer gefunden
  wurde), von `runDetection()` (Boot/manueller Button) UND der neuen
  `loop()`-Methode gemeinsam genutzt.
- `SensorDetector::loop()` neu: alle 60s (erster Aufruf sofort) ein
  reiner I2C-Rescan über `scanI2cBus()`. Bewusst **kein** erneuter
  DHT-Leseversuch dabei - `dhtProbe` teilt sich den GPIO mit
  `SensorManager`s `dhtExternal`; ein zusätzlicher Leseversuch jede
  Minute hätte den echten 60s-Sensor-Lesezyklus gestört (DHT-Sensoren
  brauchen minimalen Abstand zwischen Lesungen).
- Bei einem I2C-Treffer wird der Erkennungsstatus aktualisiert (auch bei
  Modulwechsel auf eine andere Adresse) und `sensor2Enabled` ggf.
  automatisch gesetzt, wie bei `runDetection()`. Bleibt der periodische
  Scan ergebnislos, wird der zuletzt bekannte Status **nicht**
  zurückgesetzt - sonst würde ein aktiv genutztes DHT-/Kontakt-Modul auf
  Pin 5 (das ein I2C-Scan naturgemäß nie "findet") jede Minute fälschlich
  auf "Kein Sensor" zurückfallen.
- `main.cpp`: `sensorDetector.loop();` im Hauptloop ergänzt.

Getestet: `pio run` für beide Projekte (sm hier, sm-poe siehe dortiges
Log) - baut sauber, keine neuen Warnungen. **Nicht getestet**: Verhalten
auf echter Hardware (Timing des periodischen Scans, tatsächliche Dauer
bei 112 Adressen, Zusammenspiel mit laufendem Netzwerk-/Web-Traffic) -
mangels angeschlossenem Board zum Zeitpunkt dieser Änderung.

### Nachtrag (gleicher Tag) — "vollständig" hieß auch: alle Treffer auswerten, nicht nur den ersten

Missverständnis in der ursprünglichen Umsetzung oben: "vollständiger Scan"
wurde nur auf den durchsuchten Adressbereich bezogen (0x08-0x77, der schon
vorher komplett durchlaufen wurde) — gemeint war aber zusätzlich, dass
**jede** gefundene Adresse ausgewertet wird, nicht nur die erste. Das ist
etwas anderes als die weiterhin zurückgestellte "Modul-Integration"
(mehrere Module gleichzeitig tatsächlich *nutzen*, z. B. gleichzeitig
auslesen) — hier geht es nur um die *Erkennung/Auswertung*, nicht um den
Lesepfad.

**Umgesetzte Korrektur** (`SensorDetector.h`/`.cpp`):
- `scanI2cBus()` bricht nicht mehr beim ersten Treffer ab (`break`
  entfernt), sondern durchläuft immer den kompletten Adressraum.
- Neue `I2cHit`-Struktur (Adresse + Chipname) und ein Array
  `_i2cHits[MAX_LOGGED_I2C_HITS=8]` sammeln **alle** Treffer eines Scans;
  jeder wird wie bisher einzeln über `Serial.printf` geloggt. Neue Getter
  `detectedI2cDeviceCount()`/`detectedI2cDeviceAt()`.
- Das **primäre** Gerät (niedrigste gefundene Adresse — bei den
  Kombimodulen also weiterhin AHT20/AHT21 vor BMP280/ENS160, siehe
  `sensormeter-family/repo/module-design/README.md`) wird weiterhin in
  `_detectedType`/`_detectedChipName`/`_detectedI2cAddress` gespiegelt,
  damit `SensorManager` unverändert nur genau eines liest — die
  tatsächliche Mehrfach-Nutzung bleibt bewusst offen.
- `detectedDescription()` hängt bei mehr als einem Treffer einen Hinweis
  an, z. B. „I2C-Sensor (AHT20/AHT21) (+1 weiteres I2C-Gerät)".
- Verhalten bei ergebnislosem periodischen Rescan (Status nicht
  zurücksetzen) bleibt unverändert, siehe oben.

Getestet: `pio run` für beide Projekte - baut sauber. Nicht getestet: echte
Hardware.

## 2026-07-15 — Sensor-2-Datenmodell erweitert: Druck/Lux/Luftgüte + values.csv-Größe

Start der Firmware-Einpflege für die in `module-design/README.md`
"Firmware-Lücke" gelisteten Punkte (BMP280-Chip-ID-Check, BH1750-Lux,
ENS160-Luftgüte, DHT11/DHT21-Typauswahl) - ausdrücklich NICHT die spätere
"Modul-Integration" (mehrere gleichzeitig gesteckte Module gemeinsam
lesen), die bleibt weiterhin offen, siehe SensorDetector-Klassenkommentar.

### Größenrechnung values.csv/Ringpuffer (explizit angefragt)

`HourValue` wächst von 3 Feldern (timestamp, temperature, humidity) auf 8
(timestamp, sensor1Temperature, sensor1Humidity, sensor2Temperature,
sensor2Humidity, sensor2PressureHpa, sensor2Lux, sensor2Eco2Ppm) - Sensor 1
bleibt immer intern/DHT11, Sensor 2 füllt je nach gestecktem Modul nur eine
Teilmenge, Rest NAN (leeres CSV-Feld statt "nan" oder falscher 0-Wert).

**Ergebnis: `RINGBUFFER_SIZE` (168 = 7 Tage stündlich) bleibt unverändert,
kein Verkleinern nötig.** Worst-case-Zeile (alle 7 Felder belegt, je 1
Nachkommastelle) liegt bei ~55-65 Byte, × 168 Zeilen ≈ 10-12 KB für
`history.csv` - auf der 128-KB-LittleFS-Partition (`min_spiffs.csv`, siehe
Eintrag "Flash-Setup-Skript" oben) bleiben nach Config (~2-3 KB),
Branding-Logo (~1-2 KB) und dem noch ausstehenden 64-KB-Log-Puffer (siehe
`sensormeter-family`-Memo "sm/sm-poe pending firmware work") immer noch
&gt;40 KB Luft. Sensormeter PoE hat mit `default_16MB.csv` ohnehin keine
relevante Enge.

### Umgesetzte Änderungen

- **`ConfigManager`**: neues Feld `pin5DhtType` ("DHT11"|"DHT21", Default
  "DHT21") - Auto-Unterscheidung DHT11/DHT21 ist laut Klassenkommentar von
  `SensorDetector` unzuverlässig, deshalb manuelle Auswahl auf der
  Einstellungsseite (wie schon `contactAlarmAt`). Persistiert im
  `<kontakt>`-Element neben `pin5Mode`.
- **`SensorManager`**: zwei DHT-Objekte auf demselben externen Pin
  (`dhtExternalDht11`/`dhtExternalDht21`) statt einem - der Adafruit-DHT-
  Treiber bindet den Typ am Konstruktor, daher kein Laufzeit-Umschalten
  möglich; `readExternalDht()` wählt anhand `cfg.pin5DhtType`. Drei neue
  Lesepfade `readExternalBmp280()`/`readExternalBh1750()`/
  `readExternalEns160()`, jeweils NUR die eine Messgröße liefernd (kein
  Temperatur/Feuchte-Wert im Sensor-2-Sinn). `loop()` umgebaut: die
  stündliche Ringpuffer-Speicherung hing bisher an `readInternalSensor()`
  (lief also VOR `readExternalSensorIfEnabled()` desselben Zyklus) - jetzt
  eigene `maybeRecordHourValue()`, nach beiden Lesungen aufgerufen, damit
  ein Stundenwert immer beide Sensoren desselben 60s-Zyklus abbildet.
- **`SensorDetector`**: 0x76/0x77 nicht mehr pauschal als "BME280" gemeldet
  - `knownChipName()` liest bei diesen zwei Adressen zusätzlich das Bosch-
  Chip-ID-Register (0xD0, roher `Wire`-Zugriff, keine neue Lib-Abhängigkeit
  in `SensorDetector`) und unterscheidet 0x60 (BME280) von 0x58 (BMP280).
  `KNOWN_CHIPS` um ENS160 (0x52/0x53) ergänzt.
- **`DataManager`**: `HourValue`/`SensorReading` erweitert (siehe oben),
  `saveRingbuffer()`/`loadRingbuffer()` auf 8-Spalten-CSV umgestellt
  (`parseFloatOrNan`/`formatFloatOrEmpty`). Alte 3-Spalten-Zeilen (Format
  vor heute) werden beim Laden übersprungen statt falsch interpretiert -
  ein einmaliger Verlust der bisherigen 7-Tage-Historie beim ersten Boot
  nach diesem Update ist die Konsequenz, kein Datenkorruptions-Risiko.
- **`WebServerManager`**: `values.csv`-Header/-Zeilen auf 8 Spalten
  erweitert, `/api/sensors` liefert jetzt auch `pressureHpa`/`lux`/
  `eco2Ppm`, `/api/graph` liefert zusätzlich zu den unveränderten
  `temperature`/`humidity`-Feldern (Sensor 1, vom bestehenden Chart.js-
  Chart gebunden) neue `temperature2`/`humidity2`/`pressureHpa`/`lux`/
  `eco2Ppm`-Arrays (Sensor 2) - **noch nicht im Chart dargestellt**,
  Frontend-Erweiterung ist bewusst nicht Teil dieser Änderung. Dashboard-
  Zeile und beide Display-Manager (`DisplayManager`/
  `ExternalDisplayManager`) zeigen je nach Modultyp Temp/Feuchte, Druck,
  Lux oder eCO2 an.
- **Bugfix, durch die Erweiterung aufgedeckt**: `SensorReading.valid`
  bedeutete bisher implizit immer "liefert echte Temperatur/Feuchte" - das
  gilt seit den drei neuen Modultypen nicht mehr (ein Druck-/Lux-/
  Luftgüte-Modul lässt `temperature`/`humidity` NAN, `valid` aber `true`).
  `SNMPManager`, `MqttManager` und `RelayManager` (automatisches Schalten
  mit Quelle "sensor2") prüften bisher nur `valid`, nicht zusätzlich
  `!isnan(temperature)` - ohne Korrektur hätte SNMP/MQTT NAN-Werte
  exportiert bzw. `RelayManager` das Relais bei jedem Zyklus unbemerkt
  ausgeschaltet (NAN-Vergleich ist in IEEE 754 immer `false`). Alle drei
  jetzt entsprechend abgesichert.
- **Neue Bibliotheksabhängigkeiten** (`platformio.ini`): `adafruit/Adafruit
  BMP280 Library@^3.0.0`, `claws/BH1750@^1.3.0`, `adafruit/ENS160 -
  Adafruit Fork@^3.0.1`.

### Bewusst nicht Teil dieser Änderung

- **SNMP/MQTT-Export der drei neuen Messgrößen** (Druck/Lux/eCO2) - beide
  exportieren weiterhin nur Sensor-1/2-Temperatur/Feuchte wie bisher, neue
  OIDs/Topics wären ein eigener Schritt.
- **Kalibrier-Offset für Druck** (analog zu `sensor2TempOffset`/
  `-HumOffset`) - eine sinnvolle Korrektur wäre höhenabhängig
  (Normalnull-Bezug), nicht nur ein fester Chip-Offset.
- **Dashboard-Chart-Darstellung** der neuen Sensor-2-Werte - Daten sind
  über `/api/graph` bereits abrufbar, nur die Chart.js-Konfiguration im
  Frontend wurde nicht erweitert.
- **Echtes gleichzeitiges Lesen mehrerer gesteckter Module** ("Modul-
  Integration") - weiterhin nur das primäre (niedrigste Adresse) Gerät
  wird tatsächlich gelesen, siehe Eintrag oben.
- **ENS160 Warmlaufzeit**: `begin()`/`setMode()` werden wie bei den
  anderen I2C-Sensoren bei jedem 60s-Zyklus neu aufgerufen - ob das die
  Basislinien-Kalibrierung des Metalloxid-Sensors gegenüber Dauerbetrieb
  verschlechtert, ist mangels echter Hardware nicht verifiziert.

Getestet: `pio run` für beide Projekte (sm hier: Flash 61,1% / RAM 18,9%,
vorher 60,4%/17,8% - sm-poe siehe dortiges Log) - baut sauber, drei neue
Bibliotheken erfolgreich aufgelöst. **Nicht getestet**: echte Hardware
(keiner der drei neuen Sensortypen physisch angeschlossen zum Zeitpunkt
dieser Änderung, kein Nachweis der tatsächlichen Messwerte/Timing-
Verhalten von ENS160s blockierendem `measure(true)`).

## 2026-07-16 — MQTT fest an ein Interface binden (löst "nicht verifiziert" oben auf)

Ergänzt die "Zwei-Interface-Besonderheit" oben (Zeile ~588): statt
weiterhin offenzulassen, welches Interface lwIP für die MQTT-Verbindung
wählt, wenn LAN und WLAN gleichzeitig eine IP haben, lässt sich das jetzt
über die Einstellungsseite fest vorgeben.

- Neues Feld `ConfigManager::mqttInterface` (`"lan"` | `"wlan"`, Default
  `"lan"`), per `interface`-Attribut im `<mqtt .../>`-Element persistiert.
  Bewusst **kein** `"auto"`/dritte Option - ist das gewählte Interface
  gerade nicht verbunden, schlägt der MQTT-Connect regulär fehl, auch wenn
  das jeweils andere Interface erreichbar wäre. Das ist beabsichtigt: der
  Nutzer soll ein Interface bewusst festlegen, kein stilles Failover.
- Einstellungsseite: neues Pulldown "Interface" (LAN/WLAN) im MQTT-Block,
  REST-API (`/api/config` GET/POST) um `mqttInterface` erweitert.
- `MqttManager::ensureConnected()` setzt vor jedem `connect()`-Versuch per
  lwIP `netif_set_default()` (`lwip/netif.h`) explizit das gewählte
  Interface als Default-Netif und stellt danach den vorherigen Zustand
  wieder her - andere Subsysteme (NTP, mDNS, ...) sollen von dieser
  MQTT-spezifischen Festlegung nicht dauerhaft betroffen sein.
- **Bewusst `netif_set_default()` (lwIP) statt `esp_netif_set_default_netif()`
  (esp_netif)**: letztere Funktion existiert nicht im `esp_netif.h`, das die
  hier genutzte Arduino-ESP32-2.0.17-Core tatsächlich bündelt (bestätigt per
  `pio run`-Fehlschlag beim ersten Versuch: "was not declared in this
  scope") - dieser ältere esp_netif-Header kennt nur
  `esp_netif_get_handle_from_ifkey()`/`esp_netif_get_netif_impl_index()`/
  `esp_netif_get_netif_impl_name()`, keine Default-Netif-Getter/Setter.
  Die darunterliegende lwIP-Funktion `netif_set_default()` gibt es dagegen
  in jeder lwIP-Version und ist im gebündelten `libesp_netif.a` bzw.
  `liblwip.a` tatsächlich exportiert (per `xtensa-esp32-elf-nm` geprüft -
  `esp_netif_set_default_netif` fehlt dort komplett als Symbol, nur eine
  interne, nicht exportierte `esp_netif_update_default_netif[_lwip]`-Variante
  existiert). Umsetzung: `esp_netif_get_netif_impl_index()` liefert den
  lwIP-"Netif-Index" (`netif->num + 1`), passend zu lwIPs
  `netif_get_by_index()` - so lässt sich vom esp_netif-Handle (ifkey
  `"ETH_DEF"`/`"WIFI_STA_DEF"`, identisch auf beiden Core-Versionen) auf das
  darunterliegende `struct netif*` schließen, ohne dass esp_netif dessen
  Pointer direkt exponieren muss.
- Sensormeter PoE (neuerer Core 3.x) hätte `esp_netif_set_default_netif()`
  zur Verfügung gehabt, nutzt aber bewusst denselben lwIP-Weg, damit beide
  Projekte identischen Code haben statt zweier divergierender
  Implementierungen für dasselbe Problem.

Getestet: `pio run` für beide Projekte, baut jeweils sauber (sm: Flash
61,2 %/RAM 18,9 %; sm-poe: Flash 22,3 %/RAM 19,8 %). **Nicht getestet**:
echte Hardware mit gleichzeitig aktivem LAN und WLAN (kein Nachweis, dass
der Broker tatsächlich über das gewählte statt eines anderen Interfaces
erreicht wird) - siehe weiterhin offene Verifikationslücke oben.

## 2026-07-16 — OTA-Upload: Projekt-/Versionspruefung gegen Verwechslungen

Bisher prueft `/api/ota/upload` nur die Basic-Auth (`checkAuth()`), aber
nichts am tatsaechlichen Inhalt der hochgeladenen `.bin` - eine Firmware
eines Schwesterprojekts (z.B. Sensormeter Display) oder eine aeltere
eigene Version liesse sich klaglos flashen. Umgesetzt in allen vier
Projekten der Familie (Sensormeter, Sensormeter PoE, Sensormeter WLAN,
Sensormeter Display) mit identischem Mechanismus:

- Jedes Projekt kompiliert einen eindeutigen Marker in die eigene `.bin`
  ein: `"SM-FW-ID:<FIRMWARE_PROJECT_ID>:<DEVICE_FIRMWARE_VERSION>:SM-FW-END"`
  (neue Konstante `kFirmwareIdentityMarker` in `main.cpp`, referenziert
  per `Serial.println()` beim Boot, damit der Linker sie nicht entfernt).
  `FIRMWARE_PROJECT_ID` ist ein neues Feld neben `DEVICE_FIRMWARE_VERSION`
  in `include/config.h(.example)` - hier `"SENSORMETER"`.
- `OtaManager::writeLocalUpdateChunk()` durchsucht den Byte-Stream des
  Uploads chunk-uebergreifend (kleiner Ueberlappungspuffer, falls der
  Marker genau an einer Chunk-Grenze zerschnitten ist) nach diesem
  Marker, parst Projekt-ID und Version heraus und vergleicht sie gegen
  die eigenen `FIRMWARE_PROJECT_ID`/`DEVICE_FIRMWARE_VERSION`. Der
  eigentliche `Update.write()` laeuft unabhaengig weiter (das OTA-Ziel-
  Slot-Partition-Schreiben ist unkritisch reversibel); erst
  `endLocalUpdate()` entscheidet anhand des Scan-Ergebnisses zwischen
  `Update.end(true)` (committen) und `Update.abort()` (verwerfen).
- **Downgrade-Sperre**: eine hochgeladene Version mit niedrigerer
  Semver-Praezedenz als die laufende wird abgelehnt, es sei denn, die
  neue Checkbox "Downgrade erzwingen" im Upload-Formular ist gesetzt
  (`OtaManager::setAllowDowngrade()`). Kein vollstaendiger
  RFC-Semver-Parser, deckt aber das hier genutzte `a.b.c[-rcN]`-Schema ab
  (bei gleichem `a.b.c` hat "kein Suffix" Vorrang vor "mit Suffix";
  zwei Suffixe werden lexikografisch verglichen, z.B. `rc3 < rc4`).
- **Formular-Reihenfolge ist bewusst**: die Checkbox steht im HTML VOR
  dem Datei-Feld, weil ESPAsyncWebServer Multipart-Felder in
  Body-Reihenfolge parst - ein Feld nach dem Datei-Input waere im
  Upload-Streaming-Callback (erster Chunk) noch nicht per
  `request->hasParam(..., true)` verfuegbar.
- Fehlermeldungen unterscheiden jetzt vier Faelle (Schreibfehler / kein
  Marker gefunden / falsches Projekt / zu alte Version) statt einem
  generischen "Update fehlgeschlagen".
- **Bewusst kein kryptografischer Schutz**: der Marker ist ein einfacher,
  im Klartext auffindbarer String - schuetzt vor versehentlichem
  Verwechseln der `.bin`-Dateien (der eigentliche Anlass: "wie stellen
  wir sicher, dass niemand die Sensormeter-Display-Firmware auf ein
  Sensormeter-PoE-Geraet hochlaedt"), nicht vor einem Angreifer, der
  bewusst einen falschen Marker einbaut. Basic-Auth bleibt die einzige
  Zugriffskontrolle fuer den Endpoint selbst.

Getestet: `pio run` fuer alle vier Projekte, baut jeweils sauber. Marker
in allen vier `firmware.bin` per Byte-Suche verifiziert (`SM-FW-ID:
SENSORMETER:0.9.0-rc4:SM-FW-END` hier, analoge Strings in den anderen
drei Projekten). **Nicht getestet**: echter OTA-Upload einer falschen
oder aelteren `.bin` auf echter Hardware (kein Board in dieser Session
angeschlossen) - die Chunk-uebergreifende Marker-Suche ist nur gegen
synthetische/kompilierte Binaries verifiziert, nicht gegen reale
ESPAsyncWebServer-Chunk-Groessen im Netzwerkbetrieb.

**Standing-Vorgabe fuer kuenftige Firmware-Builds**: dieser Mechanismus
(Marker-Konstante + Scan/Vergleich in OtaManager, Downgrade-Checkbox) ist
ab jetzt fester Bestandteil aller vier Projekte und muss bei neuen
Firmware-Versionen einfach mitlaufen (nichts Zusaetzliches zu pflegen -
`DEVICE_FIRMWARE_VERSION` wird ja schon bei jedem Release angepasst,
`FIRMWARE_PROJECT_ID` aendert sich nie). Falls ein fuenftes
Sensormeter-Projekt entsteht, braucht es denselben Marker-Mechanismus mit
einer neuen eindeutigen `FIRMWARE_PROJECT_ID`.

## 2026-07-16 — Persistenter Log-Puffer auf LittleFS (/log.txt) + WARNING-Stufe

Portiert aus Sensormeter WLAN (siehe dortiger Eintrag vom selben Tag fuer
die Groessen-/Rotationsrechnung, hier identisch: 32 KB je Datei, Rotation
nach `/log.old.txt`). Der bisherige RAM-Ringpuffer (`LOG_CAPACITY = 5`)
bleibt unveraendert fuer die "letzte 5 Meldungen"-Webseite und den
SyslogManager-Versand; die neue Datei ist eine zusaetzliche, neustart-feste
Historie.

- Neue benannte Severity-Stufen `DataManager::SEVERITY_ERROR/_WARNING/_INFO`
  (3/4/6) statt roher Zahlen.
- `pushLogEntry()` haengt jeden Eintrag zusaetzlich an `/log.txt` an
  (`appendLogFile()`, Rotation bei 32 KB).
- **Anders als bei Sensormeter WLAN**: dieses Projekt hat zwei Interfaces
  (LAN+WLAN). Der bestehende `networkOk()`-Zustandsautomat loggt bisher nur
  den GLEICHZEITIGEN Ausfall beider Interfaces als `ERROR` - verlor eines
  der beiden Interfaces waehrend das andere trug, gab es dafuer *gar
  keinen* Log-Eintrag. Neue `NetworkManager::logInterfaceTransitions()`
  (in `loop()` bei jedem Tick aufgerufen, unabhaengig vom
  Zustandsautomaten) trackt LAN und WLAN jeweils EINZELN und loggt Verlust
  als `WARNING` bzw. Wiederverbindung als `INFO` mit Ausfalldauer - unabhaengig
  davon, ob das jeweils andere Interface gerade oben ist. Der bestehende
  `ERROR`-Eintrag bei komplettem Ausfall bleibt unveraendert bestehen (kann
  jetzt zusammen mit den zwei neuen WARNING-Eintraegen fuer LAN und WLAN im
  selben Tick erscheinen - bewusst leicht redundant fuer bessere Diagnose-
  Granularitaet, kein Fehler).
- Web-UI: `/log.txt`/`/log.old.txt` aus LittleFS gestreamt, neue Buttons
  "Log"/"Log (alt)" auf der Hauptseite (identisch zu Sensormeter WLAN).

Getestet: `pio run` - baut sauber (Flash 61,6%/RAM 19,0%, vorher
61,3%/19,0%). Nicht getestet: echte Hardware ueber mehrere Tage/echtes
LAN+WLAN-Flapping-Szenario.

**Standing-Vorgabe**: analog zur OTA-Pruefung oben ist dieser Mechanismus
ab jetzt fester Bestandteil dieses Projekts.

## 2026-07-16 — Verdrahtungsplan gegen Board-Foto (WT32-ETH01-back.png) geprueft

Auf Anweisung ausschliesslich `WT32-ETH01-back.png` (Foto der
Platinenrueckseite, `V1.4`-Silkscreen) als Quelle herangezogen, bewusst
kein Datenblatt - analoges Vorgehen zur sm-poe-Verdrahtungsplan-Korrektur
weiter oben in diesem Projekt-Umfeld, hier aber mit einer offenen statt
einer abschliessend geklaerten Diskrepanz (siehe unten).

- **`IO0` faelschlich als "nicht verfuegbar" gefuehrt**: Abschnitt
  "Vermiedene Pins" listete `IO0` bisher neben den LAN8720-internen Pins
  als nicht am Header verfuegbar - das Foto zeigt `IO0` eindeutig als
  eigenen Header-Pin. Korrigiert.
- **Neuer Abschnitt "Reale Pinbelegung laut Board-Foto"**: zweispaltiges
  SVG-Schema mit allen 26 auf dem Foto sichtbaren Pins in ihrer echten
  physischen Reihenfolge (nicht nur den 9 aktuell genutzten) - analog zur
  sm-poe-Diagrammphilosophie "reale Lage, damit man beim Bau Pins
  zaehlen kann", hier aber als zusaetzliche Referenztabelle neben dem
  bestehenden logischen Schaltplan, nicht als dessen Ersatz.
- **Offene, NICHT geloeste Diskrepanz**: `IO32`, `IO33` (I2C SCL/SDA)
  und `IO5` (Relais-Steuerung) - alle drei von Firmware/`pins.h` genutzt
  - erscheinen auf dem Foto unter diesem Namen nicht in der
  Pin-Beschriftung. Stattdessen zeigt dieses Board zusaetzliche, bisher
  nicht dokumentierte Pins `CFG`, `485_EN`, `RXD`, `TXD`, `LINK` - deutet
  auf eine Board-Variante mit eingebautem RS485-Transceiver hin (ueber
  diese Variante war zuvor nichts in den Projektunterlagen vermerkt).
  Ob IO32/33/5 auf dieser Variante unter einem der neuen Namen mitlaufen
  oder tatsaechlich nicht herausgefuehrt sind, laesst sich allein aus dem
  Foto nicht klaeren - dafuer waere das im Projektordner ebenfalls
  vorhandene `WT32-ETH01_datasheet_V1.1- en.pdf` noetig, das in dieser
  Anpassung bewusst nicht herangezogen wurde (Anweisung: nur das genannte
  Bild). **`pins.h`/Firmware wurden NICHT angepasst** - die Firmware geht
  weiterhin von IO32/33/5 aus, das ist bewusst unveraendert, bis die
  Diskrepanz mit dem Datenblatt oder einem Multimeter am echten Board
  geklaert ist.
- Eine zweite `EN`-Beschriftung in der rechten Spalte (Position 4, direkt
  unter `3V3`) liess sich in der Foto-Aufloesung nicht mit letzter
  Sicherheit von einer Verwechslung unterscheiden - im Text als
  Unsicherheit vermerkt statt stillschweigend uebernommen.

Rein dokumentarische Aenderung (`docs/verdrahtungsplan.html`), kein
Firmware-/`pio run`-Bezug. Nicht getestet: keine Verifikation gegen das
Datenblatt oder ein Multimeter in dieser Sitzung (bewusst ausserhalb des
Auftragsumfangs) - die IO32/33/5-Frage bleibt fuer eine Folgesitzung
offen.
