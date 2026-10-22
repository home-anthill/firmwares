#pragma once

// ---------------------------------------------------------------------------
// Wire (TwoWire / I2C) mock for host-side (native) unit test compilation.
// Used by display.cpp and temp_sensor.cpp.
// ---------------------------------------------------------------------------

#include <cstdint>

class TwoWire {
public:
  uint32_t clock = 0;
  void setClock(uint32_t hz) { clock = hz; }
  int sda = -1;
  int scl = -1;
  void setPins(int new_sda, int new_scl) { sda = new_sda; scl = new_scl; }
  bool begin_success = true;
  bool begin() { return begin_success; }
  bool begin(int new_sda, int new_scl, uint32_t hz = 100000) {
    sda = new_sda;
    scl = new_scl;
    clock = hz;
    return begin_success;
  }
};

inline TwoWire Wire;
