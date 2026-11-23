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

   The reader walks the object member by member, so a value can never be
   mistaken for a key (a network name may contain any characters). The file
   is written to a temp name and renamed over the old one, so a power cut
   mid-save leaves the previous settings intact.
   ===================================================================== */
#include "settings.h"
#include "config.h"
#include "storage.h"

Settings settings;

#define CONFIG_TMP_PATH "/config.tmp"

static void applyDefaults() {
  settings.wifiSsid   = "";
  settings.wifiPass   = "";
  settings.wifiFresh  = 0;
  settings.passSalt   = "";
  settings.passHash   = "";
  settings.authToken  = "";
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
  settings.wifiFresh  = settings.wifiFresh ? 1 : 0;
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
  // A half-written password record is no password at all.
  if (settings.passHash.length() != 64 || settings.passSalt.length() != 32) {
    settings.passHash = ""; settings.passSalt = ""; settings.authToken = "";
  }
}

bool settingsEqual(const Settings& a, const Settings& b) {
  return a.wifiSsid == b.wifiSsid && a.wifiPass == b.wifiPass && a.wifiFresh == b.wifiFresh &&
         a.passSalt == b.passSalt && a.passHash == b.passHash && a.authToken == b.authToken &&
         a.outdoor == b.outdoor && a.noiseFloor == b.noiseFloor && a.watchdog == b.watchdog &&
         a.spikeRej == b.spikeRej && a.minStrikes == b.minStrikes && a.maskDist == b.maskDist &&
         a.autoNoise == b.autoNoise && a.tuneCap == b.tuneCap &&
         a.wifiPower == b.wifiPower && a.wifiSleep == b.wifiSleep &&
         a.hasLoc == b.hasLoc && a.lat == b.lat && a.lon == b.lon;
}

// ---- tiny JSON reader -------------------------------------------------
static int skipSpace(const String& s, int i) {
  while (i < (int)s.length() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) i++;
  return i;
}

// s[i] is the opening quote. Decodes into out; returns the index after the
// closing quote, or -1 if the string never ends.
static int readString(const String& s, int i, String& out) {
  out = "";
  for (i++; i < (int)s.length(); i++) {
    char c = s[i];
    if (c == '"') return i + 1;
    if (c == '\\' && i + 1 < (int)s.length()) {
      char e = s[++i];
      if      (e == 'n') out += '\n';
      else if (e == 'r') out += '\r';
      else if (e == 't') out += '\t';
      else if (e == 'u' && i + 4 < (int)s.length()) {
        out += (char)strtol(s.substring(i + 1, i + 5).c_str(), nullptr, 16);
        i += 4;
      }
      else out += e;                       // \" and \\ (and anything else)
    } else {
      out += c;
    }
  }
  return -1;
}

static void applyMember(const String& k, const String& v) {
  if      (k == "ssid")       settings.wifiSsid   = v;
  else if (k == "pass")       settings.wifiPass   = v;
  else if (k == "wifiFresh")  settings.wifiFresh  = (uint8_t)v.toInt();
  else if (k == "passSalt")   settings.passSalt   = v;
  else if (k == "passHash")   settings.passHash   = v;
  else if (k == "authToken")  settings.authToken  = v;
  else if (k == "outdoor")    settings.outdoor    = (uint8_t)v.toInt();
  else if (k == "noiseFloor") settings.noiseFloor = (uint8_t)v.toInt();
  else if (k == "watchdog")   settings.watchdog   = (uint8_t)v.toInt();
  else if (k == "spikeRej")   settings.spikeRej   = (uint8_t)v.toInt();
  else if (k == "minStrikes") settings.minStrikes = (uint8_t)v.toInt();
  else if (k == "maskDist")   settings.maskDist   = (uint8_t)v.toInt();
  else if (k == "autoNoise")  settings.autoNoise  = (uint8_t)v.toInt();
  else if (k == "tuneCap")    settings.tuneCap    = (int8_t)v.toInt();
  else if (k == "wifiPower")  settings.wifiPower  = (int8_t)v.toInt();
  else if (k == "wifiSleep")  settings.wifiSleep  = (uint8_t)v.toInt();
  else if (k == "hasLoc")     settings.hasLoc     = (uint8_t)v.toInt();
  else if (k == "lat")        settings.lat        = v.toFloat();
  else if (k == "lon")        settings.lon        = v.toFloat();
}

// Walks {"key":value, ...}. Stops quietly at anything malformed; whatever
// was read up to that point is kept.
static void parseObject(const String& s) {
  int i = skipSpace(s, 0);
  if (i >= (int)s.length() || s[i] != '{') return;
  i++;
  String key, val;
  for (;;) {
    i = skipSpace(s, i);
    if (i < (int)s.length() && s[i] == ',') { i++; continue; }
    if (i >= (int)s.length() || s[i] != '"') return;     // '}' or garbage
    i = readString(s, i, key);
    if (i < 0) return;
    i = skipSpace(s, i);
    if (i >= (int)s.length() || s[i] != ':') return;
    i = skipSpace(s, i + 1);
    if (i >= (int)s.length()) return;
    if (s[i] == '"') {
      i = readString(s, i, val);
      if (i < 0) return;
    } else {
      int end = i;
      while (end < (int)s.length() && s[end] != ',' && s[end] != '}' && s[end] != '\n') end++;
      val = s.substring(i, end);
      val.trim();
      i = end;
    }
    applyMember(key, val);
  }
}

// Escapes for a JSON string: quote, backslash and control characters.
static void printJsonString(File& f, const String& s) {
  f.print('"');
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { f.print('\\'); f.print(c); }
    else if ((uint8_t)c < 0x20) f.printf("\\u%04x", (unsigned)(uint8_t)c);
    else f.print(c);
  }
  f.print('"');
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
  src.reserve(f.size() + 1);
  while (f.available()) src += (char)f.read();
  f.close();

  parseObject(src);
  clampSettings();
}

bool saveSettings() {
  if (!storageReady()) return false;

  File f = LittleFS.open(CONFIG_TMP_PATH, "w");
  if (!f) return false;

  f.print(F("{\n"));
  f.print(F("  \"ssid\":"));        printJsonString(f, settings.wifiSsid);  f.print(F(",\n"));
  f.print(F("  \"pass\":"));        printJsonString(f, settings.wifiPass);  f.print(F(",\n"));
  f.print(F("  \"wifiFresh\":"));   f.print(settings.wifiFresh);            f.print(F(",\n"));
  f.print(F("  \"passSalt\":"));    printJsonString(f, settings.passSalt);  f.print(F(",\n"));
  f.print(F("  \"passHash\":"));    printJsonString(f, settings.passHash);  f.print(F(",\n"));
  f.print(F("  \"authToken\":"));   printJsonString(f, settings.authToken); f.print(F(",\n"));
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
