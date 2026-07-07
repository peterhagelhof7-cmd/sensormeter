#pragma once

// ============================================================================
// WT32-ETH01 - Pinbelegung
// Einzige gueltige Quelle: docs/verdrahtungsschema-v1.1.pdf (korrigiert)
// Siehe auch docs/entscheidungen.md fuer den Hintergrund der Korrektur.
// ============================================================================

// --- Ethernet PHY (LAN8720) - fest verdrahtet, NICHT aendern -----------------
#define ETH_PHY_TYPE   ETH_PHY_LAN8720
#define ETH_PHY_ADDR   1
#define ETH_PHY_MDC    23
#define ETH_PHY_MDIO   18
#define ETH_PHY_POWER  16
#define ETH_CLK_MODE   ETH_CLOCK_GPIO0_IN

// --- I2C-Bus: Display (SSD1306) + externer I2C-Sensor am RJ45 ---------------
// v1.1: vorher IO21/IO22 - das kollidierte mit dem Ethernet-PHY (RMII TX_EN/TXD1)
#define PIN_I2C_SCL 32
#define PIN_I2C_SDA 33

// --- Sensor 1 (intern, DHT11) ------------------------------------------------
#define PIN_DHT_INTERNAL 4
// Pull-up 10k zwischen PIN_DHT_INTERNAL und 3.3V erforderlich (siehe Stueckliste)

// --- RJ45 Modularanschluss (Sensor 2 / Relais / Reserve) --------------------
// v1.1: RJ45 Pin 5 vorher IO4 - das kollidierte mit dem internen DHT11
#define PIN_RJ45_PIN5_RESERVE   15
#define PIN_RJ45_PIN6_RELAY_OUT 5
#define PIN_RJ45_PIN7_RELAY_FB  14
#define PIN_RJ45_PIN8_RESERVE   12  // Boot-Strapping-Pin: muss beim Boot LOW sein (Pull-down 10k -> GND)

// --- Sensor 2 (extern, DHT22 ueber RJ45) -------------------------------------
// docs/verdrahtungsschema-v1.1.pdf widerspricht sich an dieser Stelle selbst:
// die Modul-Adapter-Seite (S.4) nennt RJ45-Pin 3 als DHT-DATA, die
// verbindliche Pin-Zusammenfassung (S.6/7) legt Pin 3 aber dauerhaft auf
// IO32 = I2C-SCL (gemeinsamer Bus mit dem Display) fest. Ein einzelner
// GPIO-Sensor (DHT) kann nicht dieselbe Leitung wie ein staendig aktiver
// I2C-Bus nutzen. Aufloesung (siehe docs/entscheidungen.md): DHT22 extern
// nutzt stattdessen RJ45 Pin 5 / IO15 - das ist laut selbem Dokument der
// einzige tatsaechlich freie, nicht boot-kritische GPIO am RJ45-Anschluss.
#define PIN_DHT_EXTERNAL PIN_RJ45_PIN5_RESERVE
