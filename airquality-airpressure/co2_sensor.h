#pragma once

void co2_init_sensor();
// Returns a fresh CO2 value in ppm, or NAN on not-ready/zero/error samples.
float co2_read();
