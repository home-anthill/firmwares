#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include "Wire.h"

inline void errorToString(int16_t error, char* message, size_t size) {
  snprintf(message, size, "sensor error %d", error);
}

struct Scd43MockState {
  TwoWire* bus = nullptr;
  uint8_t address = 0;
  int16_t wake_error = 0;
  int16_t stop_error = 0;
  int16_t serial_error = 0;
  int16_t start_error = 0;
  int16_t status_error = 0;
  int16_t read_error = 0;
  bool data_ready = true;
  uint16_t co2 = 800;
  int status_count = 0;
  int read_count = 0;
  std::vector<std::string> operations;
  static Scd43MockState& instance() { static Scd43MockState state; return state; }
  static void reset() { instance() = Scd43MockState{}; }
};

class SensirionI2cScd4x {
public:
  void begin(TwoWire& bus, uint8_t address) {
    auto& state = Scd43MockState::instance();
    state.bus = &bus; state.address = address;
    state.operations.push_back("begin");
  }
  int16_t wakeUp() { return operation("wake", Scd43MockState::instance().wake_error); }
  int16_t stopPeriodicMeasurement() { return operation("stop", Scd43MockState::instance().stop_error); }
  int16_t getSerialNumber(uint64_t& serial) {
    serial = 0x123456789ABC;
    return operation("serial", Scd43MockState::instance().serial_error);
  }
  int16_t startPeriodicMeasurement() { return operation("start", Scd43MockState::instance().start_error); }
  int16_t getDataReadyStatus(bool& ready) {
    auto& state = Scd43MockState::instance();
    ++state.status_count;
    ready = state.data_ready;
    return state.status_error;
  }
  int16_t readMeasurement(uint16_t& co2, float& temperature, float& humidity) {
    auto& state = Scd43MockState::instance();
    ++state.read_count;
    state.data_ready = false;
    co2 = state.co2; temperature = 22; humidity = 50;
    return state.read_error;
  }
private:
  int16_t operation(const char* name, int16_t error) {
    Scd43MockState::instance().operations.push_back(name);
    return error;
  }
};
