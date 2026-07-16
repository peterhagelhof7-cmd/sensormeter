#pragma once

// ============================================================================
// WT32-ETH01 - Pinbelegung
// Einzige gueltige Quelle: docs/verdrahtungsschema-v1.2.pdf (korrigiert)
// ============================================================================

// --- Ethernet PHY (LAN8720) - fest verdrahtet, NICHT aendern -----------------
#define ETH_PHY_TYPE   ETH_PHY_LAN8720
#define ETH_PHY_ADDR   1
#define ETH_PHY_MDC    23
#define ETH_PHY_MDIO   18
#define ETH_PHY_POWER  16
#define ETH_CLK_MODE   ETH_CLOCK_GPIO0_IN

// --- I2C-Bus: Display (SSD1306) + externer I2C-Sensor am RJ45 ---------------
// Silkscreen auf dem Board zeigt NICHT "IO32"/"IO33", sondern die
// Sonderfunktionsnamen "CFG" (=IO32, Pin 2 rechte Spalte) und "485_EN"
// (=IO33, Pin 3 rechte Spalte) - siehe docs/entscheidungen.md
// "IO32/IO33-Frage endgueltig geklaert" fuer die Herleitung (Datenblatt
// Tabelle-2 + Boardfoto + 3 externe Quellen).
#define PIN_I2C_SCL 32
#define PIN_I2C_SDA 33

// --- Sensor 1 (intern, DHT11) ------------------------------------------------
#define PIN_DHT_INTERNAL 4
// Pull-up 10k zwischen PIN_DHT_INTERNAL und 3.3V erforderlich (siehe Stueckliste)

// --- RJ45 Modularanschluss (Sensor 2 / Relais / 5V) --------------------------
#define PIN_RJ45_PIN5_RESERVE   15
#define PIN_RJ45_PIN6_RELAY_OUT 5
#define PIN_RJ45_PIN7_RELAY_FB  14
// Pin 8 liegt seit der Entscheidung "RJ45 Pin 8: 5V statt Reserve" (siehe
// docs/entscheidungen.md) direkt auf der 5V-Versorgungsschiene des Geraets -
// KEIN GPIO mehr, deshalb kein #define hier (Firmware hat nichts zu lesen
// oder zu schreiben). GPIO12 (bisher ueber diesen Pin herausgefuehrt) bleibt
// weiterhin ein Boot-Strapping-Pin und braucht seinen 10k-Pull-down nach GND
// nach wie vor - der sitzt jetzt aber rein platinenintern zwischen GPIO12 und
// GND, OHNE Verbindung zur RJ45-Buchse (siehe docs/entscheidungen.md fuer die
// vollstaendige Begruendung und den daraus folgenden Verdrahtungs-Umbau).

// --- Sensor 2 (extern, DHT22 ueber RJ45 Pin 5) -------------------------------
#define PIN_DHT_EXTERNAL PIN_RJ45_PIN5_RESERVE
