#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <stddef.h>
#include <stdint.h>

struct FeatureValue;

namespace ha {
  struct DisplayConfig {
    bool enabled = true;
    bool button_enabled = true;
    uint8_t button_pin = 255;
    int sda = -1;
    int scl = -1;
    int8_t reset_pin = -1;
    uint8_t address = 0x3C;
    TwoWire* wire = &Wire;
    unsigned long persistence_ms = 1500;
    unsigned long auto_off_ms = 30000;
    unsigned long long_press_ms = 10000;
    unsigned long debounce_ms = 50;
  };

  struct DisplayPolicy {
    bool (*visible)(const FeatureValue&) = nullptr;
    const char* (*label)(const FeatureValue&) = nullptr;
    uint8_t (*precision)(const FeatureValue&) = nullptr;
    uint8_t decimals = 1;
    uint8_t decimals_with_unit = 1;
    bool redraw_always_on = false;
    // Zero keeps connection errors visible; a positive duration returns to data.
    unsigned long connectivity_notice_ms = 0;
    const char* message_title = "Status";
    const char* message_detail = "Online";
  };

  void display_init(const DisplayConfig& config, const DisplayPolicy& policy = {});
  void display_update();
  void display_force_update();
  void display_show_message(const char* title, const char* detail);
  void display_set_connectivity_status(bool wifi_connected, bool mqtt_connected);
  void display_sleep_for(unsigned long duration_ms);
  void display_set_overlay(const char* line1, const char* line2, const char* line3, const char* line4 = "");
  void display_clear_overlay();

#ifdef HOME_ANTHILL_TEST_OLED_DISPLAY
    void display_reset_for_test(const DisplayConfig& config, const DisplayPolicy& policy = {});
    void display_set_available_for_test(bool available);
    void display_invalidate_render_for_test();
    bool display_is_available_for_test();
    bool display_is_powered_for_test();
    uint8_t display_button_pin_for_test();
#endif
}
