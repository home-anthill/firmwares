#include "secrets.h"

#ifdef HOME_ANTHILL_TEST_OLED_DISPLAY
#undef OLED_DISPLAY
#define OLED_DISPLAY true
#endif

#ifndef OLED_DISPLAY
#define OLED_DISPLAY false
#endif

#ifndef DISPLAY_BUTTON_ENABLED
#define DISPLAY_BUTTON_ENABLED true
#endif

#if OLED_DISPLAY == true

// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>

// Display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdio.h>
#include <string.h>

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
bool display_powered = true;
size_t display_feature_index = 0;
bool display_wifi_connected = true;
bool display_mqtt_connected = true;
bool display_current_valid = false;
bool display_current_error = false;
bool display_current_message = false;
size_t display_current_feature_index = 0;
float display_current_value = 0.0f;
bool display_has_rendered = false;
unsigned long display_last_render_ms = 0;
unsigned long display_keep_on_until_ms = 0;
unsigned long display_force_off_until_ms = 0;
unsigned long display_connectivity_notice_until_ms = 0;
uint8_t display_button_pin = 255;
int display_button_last_read = HIGH;
unsigned long display_button_last_change_ms = 0;
bool display_button_pressed = false;
bool display_button_long_press_handled = false;
unsigned long display_button_pressed_at_ms = 0;
#ifdef HOME_ANTHILL_TEST_OLED_DISPLAY
bool display_button_enabled_for_test = DISPLAY_BUTTON_ENABLED == true;
#endif

const unsigned long DISPLAY_PERSISTENCE_MS = 1500;
const unsigned long DISPLAY_AUTO_OFF_MS = 30000;
const unsigned long DISPLAY_CONNECTIVITY_NOTICE_MS = 10000;
const unsigned long BUTTON_LONG_PRESS_MS = 10000;
const unsigned long BUTTON_DEBOUNCE_MS = 50;

static bool display_deadline_active(unsigned long deadline_ms) {
  return deadline_ms != 0 && static_cast<long>(deadline_ms - millis()) > 0;
}

static bool display_button_enabled() {
#ifdef HOME_ANTHILL_TEST_OLED_DISPLAY
  return display_button_enabled_for_test;
#else
  return DISPLAY_BUTTON_ENABLED == true;
#endif
}

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

static bool display_has_connectivity_error() {
  return !display_wifi_connected || !display_mqtt_connected;
}

static bool display_feature_is_visible(const FeatureValue& value) {
  return strcmp(value.name, "temperature") == 0;
}

static void display_power_on() {
  if (!showDisplay || display_powered) {
    return;
  }

  display.ssd1306_command(SSD1306_DISPLAYON);
  display_powered = true;
  display_has_rendered = false;
  display_last_render_ms = 0;
}

static void display_power_off() {
  if (!showDisplay || !display_powered || !display_button_enabled()) {
    return;
  }

  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  display_powered = false;
  display_has_rendered = false;
  display_current_valid = false;
}

static void render_connectivity_error() {
  const char* title = display_wifi_connected ? "MQTT error" : "WiFi status";
  const char* detail = display_wifi_connected ? "Disconnected" : "Offline";

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.cp437(true);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(title);
  display.setTextSize(2);
  display.setCursor(0, 14);
  display.print(detail);
  display.display();
  display_has_rendered = true;
  display_current_valid = true;
  display_current_error = true;
  display_current_message = false;
  display_last_render_ms = millis();
}

static void render_message(const char* title, const char* detail) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.cp437(true);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(title == nullptr ? "Message" : title);
  display.setTextSize(2);
  display.setCursor(0, 14);
  display.print(detail == nullptr ? "Received" : detail);
  display.display();
  display_has_rendered = true;
  display_current_valid = true;
  display_current_error = false;
  display_current_message = true;
  display_last_render_ms = millis();
}

static void render_feature_value(const FeatureValue& value) {
  char display_unit[16];
  char value_line[32];
  copy_unit_for_display(display_unit, sizeof(display_unit), value.unit);
  if (display_unit[0] == '\0' || (display_unit[0] == '-' && display_unit[1] == '\0')) {
    snprintf(value_line, sizeof(value_line), "%.1f", value.value);
  } else {
    snprintf(value_line, sizeof(value_line), "%.1f %s", value.value, display_unit);
  }

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
  display_has_rendered = true;
  display_current_valid = true;
  display_current_error = false;
  display_current_message = false;
  display_current_value = value.value;
  display_last_render_ms = millis();
}

static void render_next_display_slot() {
  if (!showDisplay || !display_powered) {
    return;
  }

  size_t count = feature_values_count();
  if (count == 0) {
    display_current_valid = false;
    return;
  }

  for (size_t i = 0; i < count; i++) {
    size_t slot = (display_feature_index + i) % count;
    FeatureValue value = {};
    if (feature_values_get(slot, &value) && value.has_value && display_feature_is_visible(value)) {
      display_feature_index = (slot + 1) % count;
      display_current_feature_index = slot;
      render_feature_value(value);
      return;
    }
  }

  display_current_valid = false;
}

static bool render_display_slot_by_name(const char* name) {
  if (!showDisplay || !display_powered || name == nullptr) {
    return false;
  }

  size_t count = feature_values_count();
  for (size_t i = 0; i < count; i++) {
    FeatureValue value = {};
    if (feature_values_get(i, &value) &&
        value.has_value &&
        strcmp(value.name, name) == 0) {
      display_feature_index = (i + 1) % count;
      display_current_feature_index = i;
      render_feature_value(value);
      return true;
    }
  }

  return false;
}

static void render_default_display_slot() {
  if (!display_button_enabled() && render_display_slot_by_name("temperature")) {
    return;
  }

  render_next_display_slot();
}

static void render_connectivity_fallback_slot() {
  if (render_display_slot_by_name("temperature")) {
    return;
  }

  render_next_display_slot();
}

static bool refresh_current_display_slot() {
  if (!showDisplay ||
      !display_powered ||
      !display_current_valid ||
      display_current_error ||
      display_current_message) {
    return false;
  }

  FeatureValue value = {};
  if (!feature_values_get(display_current_feature_index, &value) ||
      !value.has_value ||
      !display_feature_is_visible(value)) {
    display_current_valid = false;
    return false;
  }

  if (value.value == display_current_value) {
    return false;
  }

  render_feature_value(value);
  return true;
}

static void handle_display_short_press() {
  if (!showDisplay || !display_button_enabled()) {
    return;
  }

  display_force_off_until_ms = 0;
  display_keep_on_until_ms = millis() + DISPLAY_AUTO_OFF_MS;
  display_power_on();
  render_next_display_slot();
}

static void handle_display_button() {
  if (!display_button_enabled() || display_button_pin == 255) {
    return;
  }

  int reading = digitalRead(display_button_pin);
  unsigned long now = millis();
  if (reading != display_button_last_read) {
    display_button_last_read = reading;
    display_button_last_change_ms = now;
  }

  if (now - display_button_last_change_ms < BUTTON_DEBOUNCE_MS) {
    return;
  }

  bool pressed = reading == LOW;
  if (pressed && !display_button_pressed) {
    display_button_pressed = true;
    display_button_long_press_handled = false;
    display_button_pressed_at_ms = now;
  }

  if (pressed &&
      display_button_pressed &&
      !display_button_long_press_handled &&
      now - display_button_pressed_at_ms >= BUTTON_LONG_PRESS_MS) {
    display_button_long_press_handled = true;
    ESP.restart();
    return;
  }

  if (!pressed && display_button_pressed) {
    bool was_long_press = display_button_long_press_handled ||
        now - display_button_pressed_at_ms >= BUTTON_LONG_PRESS_MS;
    display_button_pressed = false;
    display_button_long_press_handled = false;
    if (!was_long_press) {
      handle_display_short_press();
    }
  }
}

void init_display(uint8_t button_pin) {
  display_button_pin = display_button_enabled() ? button_pin : 255;
  if (display_button_enabled()) {
    pinMode(display_button_pin, INPUT_PULLUP);
    display_button_last_read = digitalRead(display_button_pin);
  } else {
    display_button_last_read = HIGH;
  }
  display_button_last_change_ms = millis();
  display_button_pressed = false;
  display_button_long_press_handled = false;
  display_button_pressed_at_ms = 0;
  display_keep_on_until_ms = display_button_enabled() ? millis() + DISPLAY_AUTO_OFF_MS : 0;
  display_force_off_until_ms = 0;
  display_connectivity_notice_until_ms = 0;
  display_powered = true;
  display_has_rendered = false;
  display_current_valid = false;
  display_current_error = false;
  display_current_message = false;

  // init display
  Wire.setPins(I2C_SDA, I2C_SCL);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  // Address 0x3C for 128x32
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    showDisplay = false;
    display_powered = false;
    return;
  }

  showDisplay = true;
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.cp437(true);
  display.setCursor(0, 0);
  display.print("Starting");
  display.display();
  display_has_rendered = true;
  display_last_render_ms = millis();
}

void display_show_message(const char* title, const char* detail) {
  if (!showDisplay) {
    return;
  }

  display_force_off_until_ms = 0;
  if (!display_button_enabled()) {
    display_keep_on_until_ms = 0;
    display_power_on();
    render_message(title, detail);
    return;
  }

  if (display_button_enabled()) {
    display_keep_on_until_ms = millis() + DISPLAY_AUTO_OFF_MS;
  }
  display_power_on();
  render_message(title, detail);
}

void display_set_connectivity_status(bool wifi_connected, bool mqtt_connected) {
  bool status_changed = display_wifi_connected != wifi_connected ||
      display_mqtt_connected != mqtt_connected;
  display_wifi_connected = wifi_connected;
  display_mqtt_connected = mqtt_connected;
  bool has_error = display_has_connectivity_error();
  if (status_changed) {
    display_has_rendered = false;
    display_current_valid = false;
  }
  if (!has_error) {
    display_connectivity_notice_until_ms = 0;
  } else if (status_changed) {
    display_connectivity_notice_until_ms = millis() + DISPLAY_CONNECTIVITY_NOTICE_MS;
  }
}

void display_sleep_for(unsigned long duration_ms) {
  if (!showDisplay) {
    return;
  }

  if (!display_button_enabled()) {
    display_force_off_until_ms = 0;
    display_keep_on_until_ms = 0;
    display_power_on();
    return;
  }

  display_force_off_until_ms = millis() + duration_ms;
  display_keep_on_until_ms = 0;
  if (!display_has_connectivity_error()) {
    display_power_off();
  }
}

void update_display() {
  handle_display_button();

  if (!showDisplay) {
    return;
  }

  if (display_has_connectivity_error()) {
    display_force_off_until_ms = 0;
    display_power_on();
    if (display_deadline_active(display_connectivity_notice_until_ms)) {
      if (display_current_error && display_has_rendered) {
        return;
      }
      render_connectivity_error();
      return;
    }
    if (display_current_error || display_current_message || !display_current_valid) {
      render_connectivity_fallback_slot();
    } else {
      refresh_current_display_slot();
    }
    return;
  }

  if (!display_button_enabled()) {
    display_force_off_until_ms = 0;
    display_keep_on_until_ms = 0;
    display_power_on();
    if (display_current_message &&
        display_has_rendered &&
        millis() - display_last_render_ms < DISPLAY_PERSISTENCE_MS) {
      return;
    }
    render_default_display_slot();
    return;
  }

  if (display_deadline_active(display_force_off_until_ms)) {
    display_power_off();
    return;
  }
  display_force_off_until_ms = 0;

  if (!display_deadline_active(display_keep_on_until_ms)) {
    display_keep_on_until_ms = 0;
    display_power_off();
    return;
  }

  display_power_on();
  if (display_current_message &&
      display_has_rendered &&
      millis() - display_last_render_ms >= DISPLAY_PERSISTENCE_MS) {
    render_default_display_slot();
    return;
  }
  refresh_current_display_slot();
}

void display_force_update() {
  if (!showDisplay) {
    return;
  }

  if (display_button_enabled() &&
      display_deadline_active(display_force_off_until_ms) &&
      !display_has_connectivity_error()) {
    display_power_off();
    return;
  }

  if (display_button_enabled()) {
    display_keep_on_until_ms = millis() + DISPLAY_AUTO_OFF_MS;
  }
  display_power_on();
  if (display_has_connectivity_error() &&
      display_deadline_active(display_connectivity_notice_until_ms)) {
    render_connectivity_error();
    return;
  }

  if (display_has_connectivity_error()) {
    render_connectivity_fallback_slot();
  } else {
    render_default_display_slot();
  }
}

#endif
