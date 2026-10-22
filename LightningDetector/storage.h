/* =====================================================================
   storage.h  —  Onboard flash filesystem (LittleFS) + strike log
   ---------------------------------------------------------------------
   IDE REQUIREMENT: select a flash layout that reserves filesystem space,
   e.g.  Tools -> Flash Size -> "4MB (FS:2MB OTA:~1019KB)".  With
   "FS:none" the filesystem cannot mount and logging/settings are off.

   Strike log rows:  epoch_seconds,distance_km,energy,flags\n
     distance_km: 1 = storm overhead, 5..40, 63 = out of range
     flags:       bit 0 = test strike from the "Simulate strike" button
   ===================================================================== */
#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <LittleFS.h>

#define STRIKE_FLAG_TEST 0x01

struct StrikeRow {
  uint32_t t;        // UTC epoch seconds
  uint8_t  d;        // distance register value
  uint32_t e;        // energy
  uint8_t  f;        // STRIKE_FLAG_*
};

void   storageBegin();       // mounts LittleFS (auto-formats on first boot)
bool   storageReady();       // true if the filesystem mounted

bool   logStrike(const StrikeRow& r);   // append, rotating at STRIKE_LOG_MAX_BYTES
void   clearStrikeLog();
size_t strikeLogBytes();     // both files together

// Visit every stored strike, oldest first. Return false from `fn` to stop.
// `idle` (may be null) runs between buffer refills so a long read can keep
// servicing the sensor.
typedef bool (*StrikeVisitor)(const StrikeRow& r, void* ctx);
void   forEachStrike(StrikeVisitor fn, void* ctx, void (*idle)());

#endif // STORAGE_H
