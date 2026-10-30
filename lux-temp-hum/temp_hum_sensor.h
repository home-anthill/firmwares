#pragma once

struct Sht45Reading {
  float temperature;
  float humidity;
};

void sht45_init_sensor();

Sht45Reading sht45_read();
