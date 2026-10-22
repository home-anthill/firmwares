#pragma once

#include "Adafruit_MCP9600.h"

class Adafruit_MCP9601 : public Adafruit_MCP9600 {
public:
  Adafruit_MCP9601() { _device_id = 0x41; }
};

#define MCP9601_STATUS_OPENCIRCUIT 0x10
#define MCP9601_STATUS_SHORTCIRCUIT 0x20
