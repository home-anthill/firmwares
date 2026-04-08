#pragma once

// ---------------------------------------------------------------------------
// DHT_Unified mock for host-side (native) unit test compilation.
//
// The singleton DhtMockState controls what getEvent() returns so that
// tests can exercise both the happy path and the read-failure path of
// dht_get_temperature() / dht_get_humidity() without real hardware.
//
// Usage in tests:
//   DhtMockState::reset();
//   DhtMockState::instance().temp_value       = 23.5f;
//   DhtMockState::instance().temp_event_result = true;
//   EXPECT_FLOAT_EQ(dht_get_temperature(), 23.5f);
// ---------------------------------------------------------------------------

#include "DHT.h"   // brings in Adafruit_Sensor.h and DHT_TYPE defines

#include <cstdint>
#include <cstring>

// ---------------------------------------------------------------------------
// Singleton mock state — reset in test SetUp() for isolation.
// ---------------------------------------------------------------------------
struct DhtMockState {
  bool  temp_event_result{true};
  float temp_value{0.0f};
  bool  hum_event_result{true};
  float hum_value{0.0f};

  static DhtMockState& instance() {
    static DhtMockState s;
    return s;
  }
  static void reset() { instance() = DhtMockState{}; }
};

// ---------------------------------------------------------------------------
// Sensor adapter — returned by DHT_Unified::temperature() / humidity().
// ---------------------------------------------------------------------------
class DhtSensorAdapter {
  bool is_temperature_;
public:
  explicit DhtSensorAdapter(bool is_temperature) : is_temperature_(is_temperature) {}

  void getSensor(sensor_t* s) { memset(s, 0, sizeof(sensor_t)); }

  bool getEvent(sensors_event_t* event) {
    if (is_temperature_) {
      event->temperature = DhtMockState::instance().temp_value;
      return DhtMockState::instance().temp_event_result;
    } else {
      event->relative_humidity = DhtMockState::instance().hum_value;
      return DhtMockState::instance().hum_event_result;
    }
  }
};

// ---------------------------------------------------------------------------
// DHT_Unified mock — mirrors the real class interface used by dht_sensor.cpp.
// ---------------------------------------------------------------------------
class DHT_Unified {
  DhtSensorAdapter temp_adapter_{true};
  DhtSensorAdapter hum_adapter_{false};
public:
  DHT_Unified(uint8_t /*pin*/, uint8_t /*type*/) {}

  void begin() {}

  DhtSensorAdapter& temperature() { return temp_adapter_; }
  DhtSensorAdapter& humidity()    { return hum_adapter_;  }
};
