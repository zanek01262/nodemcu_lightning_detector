/* =====================================================================
   auth.h  —  Device password and browser sign-in token
   ---------------------------------------------------------------------
   The password itself is never stored: only a random salt and an
   iterated SHA-256 of salt + password (settings.passSalt / passHash).
   Signing in hands the browser a random 128-bit token (settings.authToken)
   that it sends back in an X-Auth header. Setting a new password makes a
   new token, which signs every other browser out.
   ===================================================================== */
#ifndef AUTH_H
#define AUTH_H

#include <Arduino.h>

// Who may call a route (see guard() in LightningDetector.ino).
enum Access : uint8_t {
  ACC_OPEN  = 0,   // anyone on the network (viewing)
  ACC_XHR   = 1,   // must come from a page on this device (X-LD header)
  ACC_OWNER = 2    // must also carry a valid sign-in token (X-Auth header)
};

bool          authHasPassword();
bool          authCheckPassword(const String& pass);   // ~0.1 s on purpose
void          authSetPassword(const String& pass);     // new salt, hash and token; caller saves
bool          authTokenOk(const String& token);
const String& authToken();

#endif // AUTH_H
