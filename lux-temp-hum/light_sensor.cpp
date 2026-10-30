#include "config.h"
#include "secrets.h"

// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>
// include library to config  ure I2C port
#include <Wire.h>

// include specific libraries:
// `Adafruit TSL2591 Library` by Adafruit.
// You cannot use the one published on ArduinoIDE Library Manager, because it's outdated and not compatibile with ESP32 devices.
// This sensor requires I2C port, so you need to import `Wire.h` to configure I2C
#include <Adafruit_TSL2591.h>


 // pass in a number for the sensor identifier
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);

void light_init_sensor() {
  Serial.println("light_init_sensor - called");
  Wire.setPins(I2C_SDA, I2C_SCL);
  Wire.begin();
  tsl.begin();
  sensor_t sensor;
  tsl.getSensor(&sensor);
  Serial.println("light_init_sensor");
  Serial.printf("light_init_sensor - Sensor Type: %s\n", sensor.name);
  Serial.printf("light_init_sensor - Driver Ver: %d\n", sensor.version);
  Serial.printf("light_init_sensor - Unique ID: %d\n", sensor.sensor_id);
  Serial.printf("light_init_sensor - Max Value: %.2f lux\n", sensor.max_value);
  Serial.printf("light_init_sensor - Min Value: %.2f lux\n", sensor.min_value);
  Serial.printf("light_init_sensor - Resolution: %.2f lux\n", sensor.resolution);
}

long light_get_value() {
  uint32_t lum = tsl.getFullLuminosity();
  uint16_t ir, full;
  ir = lum >> 16;
  full = lum & 0xFFFF;
  float lux = tsl.calculateLux(full, ir);
  Serial.print(F("[ ")); Serial.print(millis()); Serial.print(F(" ms ] "));
  Serial.print(F("IR: ")); Serial.print(ir);  Serial.print(F("  "));
  Serial.print(F("Full: ")); Serial.print(full); Serial.print(F("  "));
  Serial.print(F("Visible: ")); Serial.print(full - ir); Serial.print(F("  "));
  Serial.print(F("Lux: ")); Serial.println(lux, 6);
  return (long)ceil(lux);
}