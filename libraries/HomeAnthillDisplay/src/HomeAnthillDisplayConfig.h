#pragma once

#include <HomeAnthillDisplay.h>

inline ha::DisplayConfig display_config = [] {
  ha::DisplayConfig config;
#ifdef HOME_ANTHILL_TEST_OLED_DISPLAY
  config.enabled = true;
#elif defined(OLED_DISPLAY)
  config.enabled = OLED_DISPLAY;
#else
  config.enabled = false;
#endif
#ifdef DISPLAY_BUTTON_ENABLED
  config.button_enabled = DISPLAY_BUTTON_ENABLED;
#endif
#ifdef DISPLAY_BUTTON_PIN
  config.button_pin = DISPLAY_BUTTON_PIN;
#endif
  config.sda = I2C_SDA;
  config.scl = I2C_SCL;
#ifdef OLED_RESET
  config.reset_pin = OLED_RESET;
#endif
  return config;
}();
