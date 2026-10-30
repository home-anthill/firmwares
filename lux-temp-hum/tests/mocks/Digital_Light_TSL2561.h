#pragma once

// ---------------------------------------------------------------------------
// Digital_Light_TSL2561 mock for host-side (native) unit test compilation.
//
// Usage in tests:
//   LightMockState::reset();
//   LightMockState::instance().lux_value = 1234;
//   EXPECT_EQ(light_get_value(), 1234L);
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Singleton mock state — reset in test SetUp() for isolation.
// ---------------------------------------------------------------------------
struct LightMockState {
  long lux_value{0};

  static LightMockState& instance() {
    static LightMockState s;
    return s;
  }
  static void reset() { instance() = LightMockState{}; }
};

// ---------------------------------------------------------------------------
// TSL2561 mock — matches the global object API used by light_sensor.cpp.
// ---------------------------------------------------------------------------
class TSL2561Class {
public:
  void init()              {}
  long readVisibleLux()    { return LightMockState::instance().lux_value; }
};

inline TSL2561Class TSL2561;
