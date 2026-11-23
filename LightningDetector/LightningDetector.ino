/* =====================================================================
   Lightning Detector  —  Wemos D1 mini (ESP8266) + AS3935
   ---------------------------------------------------------------------
   Board : LOLIN(WEMOS) D1 R2 & mini, Flash Size "4MB (FS:2MB OTA:~1019KB)"
   Sensor: AS3935 Franklin lightning sensor over I2C (default) or SPI,
           IRQ on D5 (I2C) — see config.h and wiring_diagram.svg
   Store : onboard flash via LittleFS (settings + strike log)

   Setup flow:
     1. On first boot (no saved WiFi), the device raises an open SoftAP
        named AP_SSID and serves a setup page at AP_IP (captive portal).
     2. Pick your network, enter its password and choose a device
        password; the device saves them, reboots and joins your WiFi.
     3. Dashboard at http://lightning.local (or the device's IP): live
        status, a map with distance rings around your sensor location,
        a distance-over-time chart, the strike log and sensor settings.
        Viewing is open; changing anything needs the device password.

   The setup network only comes back on purpose: "Reset Wi-Fi" on the
   dashboard, or the reset button pressed twice within 5 seconds.

   The AS3935 reports the distance to the leading edge of a storm (5-40 km,
   or "overhead"), not a direction — hence rings around the sensor rather
   than points. The blue LED (D4) flashes on each strike, blinks slowly
   while the setup network is up and double-blinks while WiFi is down.
   ===================================================================== */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <StreamString.h>
#include <Updater.h>
#include <time.h>
#include <sys/time.h>

#include "config.h"
#include "settings.h"
#include "storage.h"
#include "auth.h"
#include "as3935.h"
#include "webpages.h"

ESP8266WebServer        server(80);
DNSServer               dnsServer;

bool   apMode          = false;
String scanResultsJson = "[]";   // cached network scan for the setup page

// Why the setup network is up (shown on the setup page) and when it came up.
static bool     s_setupForced = false;   // reset pressed twice
static bool     s_freshFailed = false;   // just-saved network would not join
static uint32_t s_apStartMs   = 0;
// STA mode: network services start on the first successful join.
static bool     s_netStarted  = false;

// Deferred reboot: handlers set this instead of calling ESP.restart()
// directly, so the HTTP response fully flushes before the device goes down.
uint32_t g_rebootAt = 0;

// ---- Time ------------------------------------------------------------
// The device keeps UTC; the dashboard shows strikes in the browser's own
// time zone, so there is no time zone setting to get wrong.
static void startNTP() { configTime(0, 0, "pool.ntp.org", "time.nist.gov"); }

bool timeIsValid() { return (unsigned long)time(nullptr) >= NTP_MIN_EPOCH; }

// =====================================================================
//  STRIKE BOOKKEEPING
// =====================================================================
// Ring of the most recent logged strikes, served inline by /api/state so
// an open dashboard picks up new strikes without re-reading flash.
static StrikeRow s_recent[RECENT_RING];
static uint32_t  s_seq = 0;        // strikes committed since boot
static uint32_t  s_gen = 1;        // bumps when the log is cleared

// Strikes that arrive before the clock is set (or while the log file is
// being streamed to a browser) wait here with their uptime stamp.
struct Pending { uint32_t ms; uint8_t d; uint32_t e; uint8_t f; };
static Pending  s_pending[PENDING_MAX];
static uint8_t  s_pendingN = 0;
static uint32_t s_pendingDropped = 0;
static bool     s_streaming = false;

// Sensor statistics since boot
static uint32_t s_disturbers = 0, s_noiseSecs = 0;
static uint32_t s_lastNoiseMs = 0;
static bool     s_noiseSeen = false;
static uint8_t  s_stormDist = 0;   // latest distance estimate from the chip
static uint32_t s_stormMs   = 0;
static bool     s_haveStorm = false;

// Per-minute counters for "last hour" figures. The minute number is a
// wrap-safe counter (millis() wraps every 49.7 days). Methods rather than
// free functions: the Arduino preprocessor emits prototypes for free
// functions above this struct, where the type is not yet declared.
static uint32_t s_minuteNo = 0, s_minuteStart = 0;

struct MinuteRing {
  uint16_t b[60];
  uint32_t cur;
  void advance() {
    if (s_minuteNo - cur >= 60) { memset(b, 0, sizeof(b)); cur = s_minuteNo; }
    while (cur != s_minuteNo) { cur++; b[cur % 60] = 0; }
  }
  void add() {
    advance();
    if (b[cur % 60] < 0xFFFF) b[cur % 60]++;
  }
  uint32_t sum() {
    advance();
    uint32_t s = 0;
    for (uint8_t i = 0; i < 60; i++) s += b[i];
    return s;
  }
};
static MinuteRing s_distRing   = { {0}, 0 };
static MinuteRing s_noiseRing  = { {0}, 0 };
static MinuteRing s_distTxRing = { {0}, 0 };   // disturbers right after a web request

// Runtime noise floor (auto noise floor moves it; settings.noiseFloor is
// the floor it never goes below).
static uint8_t  s_nfNow = DEF_NOISE_FLOOR;
static uint32_t s_nfChangedMs = 0;

// ---- Wi-Fi radio -----------------------------------------------------
// The radio is the loudest thing near the sensor: every transmit burst and
// every wake-up is a step in supply current. Keep it awake (steady current)
// and transmit no louder than the link needs.
static float    s_txDbm      = WIFI_MAX_DBM;
static bool     s_txLocked   = false;   // link dropped while trimmed: full power
static bool     s_radioDirty = true;    // settings changed: re-evaluate now
static float    s_rssiAvg    = 0;
static uint32_t s_lastReqMs  = 0;       // last web request (its reply is a transmit burst)
static bool     s_reqSeen    = false;

static void radioSetPower(float dbm) {
  if (dbm > WIFI_MAX_DBM) dbm = WIFI_MAX_DBM;
  if (dbm < WIFI_AUTO_MIN_DBM) dbm = WIFI_AUTO_MIN_DBM;
  if (fabsf(dbm - s_txDbm) < 0.25f) return;
  WiFi.setOutputPower(dbm);
  Serial.printf("[WiFi] transmit power %.1f dBm (RSSI %.0f)\n", dbm, s_rssiAvg);
  s_txDbm = dbm;
}

static void radioApplySleep() {
  WiFi.setSleepMode(settings.wifiSleep ? WIFI_MODEM_SLEEP : WIFI_NONE_SLEEP);
}

static void radioUpdate() {
  static uint32_t lastSample = 0, lastEval = 0, downSince = 0;
  uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (!downSince) downSince = now | 1;
    if (now - downSince >= WIFI_DOWN_FULL_MS && s_txDbm < WIFI_MAX_DBM) {
      Serial.println(F("[WiFi] link down while trimmed -> full power until settings change"));
      s_txLocked = true;
      radioSetPower(WIFI_MAX_DBM);
    }
    return;
  }
  downSince = 0;

  // Smoothed RSSI: single readings jump several dB beacon to beacon.
  if (now - lastSample >= 5000UL || s_rssiAvg == 0) {
    lastSample = now;
    int r = WiFi.RSSI();
    if (r < 0 && r > -100) s_rssiAvg = s_rssiAvg == 0 ? r : s_rssiAvg * 0.8f + r * 0.2f;
  }
  if (!s_radioDirty && now - lastEval < WIFI_AUTO_EVERY_MS) return;
  bool forced = s_radioDirty;
  s_radioDirty = false;
  lastEval = now;

  float target;
  if (s_txLocked)                  target = WIFI_MAX_DBM;
  else if (settings.wifiPower >= 0) target = settings.wifiPower >= 20 ? WIFI_MAX_DBM : settings.wifiPower;
  else {
    // Auto: drop 1 dB per dB of signal above the reference, within limits.
    float cut = s_rssiAvg + (float)(-WIFI_AUTO_REF_RSSI);
    if (s_rssiAvg == 0 || cut < 0) cut = 0;
    target = WIFI_MAX_DBM - cut;
  }
  // Raise at once; lower only for a step of 2 dB or more (no hunting).
  if (forced || target > s_txDbm || s_txDbm - target >= 2.0f) radioSetPower(target);
}

// ---- LED -------------------------------------------------------------
static uint32_t s_ledOffAt = 0;
static void ledFlash(uint32_t ms) {
  digitalWrite(PIN_LED, LED_ON);
  s_ledOffAt = millis() + ms;
  if (!s_ledOffAt) s_ledOffAt = 1;
}
static void ledUpdate() {
  uint32_t now = millis();
  if (apMode) {                               // setup mode: short blink each second
    digitalWrite(PIN_LED, (now % 1000) < 80 ? LED_ON : !LED_ON);
    return;
  }
  if (s_ledOffAt && (int32_t)(now - s_ledOffAt) >= 0) s_ledOffAt = 0;
  if (s_ledOffAt) { digitalWrite(PIN_LED, LED_ON); return; }   // strike flash
  if (WiFi.status() != WL_CONNECTED) {        // offline: two short blinks every 2 s
    uint32_t p = now % 2000;
    digitalWrite(PIN_LED, (p < 80 || (p >= 240 && p < 320)) ? LED_ON : !LED_ON);
    return;
  }
  digitalWrite(PIN_LED, !LED_ON);
}

static void commitStrike(const StrikeRow& r) {
  if (!logStrike(r)) Serial.println(F("[LOG] write failed (filesystem?)"));
  s_recent[s_seq % RECENT_RING] = r;
  s_seq++;
}

static void recordStrike(uint8_t d, uint32_t e, uint8_t flags) {
  ledFlash(LED_STRIKE_MS);
  if (timeIsValid() && !s_streaming && s_pendingN == 0) {
    commitStrike({ (uint32_t)time(nullptr), d, e, flags });
    return;
  }
  if (s_pendingN < PENDING_MAX) s_pending[s_pendingN++] = { millis(), d, e, flags };
  else s_pendingDropped++;
}

static void flushPending() {
  if (!s_pendingN || s_streaming || !timeIsValid()) return;
  uint32_t nowT = (uint32_t)time(nullptr), nowMs = millis();
  for (uint8_t i = 0; i < s_pendingN; i++) {
    uint32_t ageS = (nowMs - s_pending[i].ms) / 1000UL;
    commitStrike({ nowT - ageS, s_pending[i].d, s_pending[i].e, s_pending[i].f });
  }
  s_pendingN = 0;
}

// =====================================================================
//  SENSOR
// =====================================================================
static uint32_t s_sensorRetryMs = 0;

static void applySensorSettings() {
  if (!asPresent()) return;
  asSetOutdoor(settings.outdoor);
  asSetNoiseFloor(s_nfNow);
  asSetWatchdog(settings.watchdog);
  asSetSpikeRejection(settings.spikeRej);
  asSetMinStrikes(settings.minStrikes);
  asSetMaskDisturbers(settings.maskDist);
}

static void sensorStart() {
  s_sensorRetryMs = millis();
  if (!asBegin()) return;
  if (settings.tuneCap >= 0) {
    asSetTuneCap((uint8_t)settings.tuneCap);
    asCalibrate();                 // oscillators follow the antenna
  }
  s_nfNow = settings.noiseFloor;
  applySensorSettings();
  if (settings.tuneCap < 0) {
    Serial.println(F("[AS3935] antenna never tuned -> tuning now"));
    asTuneStart();
  }
}

static void handleEvent(const AsEvent& ev) {
  uint32_t now = millis();
  switch (ev.type) {
    case AS_EV_LIGHTNING:
      s_stormDist = ev.distKm; s_stormMs = now; s_haveStorm = true;
      recordStrike(ev.distKm, ev.energy, 0);
      Serial.printf("[STRIKE] distance %u%s energy %lu\n", ev.distKm,
                    ev.distKm == AS_DIST_OUT_OF_RANGE ? " (out of range)" :
                    ev.distKm == AS_DIST_OVERHEAD ? " (overhead)" : " km",
                    (unsigned long)ev.energy);
      break;
    case AS_EV_DISTURBER:
      s_disturbers++;
      s_distRing.add();
      // Right after a web request the radio was sending the reply. If most
      // disturbers land here, the detector's own Wi-Fi is the noise source.
      if (s_reqSeen && now - s_lastReqMs < WIFI_TX_WINDOW_MS) s_distTxRing.add();
      break;
    case AS_EV_NOISE:
      // INT_NH repeats for as long as the noise lasts; count it once a second.
      if (!s_noiseSeen || now - s_lastNoiseMs >= 1000) { s_noiseSecs++; s_noiseRing.add(); }
      s_noiseSeen   = true;
      s_lastNoiseMs = now;
      if (settings.autoNoise && s_nfNow < 7 && now - s_nfChangedMs >= AUTO_NF_RAISE_MS) {
        s_nfNow++;
        s_nfChangedMs = now;
        asSetNoiseFloor(s_nfNow);
        Serial.printf("[AS3935] noise too high -> noise floor %u\n", s_nfNow);
      }
      break;
    case AS_EV_PURGE:
      s_stormDist = ev.distKm; s_stormMs = now; s_haveStorm = true;
      break;
  }
}

static void serviceSensor() {
  AsEvent ev;
  if (asPoll(ev)) handleEvent(ev);
}

// Runs while a long response streams (loop() is blocked meanwhile) so a
// strike during a log download is still caught. Strikes are queued, not
// written, because the log file is open for reading.
static void serviceDuringStream() {
  serviceSensor();
  yield();
}

static void sensorHousekeeping() {
  uint32_t now = millis();
  if (now - s_minuteStart >= 60000UL) { s_minuteStart += 60000UL; s_minuteNo++; }

  if (!asPresent()) {
    // Retry every 10 s so a fixed wire is picked up without a reboot.
    if (now - s_sensorRetryMs >= 10000UL) sensorStart();
    return;
  }
  if (asTuneState().busy) return;

  // Auto noise floor steps back down after a quiet half hour.
  if (settings.autoNoise && s_nfNow > settings.noiseFloor &&
      now - s_lastNoiseMs >= AUTO_NF_QUIET_MS && now - s_nfChangedMs >= AUTO_NF_QUIET_MS) {
    s_nfNow--;
    s_nfChangedMs = now;
    asSetNoiseFloor(s_nfNow);
    Serial.printf("[AS3935] quiet -> noise floor %u\n", s_nfNow);
  }

  // Health check: a brown-out resets the chip to defaults (and a loose
  // wire reads garbage). If our settings are gone, start it over.
  static uint32_t lastCheck = 0;
  if (now - lastCheck >= HEALTH_CHECK_MS) {
    lastCheck = now;
    uint8_t m = settings.minStrikes;
    uint8_t mCode = m >= 16 ? 3 : m >= 9 ? 2 : m >= 5 ? 1 : 0;
    uint8_t want1 = (uint8_t)((s_nfNow << 4) | settings.watchdog);
    uint8_t want2 = (uint8_t)(0x80 | 0x40 | (mCode << 4) | settings.spikeRej);
    uint8_t r1 = asReadReg(0x01), r2 = asReadReg(0x02);
    if (r1 != want1 || r2 != want2) {
      Serial.printf("[AS3935] registers lost (01=%02X want %02X, 02=%02X want %02X) -> re-init\n",
                    r1, want1, r2, want2);
      sensorStart();
    }
  }
}

static void onTuneDone() {
  const AsTune& t = asTuneState();
  if (t.valid) {
    settings.tuneCap = t.best;
    saveSettings();
  }
}

// =====================================================================
//  WEB ROUTES — shared
// =====================================================================
// Appends s as a JSON string (quotes included). Control characters are
// dropped: nothing here needs them and they would break the page's JSON.
static void jsonStr(String& out, const String& s) {
  out += '"';
  for (size_t k = 0; k < s.length(); k++) {
    char c = s[k];
    if (c == '"' || c == '\\') out += '\\';
    if ((uint8_t)c >= 0x20) out += c;
  }
  out += '"';
}

static void sendErr(int code, const char* err) {
  String j = "{\"ok\":false,\"err\":";
  jsonStr(j, err);
  j += '}';
  server.send(code, "application/json", j);
}

// ---- Request guard ----------------------------------------------------
// Host check: a web page elsewhere can point its own domain at this
// device's address (DNS rebinding) and then read its replies. Such
// requests still carry that domain in Host, so only names that mean this
// device are accepted.
static bool hostOk() {
#if HOST_CHECK
  String h = server.hostHeader();
  if (h.length() == 0) return true;          // HTTP/1.0 tools; browsers always send Host
  int colon = h.indexOf(':');
  if (colon >= 0) h = h.substring(0, colon);
  h.toLowerCase();
  if (h.endsWith(".")) h.remove(h.length() - 1);
  if (h == WiFi.localIP().toString()) return true;
  // First label: the hostname, or what mDNS made of it after a name
  // clash on the network ("lightning-2").
  int dot = h.indexOf('.');
  String label = dot < 0 ? h : h.substring(0, dot);
  if (!label.startsWith(MDNS_HOSTNAME)) return false;
  if (dot < 0) return true;
  String suffix = h.substring(dot + 1);
  static const char* const kSuffixes[] = HOST_SUFFIXES;
  for (const char* s : kSuffixes) if (suffix == s) return true;
  return false;
#else
  return true;
#endif
}

// 0 when the request may go ahead, else the HTTP status to refuse it with.
//  X-LD   : sent by this device's own pages. Another site cannot add a
//           custom header to a request here without the browser first
//           asking permission (CORS), which this server never grants.
//  X-Auth : the sign-in token from /api/login.
static int accessCode(uint8_t access) {
  if (!hostOk()) return 421;
  if (access >= ACC_XHR && !server.hasHeader("X-LD")) return 403;
  if (access >= ACC_OWNER) {
    if (!authHasPassword()) return 428;
    if (!authTokenOk(server.header("X-Auth"))) return 401;
  }
  return 0;
}

static bool guard(uint8_t access) {
  int code = accessCode(access);
  switch (code) {
    case 0:   return true;
    case 421: server.send(421, "text/plain",
                "Open this detector by its own address: http://" MDNS_HOSTNAME ".local or its IP.");
              break;
    case 403: sendErr(403, "requests must come from the detector's own page"); break;
    case 428: sendErr(428, "set a device password first"); break;
    default:  sendErr(401, "locked: sign in with the device password"); break;
  }
  return false;
}

static void route(const char* uri, HTTPMethod method, void (*fn)(), uint8_t access) {
  server.on(uri, method, [fn, access]() { if (guard(access)) fn(); });
}

static void collectHeaders() {
  server.collectHeaders(F("X-Auth"), F("X-LD"));
}

static void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache");   // a stale page looks like a frozen device
  if (apMode) server.send_P(200, "text/html", SETUP_PAGE);
  else        server.send_P(200, "text/html", DASH_PAGE);
}

// ---- AP / provisioning routes ---------------------------------------
static void buildScanJson(int n) {
  String j = "[";
  for (int i = 0; i < n; i++) {
    if (i) j += ',';
    j += "{\"ssid\":";
    jsonStr(j, WiFi.SSID(i));
    j += ",\"rssi\":" + String(WiFi.RSSI(i)) +
         ",\"lock\":" + String(WiFi.encryptionType(i) == ENC_TYPE_NONE ? 0 : 1) +
         "}";
  }
  j += "]";
  scanResultsJson = j;
}

static void handleScan() {
  // Never scan synchronously here: a blocking scan channel-hops the radio
  // and DROPS connected AP clients — the phone loading this very page.
  int st = WiFi.scanComplete();
  if (st >= 0) { buildScanJson(st); WiFi.scanDelete(); }
  server.send(200, "application/json", scanResultsJson);
}

static void handleRescan() {
  WiFi.scanNetworks(true /*async*/);
  server.send(200, "application/json", "{\"ok\":true}");
}

// What the setup page needs to know: is a device password set already,
// which network is saved, and did it just fail to join.
static void handleInfo() {
  String j = "{\"hasPass\":";
  j += authHasPassword() ? '1' : '0';
  j += ",\"ssid\":";
  jsonStr(j, settings.wifiSsid);
  j += ",\"failed\":";
  j += s_freshFailed ? '1' : '0';
  j += ",\"forced\":";
  j += s_setupForced ? '1' : '0';
  j += ",\"fw\":\"" FW_VERSION "\"}";
  server.send(200, "application/json", j);
}

static void handleSave() {
  if (!server.hasHeader("X-LD")) { sendErr(403, "requests must come from the setup page"); return; }
  String ssid = server.arg("ssid"), pass = server.arg("pass"), dev = server.arg("devpass");
  if (ssid.length() == 0 || ssid.length() > 32) {
    sendErr(400, "the network name must be 1 to 32 characters"); return;
  }
  // Same network, password left blank: keep the saved password.
  if (pass.length() == 0 && ssid == settings.wifiSsid) pass = settings.wifiPass;
  if (pass.length() > 64) { sendErr(400, "Wi-Fi passwords are at most 64 characters"); return; }
  if (dev.length()) {
    if (dev.length() < PASS_MIN_LEN || dev.length() > PASS_MAX_LEN) {
      sendErr(400, "the device password must be 8 to 64 characters"); return;
    }
  } else if (!authHasPassword()) {
    sendErr(400, "choose a device password (8 or more characters)"); return;
  }
  // Setup is the recovery path for a forgotten device password, so it may
  // replace one without the old: reaching it takes the reset button or a
  // signed-in "Reset Wi-Fi".
  if (dev.length()) authSetPassword(dev);
  settings.wifiSsid  = ssid;
  settings.wifiPass  = pass;
  settings.wifiFresh = 1;                 // first join decides: back to setup if it fails
  bool ok = saveSettings();
  if (!ok) { sendErr(500, "save failed (filesystem)"); return; }
  server.send(200, "application/json", "{\"ok\":true}");
  g_rebootAt = millis() + 1500;           // reboot from loop(), post-flush
}

// ---- STA / dashboard routes -----------------------------------------
static int32_t ageSecs(bool have, uint32_t atMs) {
  return have ? (int32_t)((millis() - atMs) / 1000UL) : -1;
}

static void handleState() {
  // ?since=<seq> returns the strikes committed after it. A page whose seq
  // fell out of the ring (or predates a reboot) is told to reload the log.
  uint32_t since = s_seq;
  if (server.hasArg("since")) since = strtoul(server.arg("since").c_str(), nullptr, 10);
  uint32_t oldest = s_seq > RECENT_RING ? s_seq - RECENT_RING : 0;
  bool reload = server.hasArg("since") && (since > s_seq || since < oldest);
  if (reload) since = s_seq;

  const AsTune& tune = asTuneState();
  // Fixed buffer, no String churn: this is polled every few seconds for
  // months. Header ~720 bytes + 32 rows x 26 = ~1550 worst case.
  static char buf[1800];
  int off = snprintf(buf, sizeof(buf),
    "{\"fw\":\"" FW_VERSION "\",\"up\":%lu,\"time\":%lu,\"timeValid\":%d,\"fs\":%d,"
    "\"sensor\":%d,\"bus\":\"%s\",\"defaults\":%d,\"calib\":%d,"
    "\"tuning\":%d,\"tuneStep\":%u,\"cap\":%d,\"nf\":%u,\"nfMin\":%u,\"autoNoise\":%u,"
    "\"storm\":%u,\"stormAge\":%ld,"
    "\"disturbers\":%lu,\"disturbersHr\":%lu,\"distTxHr\":%lu,"
    "\"noiseSecs\":%lu,\"noiseHr\":%lu,\"noiseAge\":%ld,"
    "\"pending\":%u,\"dropped\":%lu,\"gen\":%lu,\"reload\":%d,"
    "\"rssi\":%d,\"txDbm\":%.1f,\"txLocked\":%d,\"wifiPower\":%d,\"wifiSleep\":%u,"
    "\"hasPass\":%d,\"auth\":%d,\"heap\":%u,\"new\":[",
    (unsigned long)(millis() / 1000UL), (unsigned long)time(nullptr),
    timeIsValid() ? 1 : 0, storageReady() ? 1 : 0,
    asPresent() ? 1 : 0, asBusName(), asDefaultsOk() ? 1 : 0, asCalibOk() ? 1 : 0,
    tune.busy ? 1 : 0, tune.step, settings.tuneCap, s_nfNow, settings.noiseFloor,
    settings.autoNoise,
    s_stormDist, (long)ageSecs(s_haveStorm, s_stormMs),
    (unsigned long)s_disturbers, (unsigned long)s_distRing.sum(),
    (unsigned long)s_distTxRing.sum(),
    (unsigned long)s_noiseSecs, (unsigned long)s_noiseRing.sum(),
    (long)ageSecs(s_noiseSeen, s_lastNoiseMs),
    s_pendingN, (unsigned long)s_pendingDropped, (unsigned long)s_gen,
    reload ? 1 : 0,
    WiFi.RSSI(), (double)s_txDbm, s_txLocked ? 1 : 0, settings.wifiPower, settings.wifiSleep,
    authHasPassword() ? 1 : 0, authTokenOk(server.header("X-Auth")) ? 1 : 0,
    ESP.getFreeHeap());
  // "seq" is the next strike the page should ask for. It follows the rows
  // actually sent, so if the buffer ever filled up the rest come next poll.
  uint32_t q = since;
  for (; q < s_seq && off < (int)sizeof(buf) - 60; q++) {
    const StrikeRow& r = s_recent[q % RECENT_RING];
    off += snprintf(buf + off, sizeof(buf) - off, "%s[%lu,%u,%lu,%u]",
                    q > since ? "," : "", (unsigned long)r.t, r.d,
                    (unsigned long)r.e, r.f);
  }
  snprintf(buf + off, sizeof(buf) - off, "],\"seq\":%lu}", (unsigned long)q);
  server.send(200, "application/json", buf);
}

// Chunked JSON writer for /api/strikes (methods for the same prototype
// reason as MinuteRing).
struct JsonStream {
  char     buf[512];
  int      used;
  uint32_t since;
  bool     first;
  void flush() {
    if (used) server.sendContent(buf, used);
    used = 0;
  }
  static bool visit(const StrikeRow& r, void* ctx) {
    JsonStream& c = *(JsonStream*)ctx;
    if (r.t < c.since) return true;
    if (c.used > (int)sizeof(c.buf) - 40) c.flush();
    c.used += snprintf(c.buf + c.used, sizeof(c.buf) - c.used, "%s[%lu,%u,%lu,%u]",
                       c.first ? "" : ",", (unsigned long)r.t, r.d,
                       (unsigned long)r.e, r.f);
    c.first = false;
    return true;
  }
};

static void handleStrikes() {
  // The whole log as [[epoch, distance, energy, flags], ...], oldest first.
  // ?since=<epoch> limits it. Streamed in chunks; nothing big is buffered.
  static JsonStream c;
  c.used = 0; c.first = true;
  c.since = server.hasArg("since") ? strtoul(server.arg("since").c_str(), nullptr, 10) : 0;
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  c.buf[c.used++] = '[';
  s_streaming = true;
  forEachStrike(JsonStream::visit, &c, serviceDuringStream);
  s_streaming = false;
  if (c.used > (int)sizeof(c.buf) - 2) c.flush();
  c.buf[c.used++] = ']';
  c.flush();
  server.sendContent("");
}

static void handleConfigGet() {
  FSInfo fsi;
  memset(&fsi, 0, sizeof(fsi));
  if (storageReady()) LittleFS.info(fsi);
  String j = "{";
  j += "\"outdoor\":"     + String(settings.outdoor) + ",";
  j += "\"noiseFloor\":"  + String(settings.noiseFloor) + ",";
  j += "\"watchdog\":"    + String(settings.watchdog) + ",";
  j += "\"spikeRej\":"    + String(settings.spikeRej) + ",";
  j += "\"minStrikes\":"  + String(settings.minStrikes) + ",";
  j += "\"maskDist\":"    + String(settings.maskDist) + ",";
  j += "\"autoNoise\":"   + String(settings.autoNoise) + ",";
  j += "\"tuneCap\":"     + String(settings.tuneCap) + ",";
  j += "\"wifiPower\":"   + String(settings.wifiPower) + ",";
  j += "\"wifiSleep\":"   + String(settings.wifiSleep) + ",";
  j += "\"hasLoc\":"      + String(settings.hasLoc) + ",";
  j += "\"lat\":"         + String(settings.lat, 6) + ",";
  j += "\"lon\":"         + String(settings.lon, 6) + ",";
  j += "\"ip\":\""        + WiFi.localIP().toString() + "\",";
  j += "\"host\":\"" MDNS_HOSTNAME "\",";
  j += "\"logBytes\":"    + String((unsigned long)strikeLogBytes()) + ",";
  j += "\"fsUsed\":"      + String((unsigned long)fsi.usedBytes) + ",";
  j += "\"fsTotal\":"     + String((unsigned long)fsi.totalBytes);
  j += "}";
  server.send(200, "application/json", j);
}

static bool argU8(const char* name, uint8_t& out) {
  if (!server.hasArg(name)) return false;
  long v = server.arg(name).toInt();
  out = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
  return true;
}

static void handleConfigSet() {
  const Settings before = settings;      // flash is only written if something changed
  // Location: validate both halves before changing anything.
  if (server.hasArg("lat") || server.hasArg("lon")) {
    float la = server.arg("lat").toFloat(), lo = server.arg("lon").toFloat();
    if (!server.hasArg("lat") || !server.hasArg("lon") ||
        !(la >= -90 && la <= 90 && lo >= -180 && lo <= 180)) {
      server.send(400, "application/json", "{\"ok\":false,\"err\":\"bad location\"}");
      return;
    }
    settings.lat = la; settings.lon = lo; settings.hasLoc = 1;
  }
  if (server.hasArg("clearLoc")) { settings.hasLoc = 0; settings.lat = 0; settings.lon = 0; }

  bool sensorChanged = false;
  uint8_t v;
  if (argU8("outdoor", v))    { settings.outdoor    = v; sensorChanged = true; }
  if (argU8("noiseFloor", v)) { settings.noiseFloor = v; sensorChanged = true; }
  if (argU8("watchdog", v))   { settings.watchdog   = v; sensorChanged = true; }
  if (argU8("spikeRej", v))   { settings.spikeRej   = v; sensorChanged = true; }
  if (argU8("minStrikes", v)) { settings.minStrikes = v; sensorChanged = true; }
  if (argU8("maskDist", v))   { settings.maskDist   = v; sensorChanged = true; }
  if (argU8("autoNoise", v))  { settings.autoNoise  = v; sensorChanged = true; }
  int8_t oldCap = settings.tuneCap;
  if (server.hasArg("tuneCap")) settings.tuneCap = (int8_t)server.arg("tuneCap").toInt();
  bool radioChanged = false;
  if (server.hasArg("wifiPower")) { settings.wifiPower = (int8_t)server.arg("wifiPower").toInt(); radioChanged = true; }
  if (argU8("wifiSleep", v))      { settings.wifiSleep = v; radioChanged = true; }
  clampSettings();

  if (radioChanged) {
    s_txLocked   = false;              // a new choice gets a fresh try
    s_radioDirty = true;               // radioUpdate() applies the power
    radioApplySleep();
  }

  if (asPresent() && !asTuneState().busy) {
    if (sensorChanged) {
      s_nfNow = settings.noiseFloor;     // restart auto noise floor from the new minimum
      s_nfChangedMs = millis();
      applySensorSettings();
    }
    if (settings.tuneCap != oldCap && settings.tuneCap >= 0) {
      asSetTuneCap((uint8_t)settings.tuneCap);
      asCalibrate();
    }
  }
  bool ok = settingsEqual(before, settings) || saveSettings();
  server.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false,\"err\":\"save failed\"}");
}

static void handleTuneGet() {
  const AsTune& t = asTuneState();
  char buf[360];
  int off = snprintf(buf, sizeof(buf),
    "{\"busy\":%d,\"step\":%u,\"valid\":%d,\"best\":%d,\"cap\":%d,"
    "\"target\":%lu,\"tol\":%.1f,\"err\":\"%s\",\"hz\":[",
    t.busy ? 1 : 0, t.step, t.valid ? 1 : 0, t.best, settings.tuneCap,
    (unsigned long)TUNE_TARGET_HZ, (double)TUNE_TOL_PCT, t.err);
  for (uint8_t i = 0; i < 16; i++)
    off += snprintf(buf + off, sizeof(buf) - off, "%s%lu", i ? "," : "", (unsigned long)t.hz[i]);
  snprintf(buf + off, sizeof(buf) - off, "]}");
  server.send(200, "application/json", buf);
}

static void handleTuneStart() {
  if (!asPresent()) {
    server.send(503, "application/json", "{\"ok\":false,\"err\":\"sensor not found\"}");
    return;
  }
  asTuneStart();
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleSimulate() {
  // A test strike for checking the map and log without waiting for a storm.
  // Logged with the test flag; the dashboard labels it and can hide it.
  // Rate-limited: each one is a flash write, and a flood would push real
  // strikes out of the log.
  static uint32_t lastMs = 0;
  static bool     any    = false;
  if (any && millis() - lastMs < SIMULATE_MIN_MS) { sendErr(429, "one test strike a second, please"); return; }
  lastMs = millis(); any = true;
  static const uint8_t kDist[] = { 1, 5, 6, 8, 10, 12, 14, 17, 20, 24, 27, 31, 34, 37, 40 };
  uint8_t d = kDist[random(0, sizeof(kDist))];
  uint32_t e = (uint32_t)random(2000, 400000);
  recordStrike(d, e, STRIKE_FLAG_TEST);
  char b[64];
  snprintf(b, sizeof(b), "{\"ok\":true,\"d\":%u,\"e\":%lu}", d, (unsigned long)e);
  server.send(200, "application/json", b);
}

static void handleClearLog() {
  clearStrikeLog();
  s_pendingN = 0;
  s_gen++;
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleTimeSet() {
  // Fallback clock from the browser for networks without internet access.
  // NTP, when it answers, overrides it.
  uint32_t t = server.hasArg("t") ? strtoul(server.arg("t").c_str(), nullptr, 10) : 0;
  bool set = false;
  if (!timeIsValid() && t >= NTP_MIN_EPOCH) {
    struct timeval tv = { (time_t)t, 0 };
    settimeofday(&tv, nullptr);
    set = true;
    Serial.println(F("[TIME] clock set from browser"));
  }
  server.send(200, "application/json", set ? "{\"ok\":true,\"set\":1}" : "{\"ok\":true,\"set\":0}");
}

static void handleRegs() {
  static const uint8_t regs[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x3A, 0x3B };
  char buf[320];
  int off = snprintf(buf, sizeof(buf), "{\"sensor\":%d,\"bus\":\"%s\",\"defaults\":%d,\"calib\":%d,\"regs\":{",
                     asPresent() ? 1 : 0, asBusName(), asDefaultsOk() ? 1 : 0, asCalibOk() ? 1 : 0);
  for (uint8_t i = 0; i < sizeof(regs); i++)
    off += snprintf(buf + off, sizeof(buf) - off, "%s\"0x%02X\":\"0x%02X\"",
                    i ? "," : "", regs[i], asReadReg(regs[i]));
  snprintf(buf + off, sizeof(buf) - off, "}}");
  server.send(200, "application/json", buf);
}

static void handleReset() {
  // Clears saved WiFi -> next boot raises the setup network. The device
  // password stays.
  settings.wifiSsid  = "";
  settings.wifiPass  = "";
  settings.wifiFresh = 0;
  saveSettings();
  server.send(200, "application/json", "{\"ok\":true}");
  g_rebootAt = millis() + 800;
}

// ---- Sign-in ----------------------------------------------------------
// Wrong passwords slow the next try down (1 s, then 30 s after 5 in a
// row), so guessing a decent password over the network is hopeless.
static uint8_t  s_loginFails  = 0;
static uint32_t s_loginWaitMs = 0, s_loginFailAt = 0;

static bool loginThrottled() {
  if (!s_loginFails || millis() - s_loginFailAt >= s_loginWaitMs) return false;
  uint32_t left = (s_loginWaitMs - (millis() - s_loginFailAt) + 999) / 1000;
  char b[96];
  snprintf(b, sizeof(b), "{\"ok\":false,\"err\":\"too many wrong tries: wait %lu s\",\"wait\":%lu}",
           (unsigned long)left, (unsigned long)left);
  server.sendHeader("Retry-After", String(left));
  server.send(429, "application/json", b);
  return true;
}

static void loginFailed() {
  if (s_loginFails < 255) s_loginFails++;
  s_loginFailAt = millis();
  s_loginWaitMs = s_loginFails >= LOGIN_LOCK_AFTER ? LOGIN_LOCK_MS : LOGIN_DELAY_MS;
  Serial.printf("[AUTH] wrong password (%u in a row)\n", s_loginFails);
}

static void sendToken() {
  String j = "{\"ok\":true,\"token\":";
  jsonStr(j, authToken());
  j += '}';
  server.send(200, "application/json", j);
}

static void handleLogin() {
  if (!authHasPassword()) { sendErr(428, "set a device password first"); return; }
  if (loginThrottled()) return;
  if (!authCheckPassword(server.arg("pass"))) { loginFailed(); sendErr(401, "wrong password"); return; }
  s_loginFails = 0;
  sendToken();
}

// Set the first password (no password yet), or change it (needs the
// current one). A new password makes a new token: every other browser is
// signed out.
static void handlePassword() {
  String nw = server.arg("new");
  if (nw.length() < PASS_MIN_LEN || nw.length() > PASS_MAX_LEN) {
    sendErr(400, "the password must be 8 to 64 characters"); return;
  }
  if (authHasPassword()) {
    if (loginThrottled()) return;
    if (!authCheckPassword(server.arg("old"))) { loginFailed(); sendErr(401, "the current password is wrong"); return; }
    s_loginFails = 0;
  }
  authSetPassword(nw);
  if (!saveSettings()) { sendErr(500, "save failed (filesystem)"); return; }
  Serial.println(F("[AUTH] device password set"));
  sendToken();
}

// ---- Firmware update ------------------------------------------------
// Replaces the stock ESP8266HTTPUpdateServer so the upload needs the same
// sign-in token as every other change. The check happens when the upload
// starts; a refused upload is read and thrown away, never written.
static bool   s_updAllowed = false, s_updStarted = false;
static String s_updErr;

static String updaterError() {
  StreamString s;
  Update.printError(s);
  s.trim();
  return s.length() ? String(s.c_str()) : String("update failed");
}

static void handleUpdateUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    s_updStarted = true;
    s_updErr     = "";
    s_updAllowed = accessCode(ACC_OWNER) == 0;
    if (!s_updAllowed) { Serial.println(F("[OTA] refused: not signed in")); return; }
    Serial.printf("[OTA] receiving %s\n", up.filename.c_str());
    WiFiUDP::stopAll();
    uint32_t space = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(space, U_FLASH)) s_updErr = updaterError();
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (s_updAllowed && !s_updErr.length() &&
        Update.write(up.buf, up.currentSize) != up.currentSize) s_updErr = updaterError();
  } else if (up.status == UPLOAD_FILE_END) {
    if (s_updAllowed && !s_updErr.length() && !Update.end(true)) s_updErr = updaterError();
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (s_updAllowed) Update.end();
    s_updErr = "upload aborted";
  }
  yield();
}

static void handleUpdateDone() {
  bool started = s_updStarted, allowed = s_updAllowed;
  s_updStarted = s_updAllowed = false;
  if (!guard(ACC_OWNER)) return;
  if (!started || !allowed) { sendErr(400, "no firmware file received"); return; }
  if (s_updErr.length()) {
    Serial.printf("[OTA] failed: %s\n", s_updErr.c_str());
    sendErr(500, s_updErr.c_str());
    return;
  }
  Serial.println(F("[OTA] update written -> rebooting"));
  server.send(200, "application/json", "{\"ok\":true}");
  g_rebootAt = millis() + 1000;
}

// Old bookmarks: the update page is now part of the dashboard.
static void handleOldUpdate() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

static void handleNotFound() {
  if (apMode) {
    // Captive-portal style: bounce every unknown URL to the setup page.
    server.sendHeader("Location", String("http://") + AP_IP.toString(), true);
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain", "not found");
  }
}

// =====================================================================
//  MODE STARTUP
// =====================================================================
static void startAPMode() {
  apMode = true;
  s_apStartMs = millis();
  // AP_STA, not AP: the STA interface must be up for WiFi.scanNetworks().
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();          // idle STA: stops failed-join retries from
                              // channel-hopping the SoftAP around
  buildScanJson(WiFi.scanNetworks());   // blocking scan BEFORE the AP is up:
                                        // no clients yet to disrupt

  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(300);

  dnsServer.start(53, "*", AP_IP);      // every hostname resolves to us

  // No host check here: phones probe captive-portal URLs on other names,
  // and this network has no internet for a rebinding attack to come from.
  server.on("/",       handleRoot);
  server.on("/scan",   handleScan);
  server.on("/rescan", handleRescan);
  server.on("/info",   handleInfo);
  server.on("/save",   HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);
  collectHeaders();
  server.begin();

  Serial.printf("[AP] setup network \"%s\" at http://%s (%s)\n", AP_SSID, AP_IP.toString().c_str(),
                s_setupForced ? "reset pressed twice" : s_freshFailed ? "saved network would not join"
                                                                      : "no network saved");
}

// Only when a network is saved (setup entered with reset-twice, or a
// just-saved network that would not join): after a grace period, retry it
// while nobody is on the setup page (a join moves the radio to the
// router's channel, which would drop a phone on the AP), and reboot into
// normal mode once it joins.
static void apRetrySavedNetwork() {
  static uint32_t lastTry = 0, tryStart = 0;
  static bool trying = false;
  if (settings.wifiSsid.length() == 0 || g_rebootAt) return;
  if (s_setupForced && millis() - s_apStartMs < AP_SETUP_GRACE_MS) return;
  uint8_t clients = WiFi.softAPgetStationNum();
  if (trying) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(F("[AP] saved network joined -> rebooting into normal mode"));
      g_rebootAt = millis() + 500;
    } else if (clients > 0 || millis() - tryStart > AP_RETRY_JOIN_MS) {
      WiFi.disconnect();
      trying  = false;
      lastTry = millis();
    }
    return;
  }
  if (clients == 0 && millis() - lastTry > AP_RETRY_EVERY_MS) {
    Serial.println(F("[AP] retrying saved network"));
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPass.c_str());
    trying   = true;
    tryStart = millis();
  }
}

// mustJoin: the network was just saved on the setup page. Give the first
// join WIFI_CONNECT_TIMEOUT_MS and report failure (back to setup). After
// that, the detector never leaves normal mode on its own: a power cut or
// a router reboot just means retrying until the network is back.
static bool startSTAMode(bool mustJoin) {
  apMode = false;
  WiFi.mode(WIFI_STA);
  WiFi.hostname(MDNS_HOSTNAME);
  WiFi.setAutoReconnect(true);
  // Join at full power; radioUpdate() trims it once the link is up.
  WiFi.setOutputPower(WIFI_MAX_DBM);
  s_txDbm = WIFI_MAX_DBM;
  radioApplySleep();
#ifdef USE_STATIC_IP
  WiFi.config(STATIC_IP, STATIC_GATEWAY, STATIC_SUBNET, STATIC_DNS);
#endif
  WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPass.c_str());
  Serial.printf("[WiFi] joining \"%s\"\n", settings.wifiSsid.c_str());

  if (mustJoin) {
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
      delay(250);
      serviceSensor();              // first-boot tuning / early strikes
      if (asTuneUpdate()) onTuneDone();
      doubleResetDisarm();
    }
    if (WiFi.status() != WL_CONNECTED) return false;
    settings.wifiFresh = 0;         // proven: from now on, never back to setup by itself
    saveSettings();
  }

  startNTP();                       // SNTP starts working once the link is up

  route("/",             HTTP_GET,  handleRoot,       ACC_OPEN);
  route("/api/state",    HTTP_GET,  handleState,      ACC_OPEN);
  route("/api/strikes",  HTTP_GET,  handleStrikes,    ACC_OPEN);
  route("/api/config",   HTTP_GET,  handleConfigGet,  ACC_OPEN);
  route("/api/tune",     HTTP_GET,  handleTuneGet,    ACC_OPEN);
  route("/api/regs",     HTTP_GET,  handleRegs,       ACC_OPEN);
  route("/api/time",     HTTP_POST, handleTimeSet,    ACC_XHR);
  route("/api/login",    HTTP_POST, handleLogin,      ACC_XHR);
  route("/api/password", HTTP_POST, handlePassword,   ACC_XHR);
  route("/api/config",   HTTP_POST, handleConfigSet,  ACC_OWNER);
  route("/api/tune",     HTTP_POST, handleTuneStart,  ACC_OWNER);
  route("/api/simulate", HTTP_POST, handleSimulate,   ACC_OWNER);
  route("/api/clearlog", HTTP_POST, handleClearLog,   ACC_OWNER);
  route("/api/reset",    HTTP_POST, handleReset,      ACC_OWNER);
  route("/update",       HTTP_GET,  handleOldUpdate,  ACC_OPEN);
  server.on("/api/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.onNotFound(handleNotFound);
  collectHeaders();
  // Timestamp every request: its reply is a transmit burst, which is what
  // the "near Wi-Fi traffic" disturber count is measured against.
  // (No HTTP keep-alive: this server handles one client at a time, and an
  // idle kept-alive connection would stall other requests for seconds.)
  server.addHook([](const String&, const String&, WiFiClient*, ESP8266WebServer::ContentTypeFunction) {
    s_lastReqMs = millis();
    s_reqSeen   = true;
    return ESP8266WebServer::CLIENT_REQUEST_CAN_CONTINUE;
  });
  server.begin();
  return true;
}

// STA mode, every loop: start the network services on the first join, and
// start a fresh join attempt if the link has been down for a while (the
// SDK's own reconnect gives up in some states, e.g. after a wrong-password
// reply from a router that was mid-reboot).
static void staWatch() {
  static uint32_t downSince = 0, lastBegin = 0;
  static bool     wasUp     = false;
  uint32_t now = millis();
  bool up = WiFi.status() == WL_CONNECTED;

  if (up && !wasUp) {
    Serial.printf("[WiFi] connected: http://%s  (http://" MDNS_HOSTNAME ".local)\n",
                  WiFi.localIP().toString().c_str());
    if (!s_netStarted) {
      s_netStarted = true;
      // mDNS restarts itself on later reconnects once begun.
      if (MDNS.begin(MDNS_HOSTNAME)) MDNS.addService("http", "tcp", 80);
#ifdef OTA_PASSWORD
      ArduinoOTA.setHostname(MDNS_HOSTNAME);
      ArduinoOTA.setPassword(OTA_PASSWORD);
      ArduinoOTA.begin();
#endif
    }
  } else if (!up && wasUp) {
    Serial.println(F("[WiFi] link lost - retrying"));
  }
  wasUp = up;

  if (up) { downSince = 0; return; }
  if (!downSince) { downSince = now | 1; lastBegin = now; return; }
  if (now - downSince >= STA_REJOIN_MS && now - lastBegin >= STA_REJOIN_MS) {
    Serial.println(F("[WiFi] still offline -> new join attempt"));
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPass.c_str());
    lastBegin = now;
  }
}

// Reset pressed twice within SETUP_DOUBLE_RESET_MS -> setup network. The
// first press leaves a mark in RTC memory (which survives a reset but not
// a power cut); the mark is wiped once the window has passed.
static bool     s_drdArmed = false;
static const uint32_t DRD_MARK = 0x4C445253UL;   // "LDRS"

static bool doubleResetCheck() {
  uint32_t mark = 0;
  ESP.rtcUserMemoryRead(SETUP_RTC_BLOCK, &mark, sizeof(mark));
  bool byButton = ESP.getResetInfoPtr()->reason == REASON_EXT_SYS_RST;
  bool twice    = byButton && mark == DRD_MARK;
  mark = (byButton && !twice) ? DRD_MARK : 0;
  ESP.rtcUserMemoryWrite(SETUP_RTC_BLOCK, &mark, sizeof(mark));
  s_drdArmed = mark == DRD_MARK;
  return twice;
}

static void doubleResetDisarm() {
  if (!s_drdArmed || millis() < SETUP_DOUBLE_RESET_MS) return;
  uint32_t mark = 0;
  ESP.rtcUserMemoryWrite(SETUP_RTC_BLOCK, &mark, sizeof(mark));
  s_drdArmed = false;
}

// =====================================================================
//  SETUP / LOOP
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println(F("\n[LD] Lightning Detector v" FW_VERSION " booting"));

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, !LED_ON);
  randomSeed(ESP.random());
  s_setupForced = doubleResetCheck();

  WiFi.persistent(false);   // creds live in LittleFS; stop the SDK
                            // rewriting them to flash on every connect

  storageBegin();
  loadSettings();
  sensorStart();
  if (!asPresent()) {       // three quick blinks: sensor not found
    for (uint8_t i = 0; i < 3; i++) {
      digitalWrite(PIN_LED, LED_ON);  delay(80);
      digitalWrite(PIN_LED, !LED_ON); delay(120);
    }
  }
  s_minuteStart = millis();

  if (s_setupForced) {
    Serial.println(F("[LD] reset pressed twice -> setup network"));
    startAPMode();
  } else if (settings.wifiSsid.length() == 0) {
    Serial.println(F("[LD] no WiFi saved -> setup network"));
    startAPMode();
  } else if (!startSTAMode(settings.wifiFresh)) {
    Serial.println(F("[LD] the network saved on the setup page would not join -> setup network"));
    s_freshFailed = true;
    startAPMode();
  }
}

void loop() {
  if (apMode) {
    dnsServer.processNextRequest();
    apRetrySavedNetwork();
  } else {
    staWatch();
    if (s_netStarted) MDNS.update();
#ifdef OTA_PASSWORD
    if (s_netStarted) ArduinoOTA.handle();
#endif
    radioUpdate();
  }
  server.handleClient();

  serviceSensor();
  if (asTuneUpdate()) onTuneDone();
  sensorHousekeeping();
  flushPending();
  ledUpdate();
  doubleResetDisarm();

  if (g_rebootAt && (int32_t)(millis() - g_rebootAt) >= 0) ESP.restart();
  yield();
}
