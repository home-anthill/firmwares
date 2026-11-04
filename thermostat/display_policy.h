#pragma once

#include <HomeAnthillDisplay.h>
#include <HomeAnthillCore.h>
#include <string.h>

inline bool display_temperature_only(const FeatureValue& value) {
  return strcmp(value.name, "temperature") == 0;
}

inline const ha::DisplayPolicy display_policy = [] {
  ha::DisplayPolicy policy;
  policy.visible = display_temperature_only;
  policy.redraw_always_on = true;
  policy.decimals_with_unit = 2;
  policy.connectivity_notice_ms = 10000;
  policy.message_title = "Message";
  policy.message_detail = "Received";
  return policy;
}();
