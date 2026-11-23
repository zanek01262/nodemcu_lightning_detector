/* =====================================================================
   auth.cpp  —  Device password hashing and token checks
   ===================================================================== */
#include "auth.h"
#include "config.h"
#include "settings.h"
#include <bearssl/bearssl_hash.h>

static String toHex(const uint8_t* b, size_t n) {
  static const char hx[] = "0123456789abcdef";
  String s;
  s.reserve(n * 2);
  for (size_t i = 0; i < n; i++) { s += hx[b[i] >> 4]; s += hx[b[i] & 15]; }
  return s;
}

// The ESP8266's hardware random number generator (the radio is on, which
// is what makes it truly random).
static String randomHex(size_t bytes) {
  uint8_t b[32];
  if (bytes > sizeof(b)) bytes = sizeof(b);
  ESP.random(b, bytes);
  return toHex(b, bytes);
}

// Iterated SHA-256: slows down guessing from a copy of the flash. Each
// round costs ~50 us on the ESP8266, so a check takes about 0.1 s.
static String hashPassword(const String& pass, const String& salt) {
  uint8_t h[32];
  br_sha256_context c;
  br_sha256_init(&c);
  br_sha256_update(&c, salt.c_str(), salt.length());
  br_sha256_update(&c, pass.c_str(), pass.length());
  br_sha256_out(&c, h);
  for (uint16_t i = 1; i < AUTH_HASH_ROUNDS; i++) {
    br_sha256_init(&c);
    br_sha256_update(&c, h, sizeof(h));
    br_sha256_update(&c, pass.c_str(), pass.length());
    br_sha256_out(&c, h);
    if ((i & 255) == 0) yield();
  }
  return toHex(h, sizeof(h));
}

// Compares in the same time whether or not (and wherever) they differ.
static bool sameSecret(const String& a, const String& b) {
  if (a.length() != b.length() || a.length() == 0) return false;
  uint8_t d = 0;
  for (size_t i = 0; i < a.length(); i++) d |= (uint8_t)(a[i] ^ b[i]);
  return d == 0;
}

bool authHasPassword() {
  return settings.passHash.length() == 64 && settings.passSalt.length() == 32;
}

bool authCheckPassword(const String& pass) {
  if (!authHasPassword()) return false;
  return sameSecret(hashPassword(pass, settings.passSalt), settings.passHash);
}

void authSetPassword(const String& pass) {
  settings.passSalt  = randomHex(16);
  settings.passHash  = hashPassword(pass, settings.passSalt);
  settings.authToken = randomHex(16);
}

bool authTokenOk(const String& token) {
  return authHasPassword() && settings.authToken.length() == 32 &&
         sameSecret(token, settings.authToken);
}

const String& authToken() { return settings.authToken; }
