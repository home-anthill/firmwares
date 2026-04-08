#pragma once

// ---------------------------------------------------------------------------
// Adafruit_SSD1306 mock for host-side (native) unit test compilation.
//
// Captures init and render calls in DisplayMockState so tests can verify
// display behaviour without real I2C hardware.
//
// Usage:
//   DisplayMockState::reset();                         // clean state in SetUp
//   DisplayMockState::instance().begin_result = false; // simulate init fail
//   init_display();
//   EXPECT_EQ(DisplayMockState::instance().begin_count, 1);
// ---------------------------------------------------------------------------

#include "Adafruit_GFX.h"
#include "Wire.h"

#define SSD1306_SWITCHCAPVCC 1

struct DisplayMockState {
  bool  begin_result{true};
  bool  initialized{false};
  int   begin_count{0};
  int   clear_count{0};
  int   display_count{0};
  float last_print_float{0.0f};
  const char* last_print_str{nullptr};

  static DisplayMockState& instance() {
    static DisplayMockState s;
    return s;
  }
  static void reset() { instance() = DisplayMockState{}; }
};

class Adafruit_SSD1306 : public Adafruit_GFX {
public:
  Adafruit_SSD1306(int16_t w, int16_t h, TwoWire* /*wire*/, int8_t /*rst*/)
    : Adafruit_GFX(w, h) {}

  bool begin(uint8_t /*vcc*/, uint8_t /*addr*/) {
    auto& s = DisplayMockState::instance();
    s.begin_count++;
    s.initialized = s.begin_result;
    return s.begin_result;
  }

  void clearDisplay() { DisplayMockState::instance().clear_count++; }
  void display()      { DisplayMockState::instance().display_count++; }

  void setTextSize(uint8_t /*s*/) override          {}
  void setTextColor(uint16_t /*c*/) override        {}
  void cp437(bool /*x*/) override                   {}
  void setCursor(int16_t /*x*/, int16_t /*y*/) override {}

  void print(float v) override        { DisplayMockState::instance().last_print_float = v; }
  void print(const char* s) override  { DisplayMockState::instance().last_print_str = s; }
};
