#pragma once
#include <cstdint>
#include <cstring>
#include "Adafruit_Sensor.h"

struct LightMockState {
  float lux_value = 0;
  uint32_t luminosity = 0;
  bool started = false;
  static LightMockState& instance() { static LightMockState state; return state; }
  static void reset() { instance() = LightMockState{}; }
};
class Adafruit_TSL2591 {
public:
  explicit Adafruit_TSL2591(int32_t) {}
  bool begin() { LightMockState::instance().started = true; return true; }
  void getSensor(sensor_t* sensor) { *sensor = {}; }
  uint32_t getFullLuminosity() { return LightMockState::instance().luminosity; }
  float calculateLux(uint16_t, uint16_t) { return LightMockState::instance().lux_value; }
};
