#pragma once

// MCP9601 slave bus connected to thermocouple-mcp9601.
#define I2C_SDA_PIN 39
#define I2C_SCL_PIN 40

// Separate master bus for the optional local OLED.
#define OLED_SDA_PIN 8
#define OLED_SCL_PIN 9

// Default ESP32 DevModule C configuration
#define RGB_LED_PIN 48

// GPIO buttons
#define BUTTON_PLUS_01_PIN 4
#define BUTTON_PLUS_05_PIN 5
#define BUTTON_MINUS_01_PIN 6
#define BUTTON_MINUS_05_PIN 7
// Status button cycles temperatureStatus 0 through 5
#define BUTTON_STATUS_PIN 10
