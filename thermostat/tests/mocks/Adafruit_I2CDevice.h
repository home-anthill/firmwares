#pragma once

// ---------------------------------------------------------------------------
// Adafruit_I2CDevice stub for host-side (native) unit test compilation.
// Included by temp_sensor.cpp; no functionality is exercised in tests.
// Pulls in Wire.h so that the global Wire instance is visible to any
// translation unit that includes this header (matching real Adafruit behaviour).
// ---------------------------------------------------------------------------

#include <cstdint>
#include "Wire.h"
