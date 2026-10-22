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
#define IR_SEND_PIN 4
