#pragma once

#include <ArduinoJson.h>
#include <stddef.h>

#define FEATURE_VALUES_MAX_COUNT 16
#define FEATURE_VALUE_NAME_MAX_LEN 32
#define FEATURE_VALUE_UNIT_MAX_LEN 12

struct FeatureValue {
  char name[FEATURE_VALUE_NAME_MAX_LEN];
  char unit[FEATURE_VALUE_UNIT_MAX_LEN];
  float value;
  bool has_value;
};

void feature_values_init(JsonArray features);
void feature_values_clear();
size_t feature_values_count();
bool feature_values_set(const char* name, float value);
bool feature_values_get(size_t index, FeatureValue* value);
