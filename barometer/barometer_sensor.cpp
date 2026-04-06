#include <math.h>
// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>
// include library to configure I2C port
#include <Wire.h>

// include specific libraries:
// `XENSIV Digital Pressure Sensor` by Infineon Technologies
#include <Dps3xx.h>

// Configure I2C GPIOs
#define I2C_SDA 39
#define I2C_SCL 40
// global variables
Dps3xx Dps3xxPressureSensor = Dps3xx();

void barometer_init_sensor() {
  Serial.println("barometer_init_sensor - called");
  Wire.setPins(I2C_SDA, I2C_SCL);
  Dps3xxPressureSensor.begin(Wire);
  Serial.println("barometer_init_sensor - sensor initialized successfully!");
}

float barometer_get_temperature() {
  Serial.println("barometer_get_temperature - called");
  float temperature;
  if (Dps3xxPressureSensor.measureTempOnce(temperature) == 0) {
    Serial.printf("barometer_get_temperature - temperature = %.2f °C\n", temperature);
    return temperature;
  }
  return NAN;
}

float barometer_get_airpressure() {
  Serial.println("barometer_get_airpressure - called");
  float pressure;
  if (Dps3xxPressureSensor.measurePressureOnce(pressure) == 0) {
    pressure = pressure / 1000;
    Serial.printf("barometer_get_airpressure - pressure = %.2f hPAa\n", pressure);
    return pressure;
  }
  return NAN;
}