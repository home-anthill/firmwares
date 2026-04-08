#pragma once

// ---------------------------------------------------------------------------
// Adafruit Unified Sensor mock for host-side (native) unit test compilation.
// Provides only the structs used by dht_sensor.cpp.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <cstring>

struct sensor_t {
  char    name[12];
  int32_t version;
  int32_t sensor_id;
  float   max_value;
  float   min_value;
  float   resolution;
};

struct sensors_event_t {
  float temperature;
  float relative_humidity;
};
