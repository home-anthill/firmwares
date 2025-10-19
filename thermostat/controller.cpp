// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>
// include json library (https://github.com/bblanchon/ArduinoJson)
#include <ArduinoJson.h>

#include "secrets.h"

// include local files
#include "storage.h"

// Temperature ranges
#define TEMP_MIN 10
#define TEMP_MAX 25
#define TOLERANCE_MIN 0
#define TOLERANCE_MAX 20

float get_setpoint() {
  Serial.println("get_setpoint - called");
  JsonDocument doc;
  JsonArray featureValues = doc.to<JsonArray>();
  storage_get_feature_values(featureValues);
  for (int i = 0; i < featureValues.size(); i++) {
    JsonObject featureValue = featureValues[i];
    const char* f_feature_name = featureValue["featureName"];
    float f_feature_value = featureValue["value"];
    if(strcmp(f_feature_name, "setpoint") == 0) {
      return f_feature_value;
    }
  }
  return 20; // default value
}

float get_tolerance() {
  Serial.println("get_tolerance - called");
  JsonDocument doc;
  JsonArray featureValues = doc.to<JsonArray>();
  storage_get_feature_values(featureValues);
  for (int i = 0; i < featureValues.size(); i++) {
    JsonObject featureValue = featureValues[i];
    const char* f_feature_name = featureValue["featureName"];
    float f_feature_value = featureValue["value"];
    if(strcmp(f_feature_name, "tolerance") == 0) {
      return f_feature_value;
    }
  }
  return 5; // default value
}

void set_configuration(char* saved_device_uuid, JsonArray saved_features, byte* payload) {  
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("set_setpoint - deserializeJson() failed: ");
    Serial.println(error.f_str());
    return;
  }

  JsonArray featureValues = doc.as<JsonArray>();
  for (int i = 0; i < featureValues.size(); i++) {
    JsonObject featureValue = featureValues[i];
    const char* f_api_token = featureValue["apiToken"];
    const char* f_device_uuid = featureValue["deviceUuid"];
    const char* f_mac = featureValue["mac"];
    const char* f_model = featureValue["model"];
    const char* f_feature_uuid = featureValue["featureUuid"];
    const char* f_feature_name = featureValue["featureName"];
    float f_feature_value = featureValue["value"];
    Serial.printf("\n---------------------------\n");
    Serial.printf("set_configuration - f_api_token = %s\n", f_api_token);
    Serial.printf("set_configuration - f_device_uuid = %s\n", f_device_uuid);
    Serial.printf("set_configuration - f_mac = %s\n", f_mac);
    Serial.printf("set_configuration - f_model = %s\n", f_model);
    Serial.printf("set_configuration - f_feature_uuid = %s\n", f_feature_uuid);
    Serial.printf("set_configuration - f_feature_name = %s\n", f_feature_name);
    Serial.printf("set_configuration - f_feature_value = %.2f\n", f_feature_value);
    Serial.printf("---------------------------\n");
    
    // validate infos
    if (strcmp(f_model, MODEL) != 0 || strcmp(f_api_token, API_TOKEN) != 0) {
      Serial.println("set_configuration - error model and api_token doesn't match");
      return;
    }
    if(strcmp(f_feature_name, "setpoint") == 0) {
      Serial.printf("set_configuration - setpoint - f_feature_value: %.2f\n", f_feature_value);
      if (f_feature_value < TEMP_MIN || f_feature_value > TEMP_MAX) {
        Serial.printf("set_configuration - cannot set value, because setpoint is out of range. setpoint must be >= %d and <= %d\n", TEMP_MIN, TEMP_MAX);
        return;
      }
    }
    if(strcmp(f_feature_name, "tolerance") == 0) {
      Serial.printf("set_configuration - tolerance - f_feature_value: %.2f\n", f_feature_value);
      if (f_feature_value < TOLERANCE_MIN || f_feature_value > TOLERANCE_MAX) {
        Serial.printf("set_configuration - cannot set value, because tolerance is out of range. tolerance must be >= %d and <= %d\n", TOLERANCE_MIN, TOLERANCE_MAX);
        return;
      }
    }
  }
  // save feature values in Preferences
  storage_set_feature_values(featureValues);
}