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

## `scripts/flash.ps1`: Mac-Unterstützung geplant, nicht umgesetzt

Auf Anfrage vermerkt für eine künftige Umsetzung: `scripts/flash.ps1` soll
zusätzlich auf dem Mac lauffähig werden - **ausdrücklich nur für
Apple-Silicon-Macs (ARM, M1/M2/M3/...)**, nicht für ältere Intel-Macs.
Noch nicht begonnen, keine Codeänderung in dieser Session. Zu klären bei
Umsetzung:

- PowerShell selbst läuft auch auf ARM-Macs (PowerShell 7+/`pwsh`, via
  Homebrew) - denkbar wäre ein direkter Wiederverwendungsversuch von
  `flash.ps1` unter `pwsh` statt einer Neuimplementierung als
  separates `flash.sh`.
- Windows-spezifische Anteile müssten identifiziert und ersetzt werden:
  winget-basierte Toolinstallation (Python/Git/PlatformIO), COM-Port-
  Erkennung (unter macOS `/dev/cu.usbserial-*`/`/dev/cu.SLAB_USBtoUART`
  statt `COMx`), sowie alle Debug-Burning-Hinweise zur USB-Seriell-
  Adaptererkennung.
- Gilt für alle vier Projekte gleichermaßen, da `scripts/flash.ps1`
  identisch in allen vier Repos liegt (siehe "Flash-Skript
  vereinheitlicht" oben) - eine Mac-Fassung müsste ebenso in alle vier
  Repos verteilt werden.
