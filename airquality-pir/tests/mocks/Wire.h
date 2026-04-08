#pragma once

// ---------------------------------------------------------------------------
// Wire (TwoWire / I2C) mock for host-side (native) unit test compilation.
// ---------------------------------------------------------------------------

#include <cstdint>

class TwoWire {
public:
  void setPins(uint8_t /*sda*/, uint8_t /*scl*/) {}
  void begin() {}
};

inline TwoWire Wire;
