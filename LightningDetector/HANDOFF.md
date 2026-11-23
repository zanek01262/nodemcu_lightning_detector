# Lightning Detector — Handoff (v1.2.0)

**Purpose of this doc:** everything a new person (or a new AI session) needs to
pick this project up cold. The root `README.md` is the short build guide.

---

## 1. What this device is

A Wemos D1 mini (ESP8266) reading an AS3935 lightning sensor. Each strike is
logged to flash with its time, the chip's distance estimate and its energy
figure. A web dashboard shows a live status banner, a map with distance rings
around the sensor's location, a distance-over-time chart, the strike log (CSV
download) and the sensor settings, including antenna auto-tuning.

The AS3935 reports **distance only** (to the leading edge of the storm, in
steps: overhead, 5, 6, 8, 10, 12, 14, 17, 20, 24, 27, 31, 34, 37, 40 km, or
out of range). There is no direction, so the map draws rings, not points.

---

## 2. Hardware

| Part | Connection | Notes |
|---|---|---|
| **Wemos / LOLIN D1 mini** | — | Board select: **LOLIN(WEMOS) D1 R2 & mini** |
| **AS3935 breakout** (CJMCU-3935 etc.), I2C | VCC→3V3, GND→G, SCL→D1, **MOSI**→D2, IRQ→D5 | The I2C data line is the **MOSI** pad (datasheet pin 13 "I2CD/MOSI") |
| AS3935 config straps | SI, A0, A1 (and EN_V if present)→3V3, CS→GND, MISO n/c | SI high = I2C; A0+A1 high = address 0x03 |
| On-board LED (D4/GPIO2) | — | Flashes on strikes, blinks once a second in setup mode, double-blinks every 2 s while Wi-Fi is down |

SPI is supported for SPI-only boards (SparkFun v2): uncomment
`AS3935_USE_SPI` in `config.h` — SCK D5, MISO D6, MOSI D7, CS D2, IRQ D1.

**IRQ must not go on D3, D4 or D8.** They are boot-strap pins; the AS3935
drives IRQ low when idle, which stops the D1 mini booting from D3/D4. D0 has
no interrupt support.

**Placement matters more than anything else.** The sensor listens at 500 kHz
and hears switching noise: keep it 15–20 cm from the D1 mini and its USB
supply, and away from monitors, LED drivers, chargers, and inverters. A
ferrite on the USB cable helps. Lots of "disturbers" or a high noise floor on
the dashboard means move it.

See `wiring_diagram.svg`. The root `README.md` has the full hardware guide:
per-board notes, SPI wiring, I2C addresses, power, placement and noise, LED
patterns and a hardware troubleshooting table.

---

## 3. Build & flash

- **Board:** LOLIN(WEMOS) D1 R2 & mini, ESP8266 community core (3.1.2 tested)
- **Flash Size:** **4MB (FS:2MB OTA:~1019KB)**. With FS:none, settings and the
  log silently fail (serial warns, the dashboard's Device panel shows it).
- **Libraries:** none beyond the ESP8266 core (the AS3935 driver is in-tree).
- **Upload:** USB, or the dashboard's Device panel → **Update firmware**
  (Sketch → Export Compiled Binary, pick the .bin; needs the device
  password; the page reloads itself when the detector is back). IDE network
  upload (port `lightning`) only exists if `OTA_PASSWORD` is set in
  `config.h`.
- `/` is sent with `Cache-Control: no-cache`, so a reload picks up a new UI.
  Devices still on v1.1.0 or older send no such header: hard-refresh
  (Ctrl+Shift+R) after updating them.

---

## 4. Setup flow

1. No saved WiFi → open SoftAP **`LightningDetector-Setup`**, captive portal
   at **http://192.168.4.1** (LED blinks once a second).
2. Pick the network (or type a hidden one), enter its password and choose a
   **device password** (8–64 chars) → saved to `/config.json` with
   `wifiFresh=1`, reboot.
3. The first join after setup gets 20 s. Success clears `wifiFresh`; failure
   (usually a typo) brings the setup network back with an error banner.
4. Dashboard at **http://lightning.local** or the device's IP.
5. **Place sensor** on the map (click, or search an address / `lat, lon`,
   drag the pin, Save). Rings appear around it.
6. First boot with a sensor attached auto-tunes the antenna (~9 s) and saves
   the capacitor value.
7. "Reset Wi-Fi" on the dashboard (signed in) forgets the network and
   returns to setup mode. The device password is kept.

**The setup network never comes up by itself** once a network has joined:
after a power cut or router reboot the detector stays in normal mode and
keeps retrying (the SDK's auto-reconnect, plus a fresh `WiFi.begin` every
60 s while offline). To enter setup deliberately (new router, forgotten
device password), **press reset twice within 5 s**: the first boot leaves a
mark in RTC user memory (block 8), the second sees it. In that mode the
saved network is retried after 5 minutes with nobody on the setup page, and
the setup page may replace the device password without the old one
(physical access is the recovery path).

**iPhone gotcha:** on the setup AP, turn **Cellular Data off**, or iOS routes
192.168.4.1 over LTE.

---

## 5. Firmware layout

```
LightningDetector.ino  orchestration, HTTP routes, WiFi modes, NTP, strike bookkeeping
config.h               pins, interface switch, defaults, paths, FW_VERSION
as3935.h/.cpp          register-level AS3935 driver: I2C/SPI, IRQ, tuning, calibration
settings.h/.cpp        persisted settings, small JSON reader/writer, atomic save (tmp + rename)
auth.h/.cpp            device password hash (salted, iterated SHA-256), sign-in token
storage.h/.cpp         LittleFS mount, strike log append/rotate/iterate
webpages.h             setup page + dashboard as PROGMEM raw strings
wiring_diagram.svg     wiring
```

**Design rules that matter:**
- `/api/state` is polled every 5 s for months: `snprintf` into a static buffer,
  never `String` concat.
- Nothing blocks `loop()` for long. The 2 ms wait the datasheet asks for
  before reading the interrupt register is done by `asPoll()` across loop
  passes, and the antenna sweep is a state machine.
- Times are UTC on the device. The browser shows local time, so there is no
  time zone setting.
- The setup page has no external resources (captive clients have no
  internet). The dashboard loads fonts and Leaflet *after* render, so it still
  works on a LAN without internet (no map).

---

## 6. How detection works

```
AS3935 raises IRQ (D5) ── ISR sets a flag (and counts edges for tuning)
loop: asPoll() sees flag or IRQ high -> waits >= 2 ms -> reads REG0x03
  INT_L  (0x08) lightning  -> read 0x04..0x07 in one burst: energy (21 bit) + distance
  INT_D  (0x04) disturber  -> counted (per-minute ring for "last hour")
  INT_NH (0x01) noise high -> counted once/second; auto noise floor raises NF_LEV
  0x00          purge      -> storm distance estimate changed (no new strike)
```

- Reading REG0x03 drops IRQ. INT_NH stays asserted while the noise lasts, so
  after one the driver looks again in 250 ms instead of hammering the bus.
- **Auto noise floor** (default on): each INT_NH raises NF_LEV one step (at
  most every 5 s). After 30 quiet minutes it steps back down, never below the
  saved value. Runtime only, no flash writes.
- **Health check** every 5 min: REG0x01/0x02 are compared with what was
  written. A brown-out resets the chip to defaults; on mismatch the driver
  re-initialises. A missing sensor is retried every 10 s, so a wiring fix is
  picked up without a reboot.
- **Clock not set yet** (no NTP): strikes wait in a 16-entry RAM queue with
  their uptime and are written with the right time once the clock is set. The
  dashboard pushes the browser's time if NTP never answers (LAN with no
  internet).
- **Antenna tuning:** LCO_FDIV=128 and DISP_LCO put the antenna frequency/128
  (~3.9 kHz) on IRQ; an interrupt counts edges for 500 ms at each of the 16
  TUN_CAP values. The closest to 500 kHz wins (spec: ±3.5%), then the RC
  oscillators are recalibrated (they are calibrated against the antenna).
  The RCO calibration puts the 1.1 MHz SRCO on IRQ for 2 ms, so the ISR is
  detached around it.

---

## 6a. Self-interference (Wi-Fi) — v1.1.0

The ESP8266 is the noisiest thing near the sensor. What the firmware does
about it:

| Measure | Why |
|---|---|
| Radio **always awake** (`WIFI_NONE_SLEEP`, setting "Radio") | Modem sleep powers the radio down and up at every beacon (~10×/s); each step in supply current is an impulse the sensor can hear. Always awake costs ~50 mA of steady draw. |
| **Auto transmit power** (setting "Wi-Fi power") | Cuts 1 dB per dB the smoothed RSSI beats −65 dBm, down to 8 dBm, re-evaluated each minute (raise at once, lower in ≥2 dB steps). Joins at full power. If the link drops for 10 s while trimmed it goes back to full power and stays there until the setting is changed. |
| Dashboard polls every **5 s** (30 s when the tab is hidden) | Each reply is a transmit burst. Nobody watching = almost no traffic. |
| I2C at **80 kHz**, not 100 kHz | A 100 kHz square clock's 5th harmonic is exactly 500 kHz. |
| Health check every **5 min** | Each check is bus traffic next to the antenna. |
| "**near Wi-Fi**" counter on the Disturbers tile | Disturbers within 500 ms of a web request (stamped by a request hook). With the page polling every 5 s, ~10% land there by chance; if most do, Wi-Fi is the source. |

HTTP keep-alive was considered and **not** used: the server handles one
client at a time, and an idle kept-alive connection blocks other requests for
up to 5 s. The handshake packets go out in the same few milliseconds as the
reply anyway.

---

## 6b. Security — v1.2.0

Threat model: the device is on a home LAN, never port-forwarded. Attackers
considered: other devices or guests on the LAN, someone in radio range, and
web pages open in a LAN user's browser (CSRF, DNS rebinding).

| Measure | Where | What it stops |
|---|---|---|
| **Device password**: salted SHA-256 iterated 2000x (~0.1 s); only salt + hash stored | `auth.cpp` | Anyone on the LAN changing settings or flashing firmware |
| **Sign-in token** (128-bit, hardware RNG) from `/api/login`, sent as `X-Auth`; a new password makes a new token | `auth.cpp`, `guard()` | The password crossing the network on every request; a password change signs everyone else out |
| **`X-LD` header** required on every POST | `accessCode()` | CSRF: another site can't add a custom header without a CORS preflight, which the server never answers |
| **Host check**: IP, `lightning`, or `lightning.<local suffix>` | `hostOk()`, `HOST_SUFFIXES` | DNS rebinding (a page reading the API through its own domain) |
| Own **`/api/update`**; auth checked at `UPLOAD_FILE_START`, refused uploads are read and discarded | `handleUpdateUpload()` | Unauthenticated firmware replacement (the stock `ESP8266HTTPUpdateServer` has no auth by default) |
| IDE OTA only with `OTA_PASSWORD` | `staWatch()` | Unauthenticated network uploads on port 8266 |
| Setup AP only on purpose (first boot, Reset Wi-Fi, reset twice, failed first join) | `setup()` | Takeover of the open AP after a power cut |
| Login throttle: 1 s after a wrong password, 30 s after 5 in a row | `loginFailed()` | Online guessing |
| Test strikes ≤ 1/s; `/api/config` writes flash only if something changed | `handleSimulate()`, `settingsEqual()` | Log flooding, flash wear |

Access levels (`enum Access` in `auth.h`): **OPEN** (all GETs), **XHR**
(`/api/time`, `/api/login`, `/api/password`), **OWNER** (every other POST).
Refusals: 421 wrong host, 403 no `X-LD`, 428 no password set yet, 401 bad
token, 429 throttled.

**Not covered** (also in the README): plain HTTP (someone with the Wi-Fi key
can sniff the token); the open setup AP while setup runs (set `AP_PASS`);
trust-on-first-use for the first password after upgrading from v1.1.0; the
Wi-Fi password is readable from flash with physical access (no flash
encryption on the ESP8266); one slow client can stall the single-client
server. The core drops request bodies too big for the heap (allocation
fails, connection closed); there is no clean hook to cap them earlier.

---

## 7. HTTP API

**STA mode** (access level in brackets, see 6b):

| Route | |
|---|---|
| `GET /` | dashboard [open] |
| `GET /api/state?since=<seq>` | status + strikes committed after `seq` (inline, from a 32-entry RAM ring). `reload:1` = page must refetch the log. `hasPass`; `auth` = this request's token is valid [open] |
| `GET /api/strikes[?since=<epoch>]` | whole log, `[[epoch, distance, energy, flags], …]`, streamed [open] |
| `GET /api/config` | sensor settings, location, radio settings, IP, storage use; never the Wi-Fi password, hash or token [open] |
| `GET /api/tune` · `GET /api/regs` | antenna sweep status · raw AS3935 registers [open] |
| `POST /api/time?t=<epoch>` | sets the clock if NTP has not [xhr] |
| `POST /api/login` (`pass`) | → `{token}`; 401 wrong, 429 throttled, 428 no password yet [xhr] |
| `POST /api/password` (`new`, `old`) | set the first password (no `old`) or change it → new `{token}` [xhr] |
| `POST /api/config` | sensor settings, location (`lat`,`lon` / `clearLoc`), `tuneCap`, `wifiPower` (−1 auto, 8/11/14/17/20), `wifiSleep` (0/1) [owner] |
| `POST /api/tune` | start the antenna sweep [owner] |
| `POST /api/simulate` | logs a test strike (flag 1) at a random distance, ≤ 1/s [owner] |
| `POST /api/clearlog` | deletes the log [owner] |
| `POST /api/reset` | forget WiFi, reboot to setup (device password kept) [owner] |
| `POST /api/update` | multipart firmware upload, reboots on success [owner] |
| `GET /update` | 302 to `/` (old bookmarks) |

**AP mode:** `/` · `/scan` · `/rescan` · `/info` (`hasPass`, saved `ssid`,
`failed`, `forced`) · `POST /save` (`ssid`, `pass`, `devpass`; needs `X-LD`;
a blank `pass` with the saved SSID keeps the saved password) · everything
else redirects to the setup page. No host check in AP mode: captive-portal
probes use other names, and that network has no internet.

---

## 8. Storage (LittleFS)

| File | Contents | Rotation |
|---|---|---|
| `/config.json` | WiFi (+ `wifiFresh`), device password salt/hash, sign-in token, sensor settings, tuning cap, radio settings, location | written to `/config.tmp` then renamed (power-cut safe) |
| `/strikes.csv` | `epoch,distance,energy,flags` | 64 KB → `/strikes.old.csv` (~5400 strikes total) |

Distance: 1 = overhead, 63 = out of range. Flags: bit 0 = test strike.

---

## 9. Testing

- **Compile check:** `arduino-cli compile --fqbn esp8266:esp8266:d1_mini:eesz=4M2M LightningDetector`
  (also compile once with `AS3935_USE_SPI` defined). On Windows, keep the
  build path short: the toolchain fails on paths over ~260 characters.
- **Page JS:** extract the `<script>` blocks from `webpages.h` and run
  `node --check`.
- **Dashboard without hardware:** serve the extracted `DASH_PAGE` HTML from a
  small mock of the API above (v1.0.0 was checked this way: rings, chart,
  tuning, placing, settings, 375 px phone width; v1.2.0 with a mock that
  applies the same access rules: password create/unlock/change, throttling,
  401/403/428 refusals, firmware upload and reload, the four setup-page
  states).
- **Also compile with `OTA_PASSWORD` defined**: that path is off by default.
- **Only verifiable on real hardware:** the host check, reset-twice, the
  real upload into flash, the AS3935 driver itself, I2C vs SPI on
  your board, tuning results, real-world noise. A piezo BBQ lighter clicked
  near the antenna makes disturbers (sometimes a "strike") for a quick
  end-to-end check.

---

## 10. Hard-won lessons (inherited from the Grandfather Clock project)

1. **Never trust vendor pinouts.** Some CJMCU boards swap MOSI/MISO labels.
2. **Blocking WiFi scans drop connected AP clients**: scan before raising the
   AP, rescan asynchronously.
3. **Never `delay()`/`ESP.restart()` inside an HTTP handler.** Defer via
   `g_rebootAt`.
4. **No external resources on the setup page.**
5. **String concat in hot API paths fragments the heap.**
6. **A cached browser page looks exactly like a frozen device.** Hard-refresh.
7. **iPhone + no-internet AP:** cellular data hijacks 192.168.4.1.
8. **Arduino `.ino` prototypes:** the IDE inserts prototypes for free functions
   at the top of the sketch, before any struct they use is declared. Types
   used in `.ino` function signatures go in a header or become methods.
9. **CARTO basemaps now need an API key.** The dark map uses Esri's World Dark
   Gray canvas (no key, native zoom ≤ 16).
10. **100 kHz I2C puts its 5th harmonic on 500 kHz**, the antenna frequency.
    Pick a bus clock whose odd harmonics miss 483–517 kHz (80 kHz does).

11. **`ESP8266WebServer::addHook` runs before the headers are parsed.** It
    can't see `Host`, `Content-Length` or custom headers; per-route checks
    go in the handler (`guard()`), with `collectHeaders()` for custom ones.
12. **RTC user memory blocks 64+ belong to eboot** (its OTA command lives at
    0x60001200). Use blocks 0–63 for your own data.
13. **`MDNS.begin()` before the station has an IP fails and never retries.**
    Start it on the first successful join; after that it restarts itself.
14. **PowerShell 5.1 `Set-Content -Encoding utf8` writes a BOM**, which breaks
    a C header. Patch test-build copies with `sed` or Python.

---

## 11. Known open items

- The driver is written from the ams datasheet (v1-04) and compile-checked,
  not yet run on hardware. Same for the v1.2.0 security code (mock-tested
  only).
- Plain HTTP: the sign-in token can be sniffed by someone who already has
  the Wi-Fi key. A TLS server is too heavy for the ESP8266 here.
- The setup AP is open unless `AP_PASS` is set; it only exists during
  deliberate setup.
- Strikes while in setup (AP) mode stay in the RAM queue and are lost on the
  reboot that follows setup.
- Map tiles (Esri, OpenStreetMap) and address search (Nominatim) need
  internet and are for light personal use under their terms.

---

## 12. Working agreement

- **Semver:** MAJOR = breaking hardware/storage, MINOR = features, PATCH =
  fixes. `FW_VERSION` in `config.h` shows on the serial banner and the
  dashboard's Device panel (that's how you confirm an update took).
