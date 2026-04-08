#pragma once

// ---------------------------------------------------------------------------
// DHT mock — defines DHT_TYPE constants only.
// DHT_Unified and the sensor adapters live in DHT_U.h (matching the real
// Adafruit split), which this header pulls in for convenience.
// ---------------------------------------------------------------------------

#include "Adafruit_Sensor.h"

#define DHT11  11
#define DHT22  22
#define DHT21  21
