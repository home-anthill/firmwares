#pragma once
#include <cstdint>
#include <vector>
#include "Arduino.h"

struct GateThreshold {
  uint8_t gate;
  uint8_t moving;
  uint8_t stationary;
};

struct RadarMockState {
  Stream* serial = nullptr;
  bool begin_success = true;
  bool configuration_success = true;
  bool max_values_success = true;
  bool moving = false;
  int configuration_requests = 0;
  int max_values_calls = 0;
  uint16_t max_moving = 0;
  uint16_t max_stationary = 0;
  uint16_t inactivity_timer = 0;
  std::vector<GateThreshold> thresholds;
  static RadarMockState& instance() { static RadarMockState state; return state; }
  static void reset() { instance() = RadarMockState{}; }
};

class ld2410 {
public:
  uint8_t max_moving_gate = 8;
  uint8_t max_stationary_gate = 8;
  uint8_t max_gate = 8;
  uint8_t motion_sensitivity[9] = {};
  uint8_t stationary_sensitivity[9] = {};

  bool begin(Stream& serial, bool = true) {
    RadarMockState::instance().serial = &serial;
    return RadarMockState::instance().begin_success;
  }
  bool requestCurrentConfiguration() {
    auto& state = RadarMockState::instance();
    ++state.configuration_requests;
    return state.configuration_success;
  }
  bool setMaxValues(uint16_t moving, uint16_t stationary, uint16_t timeout) {
    auto& state = RadarMockState::instance();
    ++state.max_values_calls;
    state.max_moving = moving;
    state.max_stationary = stationary;
    state.inactivity_timer = timeout;
    return state.max_values_success;
  }
  bool setGateSensitivityThreshold(uint8_t gate, uint8_t moving, uint8_t stationary) {
    RadarMockState::instance().thresholds.push_back({gate, moving, stationary});
    return true;
  }
  bool movingTargetDetected() { return RadarMockState::instance().moving; }
};
