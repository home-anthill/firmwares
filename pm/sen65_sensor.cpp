#include "config.h"
#include "sen65_sensor.h"

#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2cSen65.h>
#include <math.h>

static SensirionI2cSen65 sen65;
static bool sensor_started = false;

void sen65_init_sensor() {
  sensor_started = false;
  if (!Wire.begin(I2C_SDA, I2C_SCL)) {
    Serial.println("sen65_init_sensor - I2C initialization failed");
    return;
  }
  Wire.setClock(100000);
  Wire.setTimeOut(100);
  delay(200);
  Wire.beginTransmission(SEN65_I2C_ADDR_6B);
  const uint8_t bus_error = Wire.endTransmission();
  if (bus_error != 0) {
    Serial.printf("sen65_init_sensor - sensor not found at 0x6B: %u\n", bus_error);
    return;
  }
  sen65.begin(Wire, SEN65_I2C_ADDR_6B);
  int16_t error = sen65.deviceReset();
  if (error != 0) {
    char message[128] = {};
    errorToString(error, message, sizeof(message));
    Serial.printf("sen65_init_sensor - reset failed: %s (%d)\n", message, error);
    return;
  }
  delay(1200);
  error = sen65.startContinuousMeasurement();
  if (error != 0) {
    char message[128] = {};
    errorToString(error, message, sizeof(message));
    Serial.printf("sen65_init_sensor - start failed: %s (%d)\n", message, error);
    return;
  }
  sensor_started = true;
}

Sen65Reading sen65_read() {
  Sen65Reading reading{NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN};
  if (!sensor_started) {
    return reading;
  }
  const int16_t error = sen65.readMeasuredValues(
    reading.pm1, reading.pm25, reading.pm4, reading.pm10,
    reading.humidity, reading.temperature, reading.voc_index, reading.nox_index);
  if (error != 0) {
    char message[128] = {};
    errorToString(error, message, sizeof(message));
    Serial.printf("sen65_read - read failed: %s (%d)\n", message, error);
    return {NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN};
  }
  return reading;
}
