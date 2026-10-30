#pragma once
#include <cstdint>
#include "Wire.h"

static constexpr uint8_t SHT45_I2C_ADDR_44 = 0x44;

struct Sht45MockState {
  uint8_t address = 0;
  TwoWire* bus = nullptr;
  int reset_count = 0;
  int read_count = 0;
  int16_t reset_error = 0;
  int16_t read_error = 0;
  float temperature = 23.5f;
  float humidity = 60.0f;
  static Sht45MockState& instance() { static Sht45MockState state; return state; }
  static void reset() { instance() = Sht45MockState{}; }
};

class SensirionI2cSht4x {
public:
  void begin(TwoWire& bus, uint8_t address) {
    Sht45MockState::instance().bus = &bus;
    Sht45MockState::instance().address = address;
  }
  int16_t softReset() {
    auto& state = Sht45MockState::instance();
    ++state.reset_count;
    return state.reset_error;
  }
  int16_t measureHighPrecision(float& temperature, float& humidity) {
    auto& state = Sht45MockState::instance();
    ++state.read_count;
    // Deliberately write values even on failure to verify they are discarded.
    temperature = state.temperature;
    humidity = state.humidity;
    return state.read_error;
  }
};
