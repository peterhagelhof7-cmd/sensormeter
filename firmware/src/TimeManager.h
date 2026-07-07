#pragma once

// NTP-Sync gemaess docs/lastenheft.txt: de.pool.ntp.org, 60s nach Boot,
// danach alle 5h, zusaetzlich nach jedem Link-Up-Event. Sommerzeit (CET/CEST)
// und die Fehler-Fallback-Kette (DHCP-Test/Restore) folgen in Phase P1.
// P0 legt nur das Modul-Grundgeruest an.

class TimeManager {
 public:
  void begin();
  void loop();

 private:
  bool _synced = false;
};
