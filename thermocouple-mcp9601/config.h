#pragma once

// OLED display support. When OLED_DISPLAY=false, OLED code is not compiled.
// Use OLED_RESET=-1 to connect OLED reset to the board reset.
#define OLED_DISPLAY false
#define OLED_RESET -1 

// Optional GPIO button for display power saving/restart controls.
// When false, the display stays on and the button pin is not configured.
#define DISPLAY_BUTTON_ENABLED false
#define DISPLAY_BUTTON_PIN 42

// I2C configuration
#define I2C_SDA 39
#define I2C_SCL 40

// Default ESP32 DevModule C configuration
#define BOARD_RGB_LED_PIN 38

// MCP9601 converter address.
#define THERMOCOUPLE_I2C_ADDRESS 0x67

// Probe type: K, J, T, N, S, E, B or R (uppercase character).
#ifndef THERMOCOUPLE_TYPE
#define THERMOCOUPLE_TYPE 'K'
#endif

// Map the configured probe type to the Adafruit driver constant.
#if THERMOCOUPLE_TYPE == 'K'
#define THERMOCOUPLE_DRIVER_TYPE MCP9600_TYPE_K
#elif THERMOCOUPLE_TYPE == 'J'
#define THERMOCOUPLE_DRIVER_TYPE MCP9600_TYPE_J
#elif THERMOCOUPLE_TYPE == 'T'
#define THERMOCOUPLE_DRIVER_TYPE MCP9600_TYPE_T
#elif THERMOCOUPLE_TYPE == 'N'
#define THERMOCOUPLE_DRIVER_TYPE MCP9600_TYPE_N
#elif THERMOCOUPLE_TYPE == 'S'
#define THERMOCOUPLE_DRIVER_TYPE MCP9600_TYPE_S
#elif THERMOCOUPLE_TYPE == 'E'
#define THERMOCOUPLE_DRIVER_TYPE MCP9600_TYPE_E
#elif THERMOCOUPLE_TYPE == 'B'
#define THERMOCOUPLE_DRIVER_TYPE MCP9600_TYPE_B
#elif THERMOCOUPLE_TYPE == 'R'
#define THERMOCOUPLE_DRIVER_TYPE MCP9600_TYPE_R
#else
#error "THERMOCOUPLE_TYPE must be K, J, T, N, S, E, B or R as an uppercase character"
#endif
