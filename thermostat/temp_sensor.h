#pragma once

struct TemperatureReading {
  float value;
  // No accepted measurement yet, or the cached value reached its age limit.
  bool expired;
};

void temp_init_sensor();
TemperatureReading temp_get_temperature();
