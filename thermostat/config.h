#pragma once

// OLED display support. When OLED_DISPLAY=false, OLED code is not compiled.
// Use OLED_RESET=-1 to connect OLED reset to the board reset.
#define OLED_DISPLAY true
#define OLED_RESET -1 

// Optional GPIO button for display power saving/restart controls.
// When false, the display stays on and the button pin is not configured.
#define DISPLAY_BUTTON_ENABLED true
#define DISPLAY_BUTTON_PIN 42

// I2C configuration
#define I2C_SDA 39
#define I2C_SCL 40

// Default ESP32 DevModule C configuration
#define BOARD_RGB_LED_PIN 38

// Specific device configuration
// Output GPIOs. Preserve the external inactive-level bias on each output.
// ESP32-S2 / ESP32-S3 mapping (also used by host tests).
#define HEAT 5
#define COLD 6
#define FAN 7
#define PUMP 15

// Thermostat operating mode: 0 = cooling, 1 = heating.
#ifndef OPERATING_MODE
#define OPERATING_MODE 0
#endif

// Relay/output polarity. true = active LOW, false = active HIGH.
#define HOT_ACTIVE_LOW false
#define COLD_ACTIVE_LOW false
#define FAN_ACTIVE_LOW false
#define PUMP_ACTIVE_LOW false

// specific thermostat config
#define THERMOCOUPLE_TYPE "K"

// Keep FAN physically on for this many seconds after thermostat logic turns it off.
#define FAN_TURN_OFF_DELAY_SECONDS 240

// Cooling safety checks:
// Cooling safety checks used only when OPERATING_MODE is 0. These checks do
// not require a specific cooling rate. They only stop COLD if temperature has
// risen since cooling started or during a later wide monitoring window.
// Set either interval to 0 to disable that check independently.
#define COOLING_SHORT_RISE_CHECK_SECONDS 0 // disabled (by default)
#define COOLING_WIDE_RISE_CHECK_SECONDS 0 // disabled (by default)
// #define COOLING_SHORT_RISE_CHECK_SECONDS 120
// #define COOLING_WIDE_RISE_CHECK_SECONDS 600