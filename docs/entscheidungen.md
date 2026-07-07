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

## 2026-07-08 — Repo-Kuration

In diesem Repo wird nur die Kern-Dokumentation versioniert (Lastenheft,
Pflichtenheft, aktuelles Verdrahtungsschema, Flash-Anleitung,
Pinout-Referenz, Implementierungsplan, Stückliste). Produktfotos,
Datenblatt-PDF, Fritzing-/Visio-Dateien, 3D-Druckvorlagen,
HTML-Diagramm-Exporte und das ESP32-Fachbuch bleiben lokal außerhalb des
Repos.
