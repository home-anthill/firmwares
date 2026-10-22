#include "config.h"
#include "secrets.h"
#include "water_level_sensor.h"
#include <Arduino.h>

void water_level_init_sensor() {
  pinMode(WATER_LEVEL_PIN, INPUT);
  Serial.printf("water_level_init_sensor - GPIO%d configured as input\n", WATER_LEVEL_PIN);
}

bool water_level_read() {
  const bool above_threshold = (digitalRead(WATER_LEVEL_PIN) == HIGH) == WATER_LEVEL_ACTIVE_HIGH;
  Serial.printf("water_level_read - at or above threshold=%s\n", above_threshold ? "true" : "false");
  return above_threshold;
}
