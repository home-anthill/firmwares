#pragma once
#include <math.h>

struct BarometerReading {
  float airpressure = NAN;
  float altitude = NAN;
  float iaq = NAN;
  float voc = NAN;
  float eco2 = NAN;
};

void barometer_init_sensor();
void barometer_update();
BarometerReading barometer_read();
