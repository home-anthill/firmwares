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

#ifndef SEND_LG
#define SEND_LG 1
#endif
#include <ir_LG.h>

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

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

struct ConnectivityState {
  bool wifi_connected = true;
  bool mqtt_connected = true;
};

struct RenderState {
  size_t next_feature_index = 0;
  size_t current_feature_index = 0;
  float current_value = 0.0f;
  bool current_valid = false;
  bool current_error = false;
  bool current_message = false;
  bool has_rendered = false;
  unsigned long last_render_ms = 0;
};

struct ButtonState {
  uint8_t pin = 255;
  int last_read = HIGH;
  unsigned long last_change_ms = 0;
  bool pressed = false;
  bool long_press_handled = false;
  unsigned long pressed_at_ms = 0;
};

static bool available = true;
static bool powered = true;
static ConnectivityState connectivity = {};
static RenderState render_state = {};
static ButtonState button = {};
static unsigned long keep_on_until_ms = 0;
static unsigned long display_sleep_until_ms = 0;
#ifdef HOME_ANTHILL_TEST_OLED_DISPLAY
static bool display_button_enabled_for_test = DISPLAY_BUTTON_ENABLED == true;
#endif

static const unsigned long DISPLAY_PERSISTENCE_MS = 1500;
static const unsigned long DISPLAY_AUTO_OFF_MS = 30000;
static const unsigned long BUTTON_LONG_PRESS_MS = 10000;
static const unsigned long BUTTON_DEBOUNCE_MS = 50;

static bool deadline_active(unsigned long deadline_ms) {
  return deadline_ms != 0 && static_cast<long>(deadline_ms - millis()) > 0;
}

static bool button_enabled() {
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

static bool feature_is_visible(const FeatureValue& value) {
  return strcmp(value.name, "online") != 0;
}

static int display_int_value(float value) {
  return static_cast<int>(value + (value >= 0.0f ? 0.5f : -0.5f));
}

static const char* controller_display_label(const FeatureValue& value) {
  int current = display_int_value(value.value);
  if (strcmp(value.name, "on") == 0) {
    if (current == 1) {
      return "On";
    }
    if (current == 0) {
      return "Off";
    }
    return nullptr;
  }

  if (strcmp(value.name, "mode") == 0) {
    switch (current) {
      case kLgAcCool: return "Cool";
      case kLgAcDry: return "Dry";
      case kLgAcFan: return "Fan";
      case kLgAcAuto: return "Auto";
      case kLgAcHeat: return "Heat";
      default: return nullptr;
    }
  }

  if (strcmp(value.name, "fanSpeed") == 0) {
    switch (current) {
      case kLgAcFanHigh: return "Max";
      case kLgAcFanMedium: return "Med";
      case kLgAcFanLowest: return "Min";
      case kLgAcFanAuto: return "Auto";
      default: return nullptr;
    }
  }

  return nullptr;
}

static bool connectivity_has_error() {
  return !connectivity.wifi_connected || !connectivity.mqtt_connected;
}

static void invalidate_render() {
  render_state.has_rendered = false;
  render_state.current_valid = false;
}

static void mark_rendered(bool error, bool message) {
  render_state.has_rendered = true;
  render_state.current_valid = true;
  render_state.current_error = error;
  render_state.current_message = message;
  render_state.last_render_ms = millis();
}

static void begin_render() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.cp437(true);
}

static void render_two_line_screen(const char* title, const char* detail) {
  begin_render();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(title);
  display.setTextSize(2);
  display.setCursor(0, 14);
  display.print(detail);
  display.display();
}

static void power_on() {
  if (!available || powered) {
    return;
  }

  display.ssd1306_command(SSD1306_DISPLAYON);
  powered = true;
  render_state.has_rendered = false;
  render_state.last_render_ms = 0;
}

static void power_off() {
  if (!available || !powered || !button_enabled()) {
    return;
  }

  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  powered = false;
  invalidate_render();
}

static void render_connectivity_error() {
  const char* title = connectivity.wifi_connected ? "MQTT error" : "WiFi status";
  const char* detail = connectivity.wifi_connected ? "Disconnected" : "Offline";

  render_two_line_screen(title, detail);
  mark_rendered(true, false);
}

static void render_message(const char* title, const char* detail) {
  render_two_line_screen(
      title == nullptr ? "Message" : title,
      detail == nullptr ? "Received" : detail);
  mark_rendered(false, true);
}

static void render_feature_value(const FeatureValue& value) {
  char display_unit[16];
  char value_line[32];
  const char* label = controller_display_label(value);
  if (label != nullptr) {
    snprintf(value_line, sizeof(value_line), "%s", label);
  } else {
    copy_unit_for_display(display_unit, sizeof(display_unit), value.unit);
    if (display_unit[0] == '\0' || (display_unit[0] == '-' && display_unit[1] == '\0')) {
      // if unit is missing or '-' skip it while writing on display
      snprintf(value_line, sizeof(value_line), "%.1f", value.value);
    } else {
      snprintf(value_line, sizeof(value_line), "%.1f %s", value.value, display_unit);
    }
  }

  begin_render();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(value.name);
  display.setTextSize(2);
  display.setCursor(0,14);
  display.print(value_line);
  display.display();
  render_state.current_value = value.value;
  mark_rendered(false, false);
}

static void render_next_feature_value() {
  if (!available || !powered) {
    return;
  }

  size_t count = feature_values_count();
  if (count == 0) {
    render_state.current_valid = false;
    return;
  }

  for (size_t i = 0; i < count; i++) {
    // candidate_index means "the index in the feature list that we are trying to display now".
    // The % count is there to wrap around to the beginning of the list when we reach the end.
    // Example with 3 features:
    // count = 3;
    // render_state.next_feature_index = 2;

    // Loop values:
    // i = 0 -> candidate_index = (2 + 0) % 3 = 2
    // i = 1 -> candidate_index = (2 + 1) % 3 = 0
    // i = 2 -> candidate_index = (2 + 2) % 3 = 1

    // So it checks features in this order: 2, 0, 1
    size_t candidate_index = (render_state.next_feature_index + i) % count;
    FeatureValue value = {};
    if (feature_values_get(candidate_index, &value) && value.has_value && feature_is_visible(value)) {
      render_state.next_feature_index = (candidate_index + 1) % count;
      render_state.current_feature_index = candidate_index;
      render_feature_value(value);
      return;
    }
  }

  render_state.current_valid = false;
}

static bool refresh_current_feature_value() {
  if (!available ||
      !powered ||
      !render_state.current_valid ||
      render_state.current_error ||
      render_state.current_message) {
    return false;
  }

  FeatureValue value = {};
  if (!feature_values_get(render_state.current_feature_index, &value) ||
      !value.has_value ||
      !feature_is_visible(value)) {
    render_state.current_valid = false;
    return false;
  }

  if (value.value == render_state.current_value) {
    return false;
  }

  render_feature_value(value);
  return true;
}

static void handle_display_short_press() {
  if (!available || !button_enabled()) {
    return;
  }

  display_sleep_until_ms = 0;
  keep_on_until_ms = millis() + DISPLAY_AUTO_OFF_MS;
  power_on();
  render_next_feature_value();
}

static void handle_display_button() {
  if (!button_enabled() || button.pin == 255) {
    return;
  }

  int reading = digitalRead(button.pin);
  unsigned long now = millis();
  if (reading != button.last_read) {
    button.last_read = reading;
    button.last_change_ms = now;
  }

  // button debounce guard
  // Physical buttons do not switch cleanly from HIGH to LOW once. 
  // For a few milliseconds, the signal can bounce between HIGH and LOW.
  // This code ignores the button until the reading has been stable for at least BUTTON_DEBOUNCE_MS.
  if (now - button.last_change_ms < BUTTON_DEBOUNCE_MS) {
    return;
  }

  bool pressed = reading == LOW;
  if (pressed && !button.pressed) {
    button.pressed = true;
    button.long_press_handled = false;
    button.pressed_at_ms = now;
  }

  // If the button is long pressed we restart the device.
  if (pressed &&
      button.pressed &&
      !button.long_press_handled &&
      now - button.pressed_at_ms >= BUTTON_LONG_PRESS_MS) {
    button.long_press_handled = true;
    ESP.restart();
    return;
  }

  // if the button is short pressed we rotate feature values on display
  if (!pressed && button.pressed) {
    bool was_long_press = button.long_press_handled ||
        now - button.pressed_at_ms >= BUTTON_LONG_PRESS_MS;
    button.pressed = false;
    button.long_press_handled = false;
    if (!was_long_press) {
      handle_display_short_press();
    }
  }
}

static void update_connectivity_error_display() {
  display_sleep_until_ms = 0;
  power_on();
  if (render_state.current_error && render_state.has_rendered) {
    return;
  }
  render_connectivity_error();
}

static void update_always_on_display() {
  display_sleep_until_ms = 0;
  keep_on_until_ms = 0;
  power_on();
  if (render_state.current_message &&
      render_state.has_rendered &&
      millis() - render_state.last_render_ms < DISPLAY_PERSISTENCE_MS) {
    return;
  }
  if (!render_state.current_valid ||
      render_state.current_error ||
      render_state.current_message) {
    render_next_feature_value();
  }
}

static void update_button_controlled_display() {
  if (deadline_active(display_sleep_until_ms)) {
    power_off();
    return;
  }
  display_sleep_until_ms = 0;

  if (!deadline_active(keep_on_until_ms)) {
    keep_on_until_ms = 0;
    power_off();
    return;
  }

  power_on();
  if (render_state.current_message &&
      render_state.has_rendered &&
      millis() - render_state.last_render_ms >= DISPLAY_PERSISTENCE_MS) {
    render_next_feature_value();
    return;
  }
  refresh_current_feature_value();
}

void init_display(uint8_t configured_display_button_pin) {
  button.pin = button_enabled() ? configured_display_button_pin : 255;
  if (button_enabled()) {
    pinMode(button.pin, INPUT_PULLUP);
    button.last_read = digitalRead(button.pin);
  } else {
    button.last_read = HIGH;
  }
  button.last_change_ms = millis();
  button.pressed = false;
  button.long_press_handled = false;
  button.pressed_at_ms = 0;
  keep_on_until_ms = button_enabled() ? millis() + DISPLAY_AUTO_OFF_MS : 0;
  display_sleep_until_ms = 0;
  powered = true;
  render_state.has_rendered = false;
  render_state.current_valid = false;
  render_state.current_error = false;

  // init display
  Wire.setPins(I2C_SDA, I2C_SCL);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  // Address 0x3C for 128x32
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    available = false;
    powered = false;
    return;
  }

  available = true;
  begin_render();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("Starting");
  display.display();
  render_state.has_rendered = true;
  render_state.last_render_ms = millis();
}

void display_show_message(const char* title, const char* detail) {
  if (!available) {
    return;
  }

  display_sleep_until_ms = 0;
  if (button_enabled()) {
    keep_on_until_ms = millis() + DISPLAY_AUTO_OFF_MS;
  }
  power_on();
  render_message(title, detail);
}

void display_set_connectivity_status(bool wifi_connected, bool mqtt_connected) {
  bool changed = connectivity.wifi_connected != wifi_connected ||
      connectivity.mqtt_connected != mqtt_connected;
  connectivity.wifi_connected = wifi_connected;
  connectivity.mqtt_connected = mqtt_connected;
  if (changed) {
    invalidate_render();
  }
}

void display_sleep_for(unsigned long duration_ms) {
  if (!available) {
    return;
  }

  if (!button_enabled()) {
    display_sleep_until_ms = 0;
    keep_on_until_ms = 0;
    power_on();
    return;
  }

  display_sleep_until_ms = millis() + duration_ms;
  keep_on_until_ms = 0;
  if (!connectivity_has_error()) {
    power_off();
  }
}

void update_display() {
  handle_display_button();

  if (!available) {
    return;
  }

  if (connectivity_has_error()) {
    update_connectivity_error_display();
    return;
  }

  if (!button_enabled()) {
    update_always_on_display();
    return;
  }

  update_button_controlled_display();
}

void display_force_update() {
  if (!available) {
    return;
  }

  if (button_enabled() &&
      deadline_active(display_sleep_until_ms) &&
      !connectivity_has_error()) {
    power_off();
    return;
  }

  if (button_enabled()) {
    keep_on_until_ms = millis() + DISPLAY_AUTO_OFF_MS;
  }
  power_on();
  if (connectivity_has_error()) {
    render_connectivity_error();
    return;
  }

  render_next_feature_value();
}

#ifdef HOME_ANTHILL_TEST_OLED_DISPLAY
void display_reset_for_test() {
  available = true;
  powered = true;
  connectivity = {};
  render_state = {};
  button = {};
  keep_on_until_ms = 0;
  display_sleep_until_ms = 0;
  display_button_enabled_for_test = DISPLAY_BUTTON_ENABLED == true;
}

void display_set_available_for_test(bool is_available) {
  available = is_available;
}

void display_set_button_enabled_for_test(bool enabled) {
  display_button_enabled_for_test = enabled;
}

void display_invalidate_render_for_test() {
  render_state.has_rendered = false;
  render_state.last_render_ms = 0;
}

bool display_is_available_for_test() {
  return available;
}

bool display_is_powered_for_test() {
  return powered;
}

uint8_t display_button_pin_for_test() {
  return button.pin;
}
#endif

#endif
