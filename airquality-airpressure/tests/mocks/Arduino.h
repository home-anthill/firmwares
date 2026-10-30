#pragma once

// ---------------------------------------------------------------------------
// Arduino framework mock for host-side (native) unit test compilation.
//
// Extends the base Arduino stub with:
//   - PROGMEM / pgm_read_byte / __FlashStringHelper stubs so that ArduinoJson
//     compiles when ARDUINOJSON_ENABLE_PROGMEM=1 is set (needed by targets
//     that compile controller.cpp which calls DeserializationError::f_str()).
//   - GpioMockState: records every digitalWrite() call so test_main_ino can
//     verify HEAT/COLD/FAN/PUMP relay outputs.
// ---------------------------------------------------------------------------

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <map>
#include <string>
#include <functional>

// --- Arduino PROGMEM / Flash string stubs -----------------------------------
// ArduinoJson enables DeserializationError::f_str() when
// ARDUINOJSON_ENABLE_PROGMEM=1 (set in CMakeLists.txt for test_controller).
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

using byte    = uint8_t;
using word    = uint16_t;
using boolean = bool;

// Re-export std::string as Arduino's String type.
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

struct EspMockState {
  int restart_count{0};

  static EspMockState& instance() {
    static EspMockState s;
    return s;
  }
  static void reset() { instance() = EspMockState{}; }
};

struct EspClass {
  void restart() { EspMockState::instance().restart_count++; }
};
inline EspClass ESP;

// --- Time / delay -----------------------------------------------------------

inline unsigned long g_mock_millis = 0UL;

inline void          delay(unsigned long ms) { (void)ms; }
inline unsigned long millis()                { return g_mock_millis; }
inline unsigned long micros()                { return 0UL; }
inline void          delayMicroseconds(unsigned int us) { (void)us; }
inline void          mock_set_millis(unsigned long ms)  { g_mock_millis = ms; }
inline void          mock_advance_millis(unsigned long ms) { g_mock_millis += ms; }

// setTime — from the Arduino Time library; no-op in tests.
inline void setTime(int /*hr*/, int /*min*/, int /*sec*/,
                    int /*day*/, int /*mo*/,  int /*yr*/) {}

// F() macro — on real Arduino wraps a string literal in flash memory.
// On the host it is a no-op identity: just returns the const char* directly.
#ifndef F
#  define F(string_literal) (string_literal)
#endif

// --- GPIO mock with state tracking ------------------------------------------
// GpioMockState::pin_values maps pin -> last value written via digitalWrite().
// Reset in test SetUp() to start each test with a clean slate.

inline int g_digital_read_value = 0;

struct GpioMockState {
  std::map<uint8_t, uint8_t> pin_modes;
  std::map<uint8_t, uint8_t> pin_values;
  int digital_read_value{0};

  static GpioMockState& instance() {
    static GpioMockState s;
    return s;
  }
  static void reset() {
    instance() = GpioMockState{};
    g_digital_read_value = 0;
  }
};

inline void pinMode(uint8_t pin, uint8_t mode) {
  GpioMockState::instance().pin_modes[pin] = mode;
}

inline void digitalWrite(uint8_t pin, uint8_t val) {
  GpioMockState::instance().pin_values[pin] = val;
}

inline void rgbLedWrite(uint8_t /*pin*/, uint8_t /*red*/, uint8_t /*green*/, uint8_t /*blue*/) {}

inline int  digitalRead(uint8_t /*pin*/) {
  return g_digital_read_value;
}

inline int  analogRead(uint8_t /*pin*/) { return 0; }

#define INPUT  0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2
#define HIGH   0x1
#define LOW    0x0
