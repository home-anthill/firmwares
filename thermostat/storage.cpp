// NOTE: This file intentionally diverges from the shared storage.cpp used by all other
// firmwares (ac-beko, ac-lg, airquality-pir, barometer, dht-light, power-outage).
// The base functions (storage_get_uuid, storage_set_uuid, storage_get_features,
// storage_set_features) are identical to those in the other firmwares.
// The two extra functions below — storage_get_feature_values / storage_set_feature_values —
// are thermostat-specific: they persist user-configurable controller state (setpoint,
// tolerance) under the "featureValues" Preferences key, which no other firmware needs.
// Do NOT blindly overwrite this file when syncing shared modules.

// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>
// include json library (https://github.com/bblanchon/ArduinoJson)
#include <ArduinoJson.h>

// include Preferences, because eeprom lib has been deprecated for esp32
#include <Preferences.h>

Preferences preferences;

size_t storage_get_uuid(char* saved_uuid) {
  preferences.begin("device", true); 
  size_t len = preferences.getString("uuid", saved_uuid, 37);
  preferences.end();
  return len;
}

size_t storage_set_uuid(const char* uuid) {
  preferences.begin("device", false); 
  size_t len = preferences.putString("uuid", uuid);
  preferences.end();
  return len;
}

void storage_get_features(JsonArray features) {
  preferences.begin("device", true);
  String val = preferences.getString("features", "");
  if (val.length() == 0) {
    preferences.end();
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, val);
  if (err) {
    Serial.printf("storage_get_features - deserializeJson failed: %s\n", err.c_str());
    preferences.end();
    return;
  }
  JsonArray values = doc.as<JsonArray>();
  for (size_t i = 0; i < values.size(); i++) {
    features.add(values[i]);
  }
  preferences.end();
}

size_t storage_set_features(JsonArray features) {
  preferences.begin("device", false);
  preferences.putInt("numfeatures", features.size());
  String output;
  serializeJson(features, output);
  size_t len = preferences.putString("features", output.c_str());
  preferences.end();
  return len;
}

void storage_get_feature_values(JsonArray featureValues) {
  preferences.begin("device", true);
  String val = preferences.getString("featureValues", "");
  if (val.length() == 0) {
    preferences.end();
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, val);
  if (err) {
    Serial.printf("storage_get_feature_values - deserializeJson failed: %s\n", err.c_str());
    preferences.end();
    return;
  }
  JsonArray values = doc.as<JsonArray>();
  for (size_t i = 0; i < values.size(); i++) {
    featureValues.add(values[i]);
  }
  preferences.end();
}

size_t storage_set_feature_values(JsonArray featureValues) {
  preferences.begin("device", false);
  preferences.putInt("numfeatures", featureValues.size());
  String output;
  serializeJson(featureValues, output);
  size_t len = preferences.putString("featureValues", output.c_str());
  preferences.end();
  return len;
}
