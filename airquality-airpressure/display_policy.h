#pragma once

#include <HomeAnthillDisplay.h>
#include <HomeAnthillCore.h>
#include <string.h>

inline uint8_t display_precision(const FeatureValue& value) {
  return strcmp(value.name, "voc") == 0 ? 3 : 1;
}

inline const ha::DisplayPolicy display_policy = [] {
  ha::DisplayPolicy policy;
  policy.precision = display_precision;
  return policy;
}();
