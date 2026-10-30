#pragma once

// ---------------------------------------------------------------------------
// Wire (TwoWire / I2C) mock for host-side (native) unit test compilation.
// Used by display.cpp and temp_sensor.cpp.
// ---------------------------------------------------------------------------

#include <cstdint>

class TwoWire {
public:
  int sda = -1;
  int scl = -1;
  int begin_count = 0;
  void setPins(int new_sda, int new_scl) { sda = new_sda; scl = new_scl; }
  void begin() { ++begin_count; }
  void begin(int new_sda, int new_scl) { setPins(new_sda, new_scl); begin(); }
};

inline TwoWire Wire;
