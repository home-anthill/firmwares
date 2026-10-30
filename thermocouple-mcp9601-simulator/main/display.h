#pragma once

#include <stdbool.h>
#include <stdint.h>

bool simulator_display_init(float temperature_celsius);
void simulator_display_update(float temperature_celsius, uint8_t temperature_status);

