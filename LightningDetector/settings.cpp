/* =====================================================================
   settings.cpp  —  Minimal JSON persistence on LittleFS (no ArduinoJson)
   ---------------------------------------------------------------------
   The config file is small and written only by us, so a tiny hand-rolled
   reader/writer does the job without ArduinoJson's heap use. Example:

   {
     "ssid":"MyNetwork",
     "pass":"secret",
     "outdoor":0,
     "noiseFloor":2,
     ...
     "lat":47.606200,
     "lon":-122.332100
   }

   The file is written to a temp name and renamed over the old one, so a
   power cut mid-save leaves the previous settings intact.
   ===================================================================== */
#include "settings.h"
#include "config.h"
#include "storage.h"

Settings settings;

#define CONFIG_TMP_PATH "/config.tmp"

static void applyDefaults() {
  settings.wifiSsid   = "";
  settings.wifiPass   = "";
  settings.outdoor    = DEF_OUTDOOR;
  settings.noiseFloor = DEF_NOISE_FLOOR;
  settings.watchdog   = DEF_WATCHDOG;
  settings.spikeRej   = DEF_SPIKE_REJ;
  settings.minStrikes = DEF_MIN_STRIKES;
  settings.maskDist   = DEF_MASK_DIST;
  settings.autoNoise  = DEF_AUTO_NOISE;
  settings.tuneCap    = DEF_TUNE_CAP;
  settings.wifiPower  = DEF_WIFI_POWER;
  settings.wifiSleep  = DEF_WIFI_SLEEP;
  settings.hasLoc     = 0;
  settings.lat        = 0;
  settings.lon        = 0;
}

void clampSettings() {
  settings.outdoor    = settings.outdoor ? 1 : 0;
  if (settings.noiseFloor > 7)  settings.noiseFloor = 7;
  if (settings.watchdog   > 10) settings.watchdog   = 10;
  if (settings.spikeRej   > 11) settings.spikeRej   = 11;
  uint8_t m = settings.minStrikes;
  settings.minStrikes = m >= 16 ? 16 : m >= 9 ? 9 : m >= 5 ? 5 : 1;
  settings.maskDist   = settings.maskDist  ? 1 : 0;
  settings.autoNoise  = settings.autoNoise ? 1 : 0;
  if (settings.tuneCap < -1 || settings.tuneCap > 15) settings.tuneCap = -1;
  int8_t w = settings.wifiPower;     // snap to the offered steps
  settings.wifiPower = w < 0 ? -1 : w >= 19 ? 20 : w >= 16 ? 17 : w >= 13 ? 14 : w >= 10 ? 11 : 8;
  settings.wifiSleep = settings.wifiSleep ? 1 : 0;
  if (!(settings.lat >= -90 && settings.lat <= 90 &&
        settings.lon >= -180 && settings.lon <= 180)) {   // also catches NaN
    settings.hasLoc = 0; settings.lat = 0; settings.lon = 0;
  }
  settings.hasLoc = settings.hasLoc ? 1 : 0;
}

// ---- tiny extractors -------------------------------------------------
// Find "key": and return the raw value text up to , or } (no nesting).
static bool findValue(const String& src, const char* key, String& out) {
  String pat = String('"') + key + "\"";
  int k = src.indexOf(pat);
  if (k < 0) return false;
  int colon = src.indexOf(':', k + pat.length());
  if (colon < 0) return false;
  int i = colon + 1;
  while (i < (int)src.length() && (src[i] == ' ' || src[i] == '\t')) i++;
  if (i >= (int)src.length()) return false;

  if (src[i] == '"') {                 // string value
    int end = i + 1;
    String v;
    while (end < (int)src.length() && src[end] != '"') {
      if (src[end] == '\\' && end + 1 < (int)src.length()) end++; // skip esc
      v += src[end++];
    }
    out = v;
  } else {                              // number / literal
    int end = i;
    while (end < (int)src.length() &&
           src[end] != ',' && src[end] != '}' && src[end] != '\n') end++;
    out = src.substring(i, end);
    out.trim();
  }
  return true;
}

static String jsonEscape(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') o += '\\';
    o += c;
  }
  return o;
}

void loadSettings() {
  applyDefaults();
  if (!storageReady()) return;

  // A leftover temp file means a save was interrupted before the rename;
  // the real config is still the complete previous version.
  if (LittleFS.exists(CONFIG_TMP_PATH)) LittleFS.remove(CONFIG_TMP_PATH);

  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) return;

  String src;
  src.reserve(384);
  while (f.available()) src += (char)f.read();
  f.close();

  String v;
  if (findValue(src, "ssid", v))       settings.wifiSsid   = v;
  if (findValue(src, "pass", v))       settings.wifiPass   = v;
  if (findValue(src, "outdoor", v))    settings.outdoor    = (uint8_t)v.toInt();
  if (findValue(src, "noiseFloor", v)) settings.noiseFloor = (uint8_t)v.toInt();
  if (findValue(src, "watchdog", v))   settings.watchdog   = (uint8_t)v.toInt();
  if (findValue(src, "spikeRej", v))   settings.spikeRej   = (uint8_t)v.toInt();
  if (findValue(src, "minStrikes", v)) settings.minStrikes = (uint8_t)v.toInt();
  if (findValue(src, "maskDist", v))   settings.maskDist   = (uint8_t)v.toInt();
  if (findValue(src, "autoNoise", v))  settings.autoNoise  = (uint8_t)v.toInt();
  if (findValue(src, "tuneCap", v))    settings.tuneCap    = (int8_t)v.toInt();
  if (findValue(src, "wifiPower", v))  settings.wifiPower  = (int8_t)v.toInt();
  if (findValue(src, "wifiSleep", v))  settings.wifiSleep  = (uint8_t)v.toInt();
  if (findValue(src, "hasLoc", v))     settings.hasLoc     = (uint8_t)v.toInt();
  if (findValue(src, "lat", v))        settings.lat        = v.toFloat();
  if (findValue(src, "lon", v))        settings.lon        = v.toFloat();
  clampSettings();
}

bool saveSettings() {
  if (!storageReady()) return false;

  File f = LittleFS.open(CONFIG_TMP_PATH, "w");
  if (!f) return false;

  f.print(F("{\n"));
  f.print(F("  \"ssid\":\""));      f.print(jsonEscape(settings.wifiSsid)); f.print(F("\",\n"));
  f.print(F("  \"pass\":\""));      f.print(jsonEscape(settings.wifiPass)); f.print(F("\",\n"));
  f.print(F("  \"outdoor\":"));     f.print(settings.outdoor);              f.print(F(",\n"));
  f.print(F("  \"noiseFloor\":"));  f.print(settings.noiseFloor);           f.print(F(",\n"));
  f.print(F("  \"watchdog\":"));    f.print(settings.watchdog);             f.print(F(",\n"));
  f.print(F("  \"spikeRej\":"));    f.print(settings.spikeRej);             f.print(F(",\n"));
  f.print(F("  \"minStrikes\":"));  f.print(settings.minStrikes);           f.print(F(",\n"));
  f.print(F("  \"maskDist\":"));    f.print(settings.maskDist);             f.print(F(",\n"));
  f.print(F("  \"autoNoise\":"));   f.print(settings.autoNoise);            f.print(F(",\n"));
  f.print(F("  \"tuneCap\":"));     f.print(settings.tuneCap);              f.print(F(",\n"));
  f.print(F("  \"wifiPower\":"));   f.print(settings.wifiPower);            f.print(F(",\n"));
  f.print(F("  \"wifiSleep\":"));   f.print(settings.wifiSleep);            f.print(F(",\n"));
  f.print(F("  \"hasLoc\":"));      f.print(settings.hasLoc);               f.print(F(",\n"));
  f.print(F("  \"lat\":"));         f.print(settings.lat, 6);               f.print(F(",\n"));
  f.print(F("  \"lon\":"));         f.print(settings.lon, 6);               f.print(F("\n"));
  f.print(F("}\n"));
  bool ok = f.getWriteError() == 0;
  f.close();
  if (!ok) { LittleFS.remove(CONFIG_TMP_PATH); return false; }

  // LittleFS rename replaces the destination atomically.
  return LittleFS.rename(CONFIG_TMP_PATH, CONFIG_PATH);
}
