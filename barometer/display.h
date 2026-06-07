#pragma once

#include "secrets.h"

#ifdef HOME_ANTHILL_TEST_OLED_DISPLAY
#undef OLED_DISPLAY
#define OLED_DISPLAY true
#endif

#ifndef OLED_DISPLAY
#define OLED_DISPLAY false
#endif

#if OLED_DISPLAY == true
void init_display();
void update_display();
#else
inline void init_display() {}
inline void update_display() {}
#endif
