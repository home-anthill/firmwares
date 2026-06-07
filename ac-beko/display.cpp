#include "secrets.h"

#ifdef HOME_ANTHILL_TEST_OLED_DISPLAY
#undef OLED_DISPLAY
#define OLED_DISPLAY true
#endif

#ifndef OLED_DISPLAY
#define OLED_DISPLAY false
#endif

#if OLED_DISPLAY == true

// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>

// Display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdio.h>

#include "feature_values.h"

// Configure I2C for Display
// OLED GND --> GND
// OLED VCC --> 3.3V
// OLED SCL --> GPIO_40
// OLED SDA --> GPIO_39
#define I2C_SDA 39
#define I2C_SCL 40
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool showDisplay = true;
size_t display_feature_index = 0;

static void copy_unit_for_display(char* dest, size_t dest_len, const char* unit) {
  if (dest_len == 0) {
    return;
  }

  if (unit == nullptr) {
    dest[0] = '\0';
    return;
  }

  size_t in_pos = 0;
  size_t out_pos = 0;
  while (unit[in_pos] != '\0' && out_pos + 1 < dest_len) {
    const unsigned char current = static_cast<unsigned char>(unit[in_pos]);
    const unsigned char next = static_cast<unsigned char>(unit[in_pos + 1]);
    if (current == 0xC2 && next == 0xB0) {
      dest[out_pos++] = static_cast<char>(0xF8);
      in_pos += 2;
      continue;
    }

    dest[out_pos++] = unit[in_pos++];
  }
  dest[out_pos] = '\0';
}

void init_display() {
  // init display
  Wire.setPins(I2C_SDA, I2C_SCL);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  // Address 0x3C for 128x32
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    showDisplay = false;
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.cp437(true);
  display.setCursor(0, 0);
  display.print("Starting");
  display.display();
}

void update_display() {
  if (!showDisplay) {
    return;
  }

  size_t count = feature_values_count();
  if (count == 0) {
    return;
  }

  FeatureValue value = {};
  bool found_value = false;
  for (size_t i = 0; i < count; i++) {
    size_t index = (display_feature_index + i) % count;
    if (feature_values_get(index, &value) && value.has_value) {
      display_feature_index = (index + 1) % count;
      found_value = true;
      break;
    }
  }

  if (!found_value) {
    return;
  }

  char display_unit[16];
  char value_line[32];
  copy_unit_for_display(display_unit, sizeof(display_unit), value.unit);
  snprintf(value_line, sizeof(value_line), "%.1f %s", value.value, display_unit);

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.cp437(true);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(value.name);
  display.setTextSize(2);
  display.setCursor(0,14);
  display.print(value_line);
  display.display();
}

#endif
