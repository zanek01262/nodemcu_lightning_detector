/* =====================================================================
   as3935.h  —  Register-level AS3935 driver (I2C or SPI, no library)
   ---------------------------------------------------------------------
   Written against the ams AS3935 datasheet v1-04. The interface is picked
   at compile time with AS3935_USE_SPI in config.h.

   The chip raises IRQ on an event and holds it high until REG0x03 is
   read; the datasheet asks for a 2 ms wait before that read. asPoll()
   does the wait without blocking loop(), so the web server keeps running.
   ===================================================================== */
#ifndef AS3935_H
#define AS3935_H

#include <Arduino.h>

// Distance register special values (everything else is km).
#define AS_DIST_OVERHEAD      1
#define AS_DIST_OUT_OF_RANGE  63

enum AsEventType : uint8_t {
  AS_EV_NONE = 0,
  AS_EV_LIGHTNING,   // INT_L: a strike, with distance + energy
  AS_EV_DISTURBER,   // INT_D: man-made impulse rejected by the chip
  AS_EV_NOISE,       // INT_NH: background noise above the noise floor
  AS_EV_PURGE        // INT = 0: storm distance changed as old strikes aged out
};

struct AsEvent {
  uint8_t  type;
  uint8_t  distKm;   // 1 = overhead, 5..40 km, 63 = out of range
  uint32_t energy;   // dimensionless "pure number" (datasheet), 21 bits
};

struct AsTune {
  bool     busy;
  bool     valid;    // last sweep completed and saw a signal
  uint8_t  step;     // capacitor value being measured, 0..15
  int8_t   best;     // chosen capacitor value, -1 = none
  uint32_t hz[16];   // measured antenna frequency for each value
  const char* err;   // "" or why the last sweep failed
};

// Bring up the bus, find the chip, restore defaults and calibrate the RC
// oscillators. Safe to call again later (e.g. after rewiring).
bool        asBegin();
bool        asPresent();
const char* asBusName();      // "I2C 0x03" or "SPI"
bool        asDefaultsOk();   // registers read back their documented reset values
bool        asCalibOk();      // TRCO + SRCO calibration reported success
bool        asCalibrate();    // CALIB_RCO; run again after changing the tuning cap

// Register settings (each is a read-modify-write of one field).
void asSetOutdoor(bool outdoor);       // AFE gain: indoor 10010 / outdoor 01110
void asSetNoiseFloor(uint8_t nf);      // 0..7
void asSetWatchdog(uint8_t wdth);      // 0..10 characterised in the datasheet
void asSetSpikeRejection(uint8_t srej);// 0..11 characterised in the datasheet
void asSetMinStrikes(uint8_t n);       // 1, 5, 9 or 16
void asSetMaskDisturbers(bool mask);
void asSetTuneCap(uint8_t cap);        // 0..15, 8 pF per step
void asClearStatistics();

// Call every loop(). Returns true and fills `ev` when an interrupt was read.
bool asPoll(AsEvent& ev);

// Antenna auto-tune: sweeps all 16 capacitor values (~9 s), keeps the one
// closest to 500 kHz and recalibrates the oscillators. asTuneUpdate() must
// run every loop(); it returns true once, when a sweep finishes.
void          asTuneStart();
bool          asTuneUpdate();
const AsTune& asTuneState();

// Raw register access for the diagnostics endpoint.
uint8_t asReadReg(uint8_t reg);

#endif // AS3935_H
