#pragma once

#include <HomeAnthillDisplay.h>
#include <HomeAnthillCore.h>
#include <string.h>

#ifndef SEND_COOLIX
#define SEND_COOLIX 1
#endif
#include <ir_Coolix.h>

inline int display_int_value(float value) {
  return static_cast<int>(value + (value >= 0.0f ? 0.5f : -0.5f));
}

inline const char* controller_display_label(const FeatureValue& value) {
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
      case kCoolixCool: return "Cool";
      case kCoolixDry: return "Dry";
      case kCoolixAuto: return "Auto";
      case kCoolixHeat: return "Heat";
      case kCoolixFan: return "Fan";
      default: return nullptr;
    }
  }

  if (strcmp(value.name, "fanSpeed") == 0) {
    switch (current) {
      case kCoolixFanAuto0: return "Auto0";
      case kCoolixFanMax: return "Max";
      case kCoolixFanMed: return "Med";
      case kCoolixFanMin: return "Min";
      case kCoolixFanAuto: return "Auto";
      default: return nullptr;
    }
  }

  return nullptr;
}

inline const ha::DisplayPolicy display_policy = [] {
  ha::DisplayPolicy policy;
  policy.label = controller_display_label;
  policy.message_title = "Message";
  policy.message_detail = "Received";
  return policy;
}();
