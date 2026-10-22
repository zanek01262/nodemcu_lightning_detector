/* =====================================================================
   as3935.cpp  —  AS3935 driver: bus access, interrupts, antenna tuning
   ===================================================================== */
#include "as3935.h"
#include "config.h"

#ifdef AS3935_USE_SPI
  #include <SPI.h>
#else
  #include <Wire.h>
#endif

// ---- Registers (datasheet Figure 22) --------------------------------
#define R_AFE      0x00   // [5:1] AFE_GB, [0] PWD
#define R_THRESH   0x01   // [6:4] NF_LEV, [3:0] WDTH
#define R_LIGHT    0x02   // [6] CL_STAT, [5:4] MIN_NUM_LIGH, [3:0] SREJ
#define R_INT      0x03   // [7:6] LCO_FDIV, [5] MASK_DIST, [3:0] INT
#define R_ENERGY   0x04   // 0x04 LSB, 0x05 MSB, 0x06 [4:0] MMSB
#define R_DIST     0x07   // [5:0] distance
#define R_DISP     0x08   // [7] LCO, [6] SRCO, [5] TRCO on IRQ; [3:0] TUN_CAP
#define R_TRCO     0x3A   // [7] calib done, [6] calib failed
#define R_SRCO     0x3B
#define CMD_PRESET 0x3C   // direct command: all registers to defaults
#define CMD_CALIB  0x3D   // direct command: calibrate the RC oscillators
#define DIRECT_CMD 0x96

#define INT_NH     0x01
#define INT_D      0x04
#define INT_L      0x08

static bool    s_present  = false;
static bool    s_defaults = false;
static bool    s_calOk    = false;
static uint8_t s_lastDist = 0;
static char    s_busName[12] = "-";

#ifndef AS3935_USE_SPI
static uint8_t s_addr = AS3935_I2C_ADDR;
#endif

// ---- IRQ ------------------------------------------------------------
// One ISR serves both jobs: during a tuning sweep it counts the divided
// antenna frequency; otherwise the edge flag tells asPoll() an event is up.
static volatile uint32_t s_edges   = 0;
static volatile bool     s_irqFlag = false;
static bool              s_isrOn   = false;

static void IRAM_ATTR asIsr() {
  s_edges++;
  s_irqFlag = true;
}

static void isrAttach() {
  if (s_isrOn) return;
  attachInterrupt(digitalPinToInterrupt(PIN_AS_IRQ), asIsr, RISING);
  s_isrOn = true;
}

// The oscillator calibration puts the 1.1 MHz SRCO on IRQ for 2 ms. An
// interrupt per edge at that rate would stall the CPU, so the ISR is
// detached around it.
static void isrDetach() {
  if (!s_isrOn) return;
  detachInterrupt(digitalPinToInterrupt(PIN_AS_IRQ));
  s_isrOn = false;
}

// =====================================================================
//  Bus access
// =====================================================================
#ifdef AS3935_USE_SPI

static const SPISettings kSpi(AS3935_SPI_HZ, MSBFIRST, SPI_MODE1);  // CPHA=1

static void busBegin() {
  pinMode(PIN_AS_CS, OUTPUT);
  digitalWrite(PIN_AS_CS, HIGH);
  SPI.begin();
  strcpy(s_busName, "SPI");
}

static bool busWrite(uint8_t reg, uint8_t val) {
  SPI.beginTransaction(kSpi);
  digitalWrite(PIN_AS_CS, LOW);
  SPI.transfer(reg & 0x3F);          // mode bits 00 = write / direct command
  SPI.transfer(val);
  digitalWrite(PIN_AS_CS, HIGH);
  SPI.endTransaction();
  return true;
}

static bool busRead(uint8_t reg, uint8_t* buf, uint8_t n) {
  SPI.beginTransaction(kSpi);
  digitalWrite(PIN_AS_CS, LOW);
  SPI.transfer(0x40 | (reg & 0x3F)); // mode bits 01 = read
  for (uint8_t i = 0; i < n; i++) buf[i] = SPI.transfer(0);
  // Datasheet: CS must go high-low-high to terminate a read.
  digitalWrite(PIN_AS_CS, HIGH); delayMicroseconds(1);
  digitalWrite(PIN_AS_CS, LOW);  delayMicroseconds(1);
  digitalWrite(PIN_AS_CS, HIGH);
  SPI.endTransaction();
  return true;
}

// SPI has no ACK, so presence is judged purely by the reset values.
static bool busFind() { return true; }

#else  // ---- I2C ------------------------------------------------------

static void busBegin() {
  Wire.begin(PIN_AS_SDA, PIN_AS_SCL);
  Wire.setClock(AS3935_I2C_HZ);
}

static bool busWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(s_addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool busRead(uint8_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(s_addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;   // repeated START
  if (Wire.requestFrom((int)s_addr, (int)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read(); // address auto-increments
  return true;
}

// Try the configured address first, then the other two the ADD pins allow.
static bool busFind() {
  const uint8_t tryAddr[4] = { AS3935_I2C_ADDR, 0x03, 0x02, 0x01 };
  for (uint8_t i = 0; i < 4; i++) {
    Wire.beginTransmission(tryAddr[i]);
    if (Wire.endTransmission() == 0) {
      s_addr = tryAddr[i];
      snprintf(s_busName, sizeof(s_busName), "I2C 0x%02X", s_addr);
      return true;
    }
  }
  strcpy(s_busName, "I2C none");
  return false;
}

#endif

static uint8_t rd(uint8_t reg) {
  uint8_t v = 0;
  busRead(reg, &v, 1);
  return v;
}

// Read-modify-write of the bits in `mask`.
static void wrBits(uint8_t reg, uint8_t mask, uint8_t val) {
  uint8_t v = rd(reg);
  busWrite(reg, (uint8_t)((v & ~mask) | (val & mask)));
}

uint8_t asReadReg(uint8_t reg) { return s_present ? rd(reg) : 0; }

static uint8_t clampU8(uint8_t v, uint8_t hi) { return v > hi ? hi : v; }

// =====================================================================
//  Setup
// =====================================================================
bool asCalibrate() {
  if (!s_present) return false;
  bool wasOn = s_isrOn;
  isrDetach();
  // Datasheet "Clock Generation": CALIB_RCO, REG0x08[6]=1, wait 2 ms, =0.
  busWrite(CMD_CALIB, DIRECT_CMD);
  wrBits(R_DISP, 0x40, 0x40);
  delay(2);
  wrBits(R_DISP, 0x40, 0x00);
  delay(2);
  uint8_t t = rd(R_TRCO), s = rd(R_SRCO);
  // bit 7 = done (success), bit 6 = failed
  s_calOk = (t & 0x80) && !(t & 0x40) && (s & 0x80) && !(s & 0x40);
  Serial.printf("[AS3935] RCO calibration TRCO=0x%02X SRCO=0x%02X -> %s\n",
                t, s, s_calOk ? "ok" : "FAILED");
  s_irqFlag = false;
  if (wasOn) isrAttach();
  return s_calOk;
}

bool asBegin() {
  isrDetach();
  pinMode(PIN_AS_IRQ, INPUT);
  delay(5);                         // LCO + RCO start-up (2 ms each)
  busBegin();

  s_present = false;
  if (!busFind()) {
    Serial.println(F("[AS3935] no answer on I2C 0x01-0x03 - check wiring"));
    return false;
  }

  busWrite(CMD_PRESET, DIRECT_CMD);
  delay(2);
  uint8_t r[3] = { 0, 0, 0 };
  bool readOk = busRead(R_AFE, r, 3);
  // Documented reset values: 0x00 = 0x24 (indoor), 0x01 = 0x22, 0x02 = 0xC2.
  s_defaults = readOk && r[0] == 0x24 && r[1] == 0x22 && r[2] == 0xC2;
  Serial.printf("[AS3935] %s regs 00=%02X 01=%02X 02=%02X (%s)\n", s_busName,
                r[0], r[1], r[2], s_defaults ? "defaults ok" : "UNEXPECTED");
#ifdef AS3935_USE_SPI
  // A floating MISO reads all 0x00 or 0xFF; only trust matching values.
  s_present = s_defaults;
#else
  s_present = readOk;               // it ACKed; odd values are reported, not fatal
#endif
  if (!s_present) return false;

  asCalibrate();
  rd(R_INT);                        // clear anything latched during start-up
  s_irqFlag = false;
  isrAttach();
  return true;
}

bool        asPresent()    { return s_present; }
const char* asBusName()    { return s_busName; }
bool        asDefaultsOk() { return s_defaults; }
bool        asCalibOk()    { return s_calOk; }

void asSetOutdoor(bool outdoor) {
  if (s_present) wrBits(R_AFE, 0x3E, (outdoor ? 0x0E : 0x12) << 1);
}
void asSetNoiseFloor(uint8_t nf) {
  if (s_present) wrBits(R_THRESH, 0x70, (uint8_t)(clampU8(nf, 7) << 4));
}
void asSetWatchdog(uint8_t w) {
  if (s_present) wrBits(R_THRESH, 0x0F, clampU8(w, 15));
}
void asSetSpikeRejection(uint8_t s) {
  if (s_present) wrBits(R_LIGHT, 0x0F, clampU8(s, 15));
}
void asSetMinStrikes(uint8_t n) {
  uint8_t code = n >= 16 ? 3 : n >= 9 ? 2 : n >= 5 ? 1 : 0;
  if (s_present) wrBits(R_LIGHT, 0x30, code << 4);
}
void asSetMaskDisturbers(bool mask) {
  if (s_present) wrBits(R_INT, 0x20, mask ? 0x20 : 0x00);
}
void asSetTuneCap(uint8_t cap) {
  if (s_present) wrBits(R_DISP, 0x0F, cap & 0x0F);
}
void asClearStatistics() {
  if (!s_present) return;           // toggle CL_STAT high-low-high
  wrBits(R_LIGHT, 0x40, 0x40);
  wrBits(R_LIGHT, 0x40, 0x00);
  wrBits(R_LIGHT, 0x40, 0x40);
}

// =====================================================================
//  Interrupt servicing
// =====================================================================
static AsTune s_tune = { false, false, 0, -1, {0}, "" };

bool asPoll(AsEvent& ev) {
  static bool     waiting   = false;
  static uint32_t seenAt    = 0;
  static uint32_t holdUntil = 0;
  if (!s_present || s_tune.busy) return false;

  uint32_t now = millis();
  if ((int32_t)(now - holdUntil) < 0) return false;

  // The flag catches an IRQ pulse; the level catches one raised before the
  // ISR was attached (IRQ stays high until REG0x03 is read).
  if (!waiting) {
    if (!s_irqFlag && digitalRead(PIN_AS_IRQ) == LOW) return false;
    waiting = true;
    seenAt  = now;
    return false;
  }
  if (now - seenAt < 3) return false;  // datasheet: wait 2 ms before reading
  waiting   = false;
  s_irqFlag = false;

  uint8_t i = rd(R_INT) & 0x0F;        // reading REG0x03 drops IRQ
  ev.type = AS_EV_NONE; ev.distKm = 0; ev.energy = 0;

  if (i & INT_L) {
    uint8_t b[4] = { 0, 0, 0, 0 };     // 0x04..0x07: energy LSB, MSB, MMSB, distance
    busRead(R_ENERGY, b, 4);
    ev.type   = AS_EV_LIGHTNING;
    ev.energy = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)(b[2] & 0x1F) << 16);
    ev.distKm = b[3] & 0x3F;
    s_lastDist = ev.distKm;
    return true;
  }
  if (i & INT_D) { ev.type = AS_EV_DISTURBER; return true; }
  if (i & INT_NH) {
    // INT_NH stays asserted for as long as the noise lasts, so IRQ comes
    // straight back. Look again in 250 ms rather than hammering the bus.
    holdUntil = now + 250;
    ev.type = AS_EV_NOISE;
    return true;
  }
  // INT = 0: the distance estimate moved because old strikes were purged.
  // A stuck-high IRQ line also lands here, so only a real change counts.
  holdUntil = now + 50;
  uint8_t d = rd(R_DIST) & 0x3F;
  if (d == s_lastDist) return false;
  s_lastDist = d;
  ev.type   = AS_EV_PURGE;
  ev.distKm = d;
  return true;
}

// =====================================================================
//  Antenna tuning
// =====================================================================
static uint8_t  s_tPhase  = 0;     // 0 = settling, 1 = counting
static uint32_t s_tStart  = 0;
static uint32_t s_tEdges0 = 0;
static uint32_t s_tUs0    = 0;
static uint8_t  s_prevCap = 0;     // restored if a sweep sees no signal

static void tuneShow(uint8_t cap) {
  busWrite(R_DISP, 0x80 | (cap & 0x0F));   // LCO on IRQ + this capacitor
  s_tPhase = 0;
  s_tStart = millis();
}

void asTuneStart() {
  if (!s_present || s_tune.busy) return;
  memset(s_tune.hz, 0, sizeof(s_tune.hz));
  s_tune.busy  = true;
  s_tune.valid = false;
  s_tune.step  = 0;
  s_tune.best  = -1;
  s_tune.err   = "";
  s_prevCap    = rd(R_DISP) & 0x0F;
  wrBits(R_INT, 0xC0, 0xC0);              // LCO_FDIV = 128
  isrAttach();
  tuneShow(0);
  Serial.println(F("[AS3935] antenna tuning started"));
}

bool asTuneUpdate() {
  if (!s_tune.busy) return false;
  uint32_t now = millis();
  if (s_tPhase == 0) {
    if (now - s_tStart < TUNE_SETTLE_MS) return false;
    s_tEdges0 = s_edges;
    s_tUs0    = micros();
    s_tStart  = now;
    s_tPhase  = 1;
    return false;
  }
  if (now - s_tStart < TUNE_WINDOW_MS) return false;

  uint32_t edges = s_edges - s_tEdges0;
  uint32_t us    = micros() - s_tUs0;
  uint32_t hz    = us ? (uint32_t)((uint64_t)edges * TUNE_DIV * 1000000ULL / us) : 0;
  s_tune.hz[s_tune.step] = hz;
  Serial.printf("[AS3935] tune cap %2u (%3u pF): %lu Hz\n",
                s_tune.step, s_tune.step * 8, (unsigned long)hz);

  if (++s_tune.step < 16) { tuneShow(s_tune.step); return false; }

  // Sweep done: pick the value closest to 500 kHz.
  int8_t   best  = -1;
  uint32_t bestE = 0xFFFFFFFFUL;
  for (uint8_t c = 0; c < 16; c++) {
    if (s_tune.hz[c] == 0) continue;
    uint32_t e = s_tune.hz[c] > TUNE_TARGET_HZ ? s_tune.hz[c] - TUNE_TARGET_HZ
                                               : TUNE_TARGET_HZ - s_tune.hz[c];
    if (e < bestE) { bestE = e; best = (int8_t)c; }
  }
  wrBits(R_INT, 0xC0, 0x00);              // LCO_FDIV back to 16
  s_tune.busy = false;
  if (best < 0) {
    s_tune.err = "no signal on IRQ - check the IRQ wire";
    busWrite(R_DISP, s_prevCap);
    Serial.println(F("[AS3935] tuning failed: no pulses on IRQ"));
  } else {
    s_tune.best  = best;
    s_tune.valid = true;
    busWrite(R_DISP, (uint8_t)best);      // display off, keep the capacitor
    Serial.printf("[AS3935] tuned: cap %d -> %lu Hz\n", best,
                  (unsigned long)s_tune.hz[best]);
  }
  // The RC oscillators are calibrated against the antenna, so redo them.
  asCalibrate();
  rd(R_INT);
  s_irqFlag = false;
  return true;
}

const AsTune& asTuneState() { return s_tune; }
