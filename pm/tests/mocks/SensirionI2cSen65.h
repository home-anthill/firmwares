#pragma once
#include <Wire.h>
#include <cstddef>
constexpr uint8_t SEN65_I2C_ADDR_6B = 0x6b;
struct Sen65Mock {
  int16_t reset_error = 0, start_error = 0, read_error = 0;
  int starts = 0, reads = 0;
};
inline Sen65Mock sen65_mock;
inline void errorToString(int16_t, char*, size_t) {}
class SensirionI2cSen65 {
public:
  void begin(TwoWire&, uint8_t) {}
  int16_t deviceReset() { return sen65_mock.reset_error; }
  int16_t startContinuousMeasurement() { ++sen65_mock.starts; return sen65_mock.start_error; }
  int16_t readMeasuredValues(float& a, float& b, float& c, float& d, float& e, float& f, float& g, float& h) {
    ++sen65_mock.reads;
    a=1; b=2; c=3; d=4; e=50; f=23; g=100; h=5;
    return sen65_mock.read_error;
  }
};
