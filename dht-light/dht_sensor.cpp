// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>

// include specific libraries:
// - DHT Sensor: https://github.com/adafruit/DHT-sensor-library
// - Adafruit Unified Sensor: https://github.com/adafruit/Adafruit_Sensor
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHT_PIN 4 // Digital pin connected to the DHT sensor
#define DHT_TYPE DHT22 // DHT 22 (AM2302)
DHT_Unified dht(DHT_PIN, DHT_TYPE);

void dht_init_sensor() {
  Serial.println("dht_init_sensor - called");
  // Initialize DHT device
  dht.begin();
  sensor_t sensor;
  // Print temperature sensor details.
  dht.temperature().getSensor(&sensor);
  Serial.println("dht_init_sensor - temperature");
  Serial.printf("dht_init_sensor - temperature - Sensor Type: %s\n", sensor.name);
  Serial.printf("dht_init_sensor - temperature - Driver Ver: %d\n", sensor.version);
  Serial.printf("dht_init_sensor - temperature - Unique ID: %d\n", sensor.sensor_id);
  Serial.printf("dht_init_sensor - temperature - Max Value: %.2f °C\n", sensor.max_value);
  Serial.printf("dht_init_sensor - temperature - Min Value: %.2f °C\n", sensor.min_value);
  Serial.printf("dht_init_sensor - temperature - Resolution: %.2f °C\n", sensor.resolution);
  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  Serial.println("dht_init_sensor - humidity");
  Serial.printf("dht_init_sensor - humidity - Sensor Type: %s\n", sensor.name);
  Serial.printf("dht_init_sensor - humidity - Driver Ver: %d\n", sensor.version);
  Serial.printf("dht_init_sensor - humidity - Unique ID: %d\n", sensor.sensor_id);
  Serial.printf("dht_init_sensor - humidity - Max Value: %.2f %\n", sensor.max_value);
  Serial.printf("dht_init_sensor - humidity - Min Value: %.2f %\n", sensor.min_value);
  Serial.printf("dht_init_sensor - humidity - Resolution: %.2f %\n", sensor.resolution);
}

float dht_get_temperature() {
  sensors_event_t event;
  bool isEventFetched = dht.temperature().getEvent(&event);
  if (!isEventFetched) {
    return NAN;
  }
  return event.temperature;
}

float dht_get_humidity() {
  sensors_event_t event;
  bool isEventFetched = dht.humidity().getEvent(&event);
  if (!isEventFetched) {
    return NAN;
  }
  return event.relative_humidity;
}