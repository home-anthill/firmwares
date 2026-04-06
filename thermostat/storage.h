// NOTE: This file intentionally diverges from storage.h in all other firmwares.
// The two extra declarations at the bottom are thermostat-specific.
// See storage.cpp for full explanation.
#pragma once

#include <ArduinoJson.h>

size_t storage_get_uuid(char* saved_uuid);

size_t storage_set_uuid(const char* uuid);

void storage_get_features(JsonArray features);

size_t storage_set_features(JsonArray features);

void storage_get_feature_values(JsonArray featureValues);

size_t storage_set_feature_values(JsonArray featureValues);