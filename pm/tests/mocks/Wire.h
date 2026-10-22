#pragma once
#include <cstdint>
class TwoWire {
public:
  int sda = -1, scl = -1, begin_count = 0;
  bool begin_ok = true;
  uint8_t bus_error = 0;
  uint32_t clock = 0;
  uint16_t timeout = 0;
  void setPins(int a, int b) { sda = a; scl = b; }
  bool begin() { ++begin_count; return begin_ok; }
  bool begin(int a, int b) { setPins(a, b); return begin(); }
  void setClock(uint32_t v) { clock = v; }
  void setTimeOut(uint16_t v) { timeout = v; }
  void beginTransmission(uint8_t) {}
  uint8_t endTransmission() { return bus_error; }
};
inline TwoWire Wire;
