#pragma once

#include <HomeAnthillDisplay.h>
#include <HomeAnthillCore.h>
#include <string.h>

inline const char* motion_display_label(float value) {
  int motion = static_cast<int>(value);
  if (motion == 1) {
    return "Detected";
  }
  if (motion == 0) {
    return "None";
  }
  return nullptr;
}

inline const char* display_label(const FeatureValue& value) {
  return strcmp(value.name, "motion") == 0 ? motion_display_label(value.value) : nullptr;
}

inline const ha::DisplayPolicy display_policy = [] {
  ha::DisplayPolicy policy;
  policy.label = display_label;
  return policy;
}();
