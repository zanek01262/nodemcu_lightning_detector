/* =====================================================================
   config.h  —  Pins, sensor interface, network constants, defaults
   =====================================================================

   BOARD: Wemos / LOLIN D1 mini (ESP8266)
   IDE  : Tools -> Board -> "LOLIN(WEMOS) D1 R2 & mini"
          Tools -> Flash Size -> "4MB (FS:2MB OTA:~1019KB)"
          With FS:none, settings and the strike log cannot be saved.

   WIRING — I2C (default; CJMCU-3935, DFRobot Gravity and similar)
   ---------------------------------------------------------------
       AS3935 VCC   -> 3V3          AS3935 GND -> G
       AS3935 SCL   -> D1 (GPIO5)   I2C clock
       AS3935 MOSI  -> D2 (GPIO4)   I2C data (labelled SDA on some boards)
       AS3935 IRQ   -> D5 (GPIO14)  interrupt (also carries the antenna
                                    frequency during auto-tune)
       AS3935 SI    -> 3V3          selects I2C (GND would select SPI)
       AS3935 A0,A1 -> 3V3          I2C address 0x03
       EN_V (if the board has it) -> 3V3   (AS3935 internal regulator on)
       CS           -> G            unused in I2C mode, but must not float
       MISO         -> not connected

   WIRING — SPI (uncomment AS3935_USE_SPI below; SparkFun v2 is SPI only)
   ----------------------------------------------------------------------
       VCC -> 3V3, GND -> G
       SCK  -> D5 (GPIO14)   MISO -> D6 (GPIO12)   MOSI -> D7 (GPIO13)
       CS   -> D2 (GPIO4)    IRQ  -> D1 (GPIO5)
       SI   -> GND on boards that break it out (SparkFun sets it for you)

   PINS TO AVOID for IRQ: D3/GPIO0 and D4/GPIO2 must be HIGH at boot and
   D8/GPIO15 LOW. The AS3935 drives IRQ low when idle, which would stop
   the board booting on D3/D4. D0/GPIO16 has no interrupt support.

   PLACEMENT: the AS3935 hears 500 kHz radio noise. Keep it 15 cm or more
   from the D1 mini, its USB supply, monitors, LED drivers and chargers.
   See HANDOFF.md and wiring_diagram.svg.
   ===================================================================== */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <IPAddress.h>

// ---------- Firmware version (semver) ----------
// MAJOR: breaking hardware/storage changes; MINOR: features; PATCH: fixes
//   1.0.0 first release: AS3935 (I2C/SPI), setup AP, map + log dashboard
//   1.1.0 less self-interference: radio always awake, auto TX power, 5 s
//         dashboard polling, 80 kHz I2C, 5 min health check, "near Wi-Fi"
//         disturber counter
//   1.2.0 security: device password for settings and firmware updates,
//         host-name check, setup network only on purpose (reset twice),
//         IDE network upload off unless OTA_PASSWORD is set, rate limits
#define FW_VERSION "1.2.0"

// ---------- SoftAP provisioning ----------
#define AP_SSID  "LightningDetector-Setup"
// "" = open setup network. Set 8+ characters here before flashing to
// encrypt it: your Wi-Fi and device passwords then cross the air encrypted
// during setup. The setup page tells you nothing about this password, so
// write it down.
#define AP_PASS  ""
static const IPAddress AP_IP(192, 168, 4, 1);

// The setup network only comes up on purpose: on first boot (nothing
// saved), after "Reset Wi-Fi" on the dashboard, or when the reset button
// is pressed twice within SETUP_DOUBLE_RESET_MS. A power cut never raises
// it: the detector just keeps retrying your Wi-Fi.
#define SETUP_DOUBLE_RESET_MS    5000UL
#define SETUP_RTC_BLOCK          8      // RTC user memory slot (blocks 64+ belong to the OTA boot loader)

// ---------- WiFi station ----------
// Right after setup, the first join gets this long; if it fails (usually a
// mistyped password) the setup network comes back with an error message.
#define WIFI_CONNECT_TIMEOUT_MS  20000UL
// Offline for this long: start a fresh join attempt.
#define STA_REJOIN_MS            60000UL
// Setup entered with reset-twice while a network is saved: after this long
// with nobody on the setup network, it retries the saved network and goes
// back to normal mode if it joins.
#define AP_SETUP_GRACE_MS        (5UL * 60UL * 1000UL)
#define AP_RETRY_EVERY_MS        60000UL
#define AP_RETRY_JOIN_MS         20000UL

// ---------- Security ----------
#define PASS_MIN_LEN       8
#define PASS_MAX_LEN       64
#define AUTH_HASH_ROUNDS   2000     // iterated SHA-256 (~0.1 s per check)
// Wrong password: the next try waits LOGIN_DELAY_MS, and after
// LOGIN_LOCK_AFTER wrong tries in a row, LOGIN_LOCK_MS.
#define LOGIN_DELAY_MS     1000UL
#define LOGIN_LOCK_AFTER   5
#define LOGIN_LOCK_MS      30000UL
#define SIMULATE_MIN_MS    1000UL   // at most one test strike a second
// Host-name check (blocks DNS rebinding): requests must name this device —
// its IP address, or MDNS_HOSTNAME alone or followed by one of these
// local-network suffixes (routers register DHCP names under them). Set
// HOST_CHECK to 0 if you reach the detector under some other name.
#define HOST_CHECK         1
#define HOST_SUFFIXES      { "local", "lan", "home", "home.arpa", "localdomain", "internal", "fritz.box" }

// ---- Optional static IP (uncomment + fill to enable) ----
// #define USE_STATIC_IP
#ifdef USE_STATIC_IP
  static const IPAddress STATIC_IP     (192, 168, 1, 60);
  static const IPAddress STATIC_GATEWAY(192, 168, 1, 1);
  static const IPAddress STATIC_SUBNET (255, 255, 255, 0);
  static const IPAddress STATIC_DNS    (192, 168, 1, 1);
#endif

// ---------- AS3935 interface ----------
// #define AS3935_USE_SPI           // uncomment for SPI wiring (see above)

#ifdef AS3935_USE_SPI
  #define PIN_AS_CS        4        // D2
  #define PIN_AS_IRQ       5        // D1
  // SCK/MISO/MOSI are the hardware SPI pins D5/D6/D7.
  // 2 MHz, well clear of the 500 kHz band the antenna listens on.
  #define AS3935_SPI_HZ    2000000
#else
  #define PIN_AS_SDA       4        // D2
  #define PIN_AS_SCL       5        // D1
  #define PIN_AS_IRQ       14       // D5
  #define AS3935_I2C_ADDR  0x03     // A0 + A1 tied to 3V3
  // Not 100 kHz: a square-wave clock's 5th harmonic would sit at exactly
  // 500 kHz, the frequency the antenna is tuned to. At 80 kHz the 5th and
  // 7th harmonics (400/560 kHz) fall outside the ~33 kHz receive band, and
  // the slower edges suit long sensor wires better too.
  #define AS3935_I2C_HZ    80000
  // If nothing answers at AS3935_I2C_ADDR, 0x01 and 0x02 are tried too
  // (boards whose A0/A1 are left floating or tied differently).
#endif

// ---------- Wi-Fi radio (self-interference) ----------
// The AS3935 hears the ESP8266's supply current, which jumps whenever the
// radio transmits or wakes. Defaults: radio always awake (no on/off steps
// at every beacon, ~10x a second) and transmit power trimmed to what the
// link needs.
#define DEF_WIFI_POWER     -1     // -1 = auto, else dBm: 8, 11, 14, 17 or 20 (= 20.5 max)
#define DEF_WIFI_SLEEP     0      // 0 = always awake, 1 = modem sleep (SDK default)
#define WIFI_MAX_DBM       20.5f
#define WIFI_AUTO_MIN_DBM  8.0f
// Auto power: cut 1 dB for every dB the received signal beats -65 dBm,
// up to the minimum above. Re-evaluated every minute from a smoothed RSSI.
#define WIFI_AUTO_REF_RSSI -65
#define WIFI_AUTO_EVERY_MS 60000UL
// If the link drops while power is trimmed, go back to full power and
// stay there until the setting is changed.
#define WIFI_DOWN_FULL_MS  10000UL
// A disturber this soon after a web request counts as "near Wi-Fi traffic".
#define WIFI_TX_WINDOW_MS  500

// ---------- Status LED ----------
#define PIN_LED          2          // D4, the D1 mini's blue LED
#define LED_ON           LOW        // active low
#define LED_STRIKE_MS    250        // flash length on a lightning strike

// ---------- Identity / OTA ----------
#define MDNS_HOSTNAME "lightning"   // http://lightning.local
// Firmware updates normally go through the dashboard (Device panel), which
// needs the device password. Uploading from the Arduino IDE over the
// network is OFF unless you set a password for it here:
// #define OTA_PASSWORD "change-me"

// ---------- Storage paths (LittleFS) ----------
#define CONFIG_PATH          "/config.json"
#define STRIKE_LOG_PATH      "/strikes.csv"
#define STRIKE_LOG_OLD_PATH  "/strikes.old.csv"
// Rows are ~24 bytes: rotate at 64 KB (~2700 strikes), so the log holds
// ~5400 strikes across both files and the dashboard loads it in a second.
#define STRIKE_LOG_MAX_BYTES 65536UL

// ---------- Time ----------
#define NTP_MIN_EPOCH   1700000000UL   // reject garbage time (before ~Nov 2023)
// Strikes seen before the clock is set are held in RAM and written with
// their real time once NTP (or the browser) sets it.
#define PENDING_MAX     16

// ---------- Recent-strike ring (served inline by /api/state) ----------
// The page loads the whole log once, then picks up new strikes from this
// ring on each poll instead of re-reading flash during a storm.
#define RECENT_RING     32             // power of 2

// ---------- AS3935 defaults ----------
#define DEF_OUTDOOR      0     // 0 = indoor AFE gain, 1 = outdoor
#define DEF_NOISE_FLOOR  2     // NF_LEV 0..7 (chip default 2)
#define DEF_WATCHDOG     2     // WDTH 0..15 (chip default 2)
#define DEF_SPIKE_REJ    2     // SREJ 0..15 (chip default 2)
#define DEF_MIN_STRIKES  1     // 1, 5, 9 or 16 flashes before the first report
#define DEF_MASK_DIST    0     // 1 = sensor stops reporting disturbers
#define DEF_AUTO_NOISE   1     // 1 = raise the noise floor on "noise too high"
#define DEF_TUNE_CAP     -1    // -1 = untuned: auto-tune on first boot

// Register health check (catches a brown-out reset of the chip). Each
// check is I2C traffic next to the antenna, so it runs rarely.
#define HEALTH_CHECK_MS    (5UL * 60UL * 1000UL)

// Auto noise floor: each "noise too high" interrupt raises the floor one
// step (at most every AUTO_NF_RAISE_MS); after AUTO_NF_QUIET_MS without
// one it steps back down, never below the saved setting. Runtime only,
// so it never writes flash.
#define AUTO_NF_RAISE_MS   5000UL
#define AUTO_NF_QUIET_MS   (30UL * 60UL * 1000UL)

// ---------- Antenna tuning ----------
// The LC antenna must resonate at 500 kHz +/- 3.5%. During tuning the
// chip outputs the antenna frequency / 128 on IRQ (~3.9 kHz), which an
// interrupt counts for TUNE_WINDOW_MS at each of the 16 capacitor values.
#define TUNE_TARGET_HZ    500000UL
#define TUNE_TOL_PCT      3.5f
#define TUNE_DIV          128
#define TUNE_SETTLE_MS    30
#define TUNE_WINDOW_MS    500

#endif // CONFIG_H
