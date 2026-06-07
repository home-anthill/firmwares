#pragma once

// ---------------------------------------------------------------------------
// Wire (TwoWire / I2C) mock for host-side (native) unit test compilation.
// Used by display.cpp and temp_sensor.cpp.
// ---------------------------------------------------------------------------

#include <cstdint>

class TwoWire {
public:
  void setPins(int /*sda*/, int /*scl*/) {}
  void begin() {}
  void begin(int /*sda*/, int /*scl*/) {}
};

inline TwoWire Wire;
