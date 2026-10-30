#pragma once
#include <cstdint>
#include <vector>
#include "Wire.h"

constexpr uint8_t BME68X_I2C_ADDR_LOW = 0x76;
constexpr uint8_t BME68X_I2C_ADDR_HIGH = 0x77;
constexpr float BSEC_SAMPLE_RATE_LP = 0.333333f;
enum bsecSensor { BSEC_OUTPUT_RAW_PRESSURE, BSEC_OUTPUT_IAQ, BSEC_OUTPUT_BREATH_VOC_EQUIVALENT, BSEC_OUTPUT_CO2_EQUIVALENT };
struct bme68xData { float pressure = 1013.25f; };
struct bsecData { bsecSensor sensor_id; float signal; uint8_t accuracy; };
struct bsecOutputs { bsecData output[4]; uint8_t nOutputs = 0; };
class Bsec2;
using bsecCallback = void (*)(bme68xData, bsecOutputs, Bsec2);
struct BsecMockState {
  bool low_available = true;
  bool high_available = false;
  bool subscription_success = true;
  bool run_success = true;
  bool fresh_data = true;
  float pressure = 1013.25f;
  float iaq = 75.5f;
  float voc = 0.75f;
  float eco2 = 650.0f;
  uint8_t accuracy = 3;
  float sample_rate = 0;
  std::vector<uint8_t> addresses;
  std::vector<bsecSensor> subscriptions;
  static BsecMockState& instance() { static BsecMockState state; return state; }
  static void reset() { instance() = BsecMockState{}; }
};
class Bsec2 {
public:
  int status = 0;
  struct { int status = 0; } sensor;
  bool begin(uint8_t address, TwoWire&) {
    auto& state = BsecMockState::instance();
    state.addresses.push_back(address);
    return address == BME68X_I2C_ADDR_LOW ? state.low_available : state.high_available;
  }
  bool updateSubscription(bsecSensor* outputs, uint8_t count, float rate) {
    auto& state = BsecMockState::instance();
    state.subscriptions.assign(outputs, outputs + count);
    state.sample_rate = rate;
    return state.subscription_success;
  }
  void attachCallback(bsecCallback value) { callback = value; }
  bool run() {
    auto& state = BsecMockState::instance();
    if (!state.run_success) return false;
    if (state.fresh_data && callback) {
      bsecOutputs outputs{{{BSEC_OUTPUT_RAW_PRESSURE, state.pressure, 3},
        {BSEC_OUTPUT_IAQ, state.iaq, state.accuracy},
        {BSEC_OUTPUT_BREATH_VOC_EQUIVALENT, state.voc, state.accuracy},
        {BSEC_OUTPUT_CO2_EQUIVALENT, state.eco2, state.accuracy}}, 4};
      callback({state.pressure}, outputs, *this);
    }
    return true;
  }
private:
  bsecCallback callback = nullptr;
};
