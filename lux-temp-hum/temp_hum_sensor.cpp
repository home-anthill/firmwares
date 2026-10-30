#include "config.h"
#include "secrets.h"
#include "temp_hum_sensor.h"

#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2cSht4x.h>
#include <math.h>

constexpr uint8_t SHT45_ADDRESS = 0x44;

static SensirionI2cSht4x sht45;

void sht45_init_sensor() {
  Wire.setPins(I2C_SDA, I2C_SCL);
  Wire.begin();
  sht45.begin(Wire, SHT45_ADDRESS);
  int16_t error = sht45.softReset();
  if (error != 0) {
    Serial.printf("sht45_init_sensor - reset failed: %d\n", error);
  }
}

Sht45Reading sht45_read() {
  Sht45Reading reading{NAN, NAN};
  int16_t error = sht45.measureHighPrecision(reading.temperature, reading.humidity);
  if (error != 0 || !isfinite(reading.temperature) || !isfinite(reading.humidity)) {
    Serial.printf("sht45_read - invalid measurement, error: %d\n", error);
    return {NAN, NAN};
  }
  // Sensirion specifies clipping the converted RH signal to the physical range.
  reading.humidity = fmaxf(0.0f, fminf(100.0f, reading.humidity));
  return reading;
}
