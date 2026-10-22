#pragma once

struct Sen65Reading {
  float pm1;
  float pm25;
  float pm4;
  float pm10;
  float humidity;
  float temperature;
  float voc_index;
  float nox_index;
};

void sen65_init_sensor();
Sen65Reading sen65_read();
