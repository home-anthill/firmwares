// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>
// include library to configure I2C port
#include <Wire.h>

// include specific libraries:
// `Grove - Digital Light sensor` by Seeed Studio (latest version on `master branch` or
// commit hash `69f7175ed1349276364994d1d45041c6e90a129b` from `https://github.com/Seeed-Studio/Grove_Digital_Light_Sensor`).
// You cannot use the one published on ArduinoIDE Library Manager, because it's outdated and not compatibile with ESP32 devices.
// This sensor requires I2C port, so you need to import `Wire.h` to configure I2C
#include <Digital_Light_TSL2561.h>

// Configure I2C GPIOs
#define I2C_SDA 39
#define I2C_SCL 40

void light_init_sensor() {
  Serial.println("light_init_sensor - called");
  Wire.setPins(I2C_SDA, I2C_SCL);
  Wire.begin();
  TSL2561.init();
  Serial.println("light_init_sensor - sensor initialized successfully!");
}

signed long light_get_value() {
  return TSL2561.readVisibleLux();
}