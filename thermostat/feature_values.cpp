#include "feature_values.h"

#include <string.h>

FeatureValue feature_values[FEATURE_VALUES_MAX_COUNT];
size_t feature_values_len = 0;

static bool feature_is_displayable(JsonObject feature) {
  const char* name = feature["name"];
  return name != nullptr && strlen(name) > 0 && strcmp(name, "online") != 0;
}

static bool feature_is_sensor(JsonObject feature) {
  const char* type = feature["type"];
  return type != nullptr && strcmp(type, "sensor") == 0;
}

static void feature_values_add(JsonObject feature) {
  if (feature_values_len >= FEATURE_VALUES_MAX_COUNT || !feature_is_displayable(feature)) {
    return;
  }

  const char* name = feature["name"];
  const char* unit = feature["unit"];
  FeatureValue* value = &feature_values[feature_values_len];
  strncpy(value->name, name, sizeof(value->name) - 1);
  value->name[sizeof(value->name) - 1] = '\0';

  if (unit != nullptr) {
    strncpy(value->unit, unit, sizeof(value->unit) - 1);
    value->unit[sizeof(value->unit) - 1] = '\0';
  }

  value->value = 0;
  value->has_value = false;
  feature_values_len++;
}

void feature_values_clear() {
  memset(feature_values, 0, sizeof(feature_values));
  feature_values_len = 0;
}

void feature_values_init(JsonArray features) {
  feature_values_clear();

  for (JsonObject feature : features) {
    if (feature_is_sensor(feature)) {
      feature_values_add(feature);
    }
  }

  for (JsonObject feature : features) {
    if (!feature_is_sensor(feature)) {
      feature_values_add(feature);
    }
  }
}

size_t feature_values_count() {
  return feature_values_len;
}

bool feature_values_set(const char* name, float new_value) {
  if (name == nullptr) {
    return false;
  }

  for (size_t i = 0; i < feature_values_len; i++) {
    if (strcmp(feature_values[i].name, name) == 0) {
      feature_values[i].value = new_value;
      feature_values[i].has_value = true;
      return true;
    }
  }

  return false;
}

bool feature_values_get(size_t index, FeatureValue* value) {
  if (value == nullptr || index >= feature_values_len) {
    return false;
  }

  *value = feature_values[index];
  return true;
}
