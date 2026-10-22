#pragma once
#include <cstdint>
#include "Wire.h"
constexpr uint16_t ADS1X15_REG_CONFIG_MUX_DIFF_0_1 = 0;
constexpr uint16_t RATE_ADS1115_16SPS = 0x20;
enum adsGain_t { GAIN_FOUR = 0x600 };
struct AdcMockState {
  bool begin_success = true;
  bool complete = true;
  uint8_t address = 0;
  TwoWire* bus = nullptr;
  adsGain_t gain = GAIN_FOUR;
  uint16_t rate = 0;
  uint16_t mux = 99;
  bool continuous = true;
  int16_t raw = 0;
  int starts = 0;
  static AdcMockState& instance() { static AdcMockState state; return state; }
  static void reset() { instance() = AdcMockState{}; }
};
class Adafruit_ADS1115 {
public:
  bool begin(uint8_t address, TwoWire* bus) {
    auto& state = AdcMockState::instance();
    state.address = address; state.bus = bus;
    return state.begin_success;
  }
  void setGain(adsGain_t gain) { AdcMockState::instance().gain = gain; }
  void setDataRate(uint16_t rate) { AdcMockState::instance().rate = rate; }
  void startADCReading(uint16_t mux, bool continuous) {
    auto& state = AdcMockState::instance();
    state.mux = mux; state.continuous = continuous; ++state.starts;
  }
  bool conversionComplete() { return AdcMockState::instance().complete; }
  int16_t getLastConversionResults() { return AdcMockState::instance().raw; }
  float computeVolts(int16_t raw) { return raw * 1.024f / 32768.0f; }
};
