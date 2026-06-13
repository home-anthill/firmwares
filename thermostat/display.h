#pragma once

#include <stdint.h>

#include "secrets.h"

#ifdef HOME_ANTHILL_TEST_OLED_DISPLAY
#undef OLED_DISPLAY
#define OLED_DISPLAY true
#endif

#ifndef OLED_DISPLAY
#define OLED_DISPLAY false
#endif

#if OLED_DISPLAY == true
void init_display(uint8_t button_pin);
void update_display();
void display_force_update();
void display_show_message(const char* title, const char* detail);
void display_set_connectivity_status(bool wifi_connected, bool mqtt_connected);
void display_sleep_for(unsigned long duration_ms);
#else
inline void init_display(uint8_t /*button_pin*/) {}
inline void update_display() {}
inline void display_force_update() {}
inline void display_show_message(const char* /*title*/, const char* /*detail*/) {}
inline void display_set_connectivity_status(bool /*wifi_connected*/, bool /*mqtt_connected*/) {}
inline void display_sleep_for(unsigned long /*duration_ms*/) {}
#endif
