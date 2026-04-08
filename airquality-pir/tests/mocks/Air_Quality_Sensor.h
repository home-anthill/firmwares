#pragma once

// ---------------------------------------------------------------------------
// Air_Quality_Sensor mock for host-side (native) unit test compilation.
//
// The singleton AirQualityMockState controls what slope() and getValue()
// return so tests can exercise all quality-level branches of
// airquality_has_newvalue() without real hardware.
//
// Usage in tests:
//   AirQualityMockState::reset();
//   AirQualityMockState::instance().slope_result = AirQualitySensor::FRESH_AIR;
//   EXPECT_TRUE(airquality_has_newvalue());
// ---------------------------------------------------------------------------

#include <cstdint>

// ---------------------------------------------------------------------------
// Singleton mock state — reset in test SetUp() for isolation.
// ---------------------------------------------------------------------------
struct AirQualityMockState {
  int  slope_result{-99};  // default: no recognised quality level
  int  get_value_result{0};
  bool init_result{true};

  static AirQualityMockState& instance() {
    static AirQualityMockState s;
    return s;
  }
  static void reset() { instance() = AirQualityMockState{}; }
};

// ---------------------------------------------------------------------------
// AirQualitySensor mock — mirrors the real Seeed library interface.
// ---------------------------------------------------------------------------
class AirQualitySensor {
public:
  // Quality-level constants (match real library values).
  static constexpr int FORCE_SIGNAL   = 0;
  static constexpr int HIGH_POLLUTION = 1;
  static constexpr int LOW_POLLUTION  = 2;
  static constexpr int FRESH_AIR      = 3;

  explicit AirQualitySensor(int /*pin*/) {}

  bool init()     { return AirQualityMockState::instance().init_result; }
  int  slope()    { return AirQualityMockState::instance().slope_result; }
  int  getValue() { return AirQualityMockState::instance().get_value_result; }
};
