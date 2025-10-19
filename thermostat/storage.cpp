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
  JsonDocument doc;
  deserializeJson(doc, val);
  JsonArray values = doc.as<JsonArray>();
  for (int i = 0; i < values.size(); i++) {
    features.add(values[i]);
  }
  preferences.end();
}

size_t storage_set_features(JsonArray features) {
  preferences.begin("device", false);
  size_t len = preferences.putInt("numfeatures", features.size());
  String output;
  size_t written = serializeJson(features, output);
  preferences.putString("features", output.c_str());
  preferences.end();
  return len;
}

void storage_get_feature_values(JsonArray featureValues) {
  preferences.begin("device", true);
  String val = preferences.getString("featureValues", "");
  JsonDocument doc;
  deserializeJson(doc, val);
  JsonArray values = doc.as<JsonArray>();
  for (int i = 0; i < values.size(); i++) {
    featureValues.add(values[i]);
  }
  preferences.end();
}

size_t storage_set_feature_values(JsonArray featureValues) {
  preferences.begin("device", false);
  size_t len = preferences.putInt("numfeatures", featureValues.size());
  String output;
  size_t written = serializeJson(featureValues, output);
  preferences.putString("featureValues", output.c_str());
  preferences.end();
  return len;
}