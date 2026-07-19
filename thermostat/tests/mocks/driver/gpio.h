#pragma once

#include <Arduino.h>

using gpio_num_t = int;
using esp_err_t = int;

inline esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level) {
  auto& state = GpioMockState::instance();
  const uint8_t arduino_pin = static_cast<uint8_t>(pin);
  state.preloaded_values[arduino_pin] = static_cast<uint8_t>(level);
  return 0;
}
