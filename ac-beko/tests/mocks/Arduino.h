#pragma once

// ---------------------------------------------------------------------------
// Arduino framework mock for host-side (native) unit test compilation.
//
// This header is placed on the include path BEFORE the real Arduino SDK so
// that source files compiled for testing resolve Arduino symbols here instead
// of needing the actual ESP32 toolchain.
//
// Only the symbols used by the modules under test are stubbed out.
// ---------------------------------------------------------------------------

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <functional>

// --- Arduino PROGMEM / Flash string stubs -----------------------------------
// ArduinoJson enables DeserializationError::f_str() when
// ARDUINOJSON_ENABLE_PROGMEM=1 (set in CMakeLists.txt for test_ir_beko).
// On the host there is no real PROGMEM, so stub the required macros and type.
#ifndef PROGMEM
#  define PROGMEM
#endif
#ifndef pgm_read_byte
#  define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#endif
// Forward declaration matches what ArduinoJson's f_str() returns.
class __FlashStringHelper;

// --- Basic Arduino types ----------------------------------------------------

using byte     = uint8_t;
using word     = uint16_t;
using boolean  = bool;

// Re-export the std::string as Arduino's String type so headers that use
// String compile without change.
using String = std::string;

// --- Math constants ---------------------------------------------------------

#ifndef NAN
#  define NAN (0.0f / 0.0f)
#endif
#ifndef INFINITY
#  define INFINITY __builtin_inff()
#endif

using std::isnan;
using std::isinf;

// --- Print / Stream ---------------------------------------------------------
// Minimal subset of Arduino's Print/Stream hierarchy needed for ArduinoJson
// serialisation (serializeJson / serializeJsonPretty) and deserialisation
// (deserializeJson from a stream) to compile and run on the host.

class Print {
public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t) { return 0; }
  virtual size_t write(const uint8_t* buf, size_t n) {
    size_t w = 0;
    while (n--) w += write(*buf++);
    return w;
  }
  size_t write(const char* s) {
    return write(reinterpret_cast<const uint8_t*>(s), strlen(s));
  }
};

class Stream : public Print {
public:
  virtual int read()      = 0;
  virtual int peek()      = 0;
  virtual int available() = 0;
};

// --- Serial mock ------------------------------------------------------------

class SerialMock : public Print {
public:
  void begin(unsigned long /*baud*/) {}

  // write() — satisfies ArduinoJson's Print concept (silently discards).
  size_t write(uint8_t /*b*/) override { return 1; }
  size_t write(const uint8_t* /*buf*/, size_t n) override { return n; }

  void print(const char* msg)        { (void)msg; }
  void print(const std::string& msg) { (void)msg; }
  void print(int v)                  { (void)v;   }
  void print(float v)                { (void)v;   }

  void println(const char* msg = "")               { (void)msg; }
  void println(const std::string& msg)             { (void)msg; }
  void println(int v)                              { (void)v;   }
  void println(float v)                            { (void)v;   }
  void println(const __FlashStringHelper* /*msg*/) {}

  // Variadic printf — silently discards output in tests.
  void printf(const char* fmt, ...) { (void)fmt; }
};

// C++17 inline variable: exactly one definition across all translation units.
inline SerialMock Serial;

// --- ESP mock ---------------------------------------------------------------

struct EspClass {
  void restart() {}
};
inline EspClass ESP;

// --- Time / delay -----------------------------------------------------------

inline void          delay(unsigned long ms) { (void)ms; }
inline unsigned long millis()                { return 0UL; }
inline unsigned long micros()                { return 0UL; }
inline void          delayMicroseconds(unsigned int us) { (void)us; }

// setTime — from the Arduino Time library; no-op in tests.
inline void setTime(int /*hr*/, int /*min*/, int /*sec*/,
                    int /*day*/, int /*mo*/,  int /*yr*/) {}

// --- GPIO stubs (unused by math_utils, included for completeness) -----------

inline void pinMode(uint8_t /*pin*/, uint8_t /*mode*/) {}
inline void digitalWrite(uint8_t /*pin*/, uint8_t /*val*/) {}
inline int  digitalRead(uint8_t /*pin*/) { return 0; }
inline int  analogRead(uint8_t /*pin*/) { return 0; }

#define INPUT  0x0
#define OUTPUT 0x1
#define HIGH   0x1
#define LOW    0x0
