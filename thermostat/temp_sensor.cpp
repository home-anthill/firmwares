// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>

// Temperature sensor DS18B20
// include specific libraries:
// - OneWire: https://github.com/PaulStoffregen/OneWire
// - DallasTemperature: https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 9
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void temp_init_sensor() {
  Serial.println("temp_init_sensor - called");
  // init temperature sensor
  sensors.begin();

  // // Print temperature sensor details.
  // temp.temperature().getSensor(&sensor);
  // Serial.println(F("temp_init_sensor - temperature"));
  // Serial.print(F("temp_init_sensor - temperature - Sensor Type: "));
  // Serial.println(sensor.name);
  // Serial.print(F("temp_init_sensor - temperature - Driver Ver:  "));
  // Serial.println(sensor.version);
  // Serial.print(F("temp_init_sensor - temperature - Unique ID:   "));
  // Serial.println(sensor.sensor_id);
  // Serial.print(F("temp_init_sensor - temperature - Max Value:   "));
  // Serial.print(sensor.max_value); Serial.println(F("°C"));
  // Serial.print(F("temp_init_sensor - temperature - Min Value:   "));
  // Serial.print(sensor.min_value); Serial.println(F("°C"));
  // Serial.print(F("temp_init_sensor - temperature - Resolution:  "));
  // Serial.print(sensor.resolution); Serial.println(F("°C"));
}

float temp_get_temperature() {
  sensors.requestTemperatures(); 
  float tempC = sensors.getTempCByIndex(0);
  if (tempC != DEVICE_DISCONNECTED_C) {
    Serial.print("Temperature: ");
    Serial.print(tempC);
    Serial.println("°C");
  } else {
    Serial.println("Cannot read temperature");
  }
  return tempC;
}