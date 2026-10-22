/* =====================================================================
   settings.h  —  Persistent settings (JSON on LittleFS flash)
   ===================================================================== */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

struct Settings {
  String  wifiSsid;
  String  wifiPass;
  // AS3935
  uint8_t outdoor;      // 0 = indoor AFE gain, 1 = outdoor
  uint8_t noiseFloor;   // NF_LEV 0..7 (the minimum when auto noise floor is on)
  uint8_t watchdog;     // WDTH 0..10
  uint8_t spikeRej;     // SREJ 0..11
  uint8_t minStrikes;   // 1, 5, 9 or 16
  uint8_t maskDist;     // 1 = chip does not report disturbers
  uint8_t autoNoise;    // 1 = raise the noise floor while noise is too high
  int8_t  tuneCap;      // 0..15, -1 = never tuned
  // Map
  uint8_t hasLoc;       // 1 = sensor location set
  float   lat, lon;     // degrees (float: ~0.5 m resolution, plenty here)
};

extern Settings settings;

void loadSettings();   // fills `settings` from config.json, or defaults
bool saveSettings();   // writes `settings` to config.json (true on success)
void clampSettings();  // forces every field into its legal range

#endif // SETTINGS_H
