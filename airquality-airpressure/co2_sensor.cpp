#include "config.h"
#include "secrets.h"
#include "co2_sensor.h"

#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include <math.h>

constexpr uint8_t SCD43_ADDRESS = 0x62;
static SensirionI2cScd4x scd43;
static bool measuring = false;

void co2_init_sensor() {
  measuring = false;
  if (!Wire.begin(I2C_SDA, I2C_SCL, 100000)) {
    Serial.println("co2_init_sensor - I2C initialization failed");
    return;
  }
  scd43.begin(Wire, SCD43_ADDRESS);
  delay(50);
  // The sensor may still be measuring after only the ESP32 was reset.
  int16_t error = scd43.wakeUp();
  if (error != 0) {
    char message[64];
    errorToString(error, message, sizeof(message));
    Serial.printf("co2_sensor - wake up (may already be awake): %s (%d)\n", message, error);
  }
  error = scd43.stopPeriodicMeasurement();
  if (error != 0) {
    char message[64];
    errorToString(error, message, sizeof(message));
    Serial.printf("co2_sensor - stop previous measurement: %s (%d)\n", message, error);
  }
  // Library v1.1.0 already waits 500 ms inside stopPeriodicMeasurement().
  uint64_t serial_number = 0;
  error = scd43.getSerialNumber(serial_number);
  if (error != 0) {
    char message[64];
    errorToString(error, message, sizeof(message));
    Serial.printf("co2_sensor - read serial number: %s (%d)\n", message, error);
  } else {
    Serial.printf("co2_init_sensor - SCD43 address=0x%02x serial=0x%04lx%08lx\n",
                  SCD43_ADDRESS, static_cast<unsigned long>(serial_number >> 32),
                  static_cast<unsigned long>(serial_number & 0xFFFFFFFF));
  }
  error = scd43.startPeriodicMeasurement();
  if (error != 0) {
    char message[64];
    errorToString(error, message, sizeof(message));
    Serial.printf("co2_sensor - start periodic measurement: %s (%d)\n", message, error);
    return;
  }
  measuring = true;
  Serial.println("co2_init_sensor - periodic measurement started; first sample in about 5s");
}

float co2_read() {
  if (!measuring) {
    return NAN;
  }
  bool data_ready = false;
  int16_t error = scd43.getDataReadyStatus(data_ready);
  if (error != 0) {
    char message[64];
    errorToString(error, message, sizeof(message));
    Serial.printf("co2_sensor - read data-ready status: %s (%d)\n", message, error);
    return NAN;
  }
  if (!data_ready) {
    return NAN;
  }
  uint16_t co2 = 0;
  float temp = 0;
  float hum = 0;
  error = scd43.readMeasurement(co2, temp, hum);
  if (error != 0) {
    char message[64];
    errorToString(error, message, sizeof(message));
    Serial.printf("co2_sensor - read measurement: %s (%d)\n", message, error);
    return NAN;
  }
  if (co2 == 0 || co2 > 40000) {
    Serial.println("co2_read - CO2 sample is not valid");
    return NAN;
  }
  Serial.printf("co2_read - CO2=%u ppm, temp=%.2f °C, hum=%.2f %%RH\n", co2, temp, hum);
  return static_cast<float>(co2);
}
