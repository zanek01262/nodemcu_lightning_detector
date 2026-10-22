/* =====================================================================
   storage.cpp  —  LittleFS storage (settings + strike log) on flash
   ---------------------------------------------------------------------
   Notes:
     * LittleFS.begin() on the ESP8266 auto-formats an unformatted
       partition by default, so first boot "just works".
     * Even a busy storm logs a strike every few seconds at most, so
       flash wear is a non-issue (LittleFS wear-levels anyway).
     * Rotation: when /strikes.csv exceeds STRIKE_LOG_MAX_BYTES it is
       renamed to /strikes.old.csv (replacing any previous one) and a
       fresh file starts. Worst case on disk: 2x the cap.
   ===================================================================== */
#include "storage.h"
#include "config.h"

static bool g_ready = false;

void storageBegin() {
  g_ready = LittleFS.begin();
  if (g_ready) {
    FSInfo info;
    LittleFS.info(info);
    Serial.printf("[FS] mounted: %u / %u bytes used\n",
                  (unsigned)info.usedBytes, (unsigned)info.totalBytes);
  } else {
    Serial.println(F("[FS] mount failed - check Tools > Flash Size has FS space"));
  }
}

bool storageReady() { return g_ready; }

bool logStrike(const StrikeRow& r) {
  if (!g_ready) return false;
  File f = LittleFS.open(STRIKE_LOG_PATH, "a");
  if (!f) return false;
  f.printf("%lu,%u,%lu,%u\n", (unsigned long)r.t, (unsigned)r.d,
           (unsigned long)r.e, (unsigned)r.f);
  size_t sz = f.size();
  f.close();

  if (sz > STRIKE_LOG_MAX_BYTES) {
    LittleFS.remove(STRIKE_LOG_OLD_PATH);          // ok if absent
    LittleFS.rename(STRIKE_LOG_PATH, STRIKE_LOG_OLD_PATH);
    Serial.println(F("[FS] strike log rotated"));
  }
  return true;
}

void clearStrikeLog() {
  if (!g_ready) return;
  LittleFS.remove(STRIKE_LOG_PATH);
  LittleFS.remove(STRIKE_LOG_OLD_PATH);
}

static size_t fileSize(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f) return 0;
  size_t s = f.size();
  f.close();
  return s;
}

size_t strikeLogBytes() {
  if (!g_ready) return 0;
  return fileSize(STRIKE_LOG_OLD_PATH) + fileSize(STRIKE_LOG_PATH);
}

// "t,d,e,f" -> row. Rejects anything malformed (e.g. a line cut short by
// a power loss mid-append).
static bool parseRow(const char* s, StrikeRow& r) {
  char* p;
  r.t = strtoul(s, &p, 10);  if (*p != ',') return false;
  r.d = (uint8_t)strtoul(p + 1, &p, 10); if (*p != ',') return false;
  r.e = strtoul(p + 1, &p, 10); if (*p != ',') return false;
  r.f = (uint8_t)strtoul(p + 1, &p, 10);
  return r.t >= NTP_MIN_EPOCH && r.d > 0 && r.d <= 63;
}

void forEachStrike(StrikeVisitor fn, void* ctx, void (*idle)()) {
  if (!g_ready) return;
  const char* parts[2] = { STRIKE_LOG_OLD_PATH, STRIKE_LOG_PATH };
  char line[48];
  uint8_t ll = 0;
  uint8_t buf[256];
  for (int p = 0; p < 2; p++) {
    File f = LittleFS.open(parts[p], "r");
    if (!f) continue;
    ll = 0;
    // Block reads instead of readStringUntil(): no String churn, and a
    // full log parses in a fraction of the time.
    while (f.available()) {
      int n = f.read(buf, sizeof(buf));
      if (n <= 0) break;
      for (int i = 0; i < n; i++) {
        char c = (char)buf[i];
        if (c == '\n') {
          line[ll] = 0;
          StrikeRow r;
          if (ll > 0 && parseRow(line, r) && !fn(r, ctx)) { f.close(); return; }
          ll = 0;
        } else if (c != '\r' && ll < sizeof(line) - 1) {
          line[ll++] = c;
        }
      }
      if (idle) idle();
    }
    f.close();
  }
}
