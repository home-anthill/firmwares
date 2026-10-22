#pragma once

struct PhReading {
  float voltage;
  float ph;
  bool calibrated;
  bool stable;
  bool calibrating;
};

void ph_init_sensor();
void ph_sensor_update();
// Invalid, stale, uncalibrated, or calibration-in-progress samples have NAN pH.
PhReading ph_read();
