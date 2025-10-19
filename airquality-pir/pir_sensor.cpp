// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>

#define MOTION_PIN 5 // Digital pin connected to the motion sensor
// global variable used to store the current value read by the sensor
int current_pir_value = -1; // initial uninitialized value

void pir_init_sensor() {
  Serial.println("pir_init_sensor - called");
  pinMode(MOTION_PIN,INPUT);
  Serial.println("pir_init_sensor - pin configured as input to read values");
}

int pir_get_value() {
  Serial.println("pir_get_value - called");
  int new_value = digitalRead(MOTION_PIN); 
  Serial.printf("pir_get_value - digital value read = %d\n", new_value);
  if (current_pir_value != new_value) {
    current_pir_value = new_value;
  }
  return new_value;
}

int pir_get_prev_value() {
  return current_pir_value;
}