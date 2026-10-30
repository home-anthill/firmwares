#pragma once

void temp_init_sensor();

// Numeric values are the MQTT temperature-error alarm contract.
enum TempSensorError {
  TEMP_SENSOR_OK = 0,
  TEMP_SENSOR_INIT_ERROR = 1,
  TEMP_SENSOR_OPEN_CIRCUIT = 2,
  TEMP_SENSOR_SHORT_CIRCUIT = 3,
  TEMP_SENSOR_OPEN_AND_SHORT = 4,
  TEMP_SENSOR_INVALID_READING = 5
};
struct TempSensorReading {
  // NAN when error is not TEMP_SENSOR_OK.
  float temperature;
  TempSensorError error;
};

TempSensorReading temp_get_temperature();
