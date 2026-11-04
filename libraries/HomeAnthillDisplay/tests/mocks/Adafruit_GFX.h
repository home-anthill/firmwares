#pragma once

// ---------------------------------------------------------------------------
// Adafruit_GFX mock for host-side (native) unit test compilation.
// Provides the base class that Adafruit_SSD1306 inherits from.
// ---------------------------------------------------------------------------

#include <cstdint>

#define WHITE 1

class Adafruit_GFX {
public:
  Adafruit_GFX(int16_t /*w*/, int16_t /*h*/) {}
  virtual ~Adafruit_GFX() = default;

  virtual void setTextSize(uint8_t /*s*/)      {}
  virtual void setTextColor(uint16_t /*c*/)    {}
  virtual void cp437(bool /*x*/)               {}
  virtual void setCursor(int16_t /*x*/, int16_t /*y*/) {}
  virtual void print(const char* /*s*/)        {}
  virtual void print(float /*v*/)              {}
};
