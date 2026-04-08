#pragma once

// ---------------------------------------------------------------------------
// Stub for <avr/pgmspace.h> used by ArduinoJson when ARDUINOJSON_ENABLE_PROGMEM=1
// is set without the ARDUINO define.  On the host there is no AVR toolchain,
// so we provide the minimal set of macros that ArduinoJson's pgmspace.hpp
// needs before it defines its own strlen_P / strcmp_P / memcpy_P helpers.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <cstring>

#ifndef PROGMEM
#  define PROGMEM
#endif

#ifndef pgm_read_byte
#  define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#endif

// Forward declaration expected by ArduinoJson's FlashString adapter.
class __FlashStringHelper;
