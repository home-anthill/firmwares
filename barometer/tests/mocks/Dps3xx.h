#pragma once

// ---------------------------------------------------------------------------
// Dps3xx mock for host-side (native) unit test compilation.
//
// The singleton Dps3xxMockState controls what measureTempOnce() and
// measurePressureOnce() return so tests can exercise both the happy path
// and the read-failure path of barometer_get_temperature() /
// barometer_get_airpressure() without real hardware.
//
// Usage in tests:
//   Dps3xxMockState::reset();
//   Dps3xxMockState::instance().temp_value   = 21.5f;
//   Dps3xxMockState::instance().temp_success = true;
//   EXPECT_FLOAT_EQ(barometer_get_temperature(), 21.5f);
// ---------------------------------------------------------------------------

#include "Wire.h"  // TwoWire used by begin()

// ---------------------------------------------------------------------------
// Singleton mock state — reset in test SetUp() for isolation.
// ---------------------------------------------------------------------------
struct Dps3xxMockState {
  float temp_value{0.0f};
  bool  temp_success{true};
  float pressure_value{0.0f};
  bool  pressure_success{true};

  static Dps3xxMockState& instance() {
    static Dps3xxMockState s;
    return s;
  }
  static void reset() { instance() = Dps3xxMockState{}; }
};

// ---------------------------------------------------------------------------
// Dps3xx mock — mirrors the real Infineon library interface.
// ---------------------------------------------------------------------------
class Dps3xx {
public:
  void begin(TwoWire& /*wire*/) {}

  // Returns 0 on success (matching real library contract).
  int measureTempOnce(float& temperature) {
    auto& s = Dps3xxMockState::instance();
    temperature = s.temp_value;
    return s.temp_success ? 0 : -1;
  }

  int measurePressureOnce(float& pressure) {
    auto& s = Dps3xxMockState::instance();
    pressure = s.pressure_value;
    return s.pressure_success ? 0 : -1;
  }
};
