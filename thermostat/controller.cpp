// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>
// include json library (https://github.com/bblanchon/ArduinoJson)
#include <ArduinoJson.h>
#include <ctime>

#include "secrets.h"

// include local files
#include "storage.h"

bool hmac_sha256_hex(const char* key, const char* message, char* out);

static const long COMMAND_MAX_SKEW_SECS = 300;
static const size_t HMAC_SHA256_HEX_LEN = 64;

// Temperature ranges
#define TEMP_MIN 10
#define TEMP_MAX 25
#define TOLERANCE_MIN 0
#define TOLERANCE_MAX 20

static bool payload_matches_registered_feature(JsonArray saved_features,
                                               const char* feature_uuid,
                                               const char* feature_name) {
  for (size_t i = 0; i < saved_features.size(); i++) {
    JsonObject feature = saved_features[i];
    const char* saved_uuid = feature["uuid"];
    const char* saved_name = feature["name"];
    if (saved_uuid == nullptr || saved_name == nullptr) continue;
    if (strcmp(saved_uuid, feature_uuid) == 0 &&
        strcmp(saved_name, feature_name) == 0) {
      return true;
    }
  }
  return false;
}

static bool signature_equals(const char* expected, const char* actual) {
  if (expected == nullptr || actual == nullptr ||
      strlen(expected) != HMAC_SHA256_HEX_LEN ||
      strlen(actual) != HMAC_SHA256_HEX_LEN) {
    return false;
  }
  uint8_t diff = 0;
  for (size_t i = 0; i < HMAC_SHA256_HEX_LEN; i++) {
    diff |= static_cast<uint8_t>(expected[i] ^ actual[i]);
  }
  return diff == 0;
}

static bool verify_command_signature(JsonObject featureValue) {
  const char* f_device_uuid = featureValue["deviceUuid"];
  const char* f_mac = featureValue["mac"];
  const char* f_model = featureValue["model"];
  const char* f_feature_uuid = featureValue["featureUuid"];
  const char* f_feature_name = featureValue["featureName"];
  const char* f_nonce = featureValue["nonce"];
  const char* f_signature = featureValue["signature"];
  long f_timestamp = featureValue["timestamp"] | 0L;
  JsonObject payloadObj = featureValue["payload"];

  if (f_model == nullptr || f_device_uuid == nullptr || f_mac == nullptr ||
      f_feature_uuid == nullptr || f_feature_name == nullptr ||
      f_nonce == nullptr || f_signature == nullptr ||
      f_timestamp <= 0 || payloadObj.isNull()) {
    Serial.println("set_configuration - error missing required signed command fields");
    return false;
  }

  long now = static_cast<long>(time(nullptr));
  if (now <= 0 || labs(now - f_timestamp) > COMMAND_MAX_SKEW_SECS) {
    Serial.println("set_configuration - command timestamp outside allowed window");
    return false;
  }

  char payload_json[96];
  serializeJson(payloadObj, payload_json, sizeof(payload_json));
  char signed_payload[512];
  snprintf(signed_payload, sizeof(signed_payload), "%s\n%s\n%s\n%s\n%s\n%ld\n%s\n%s",
           f_device_uuid, f_mac, f_model, f_feature_uuid, f_feature_name,
           f_timestamp, f_nonce, payload_json);
  char expected_signature[65];
  if (!hmac_sha256_hex(API_TOKEN, signed_payload, expected_signature)) {
    Serial.println("set_configuration - failed to verify command signature");
    return false;
  }
  if (!signature_equals(expected_signature, f_signature)) {
    Serial.println("set_configuration - command signature mismatch");
    return false;
  }
  return true;
}

static constexpr float DEFAULT_SETPOINT = 20.0f;
static constexpr float DEFAULT_TOLERANCE = 5.0f;

float get_setpoint() {
  Serial.println("get_setpoint - called");
  JsonDocument doc;
  JsonArray featureValues = doc.to<JsonArray>();
  storage_get_feature_values(featureValues);
  for (int i = 0; i < featureValues.size(); i++) {
    JsonObject featureValue = featureValues[i];
    const char* f_feature_name = featureValue["featureName"];
    if (f_feature_name == nullptr) continue;
    float f_feature_value = featureValue["value"];
    if (strcmp(f_feature_name, "setpoint") == 0) {
      return f_feature_value;
    }
  }
  return DEFAULT_SETPOINT;
}

float get_tolerance() {
  Serial.println("get_tolerance - called");
  JsonDocument doc;
  JsonArray featureValues = doc.to<JsonArray>();
  storage_get_feature_values(featureValues);
  for (int i = 0; i < featureValues.size(); i++) {
    JsonObject featureValue = featureValues[i];
    const char* f_feature_name = featureValue["featureName"];
    if (f_feature_name == nullptr) continue;
    float f_feature_value = featureValue["value"];
    if (strcmp(f_feature_name, "tolerance") == 0) {
      return f_feature_value;
    }
  }
  return DEFAULT_TOLERANCE;
}

void set_configuration(const char* saved_device_uuid,
                       const char* saved_mac_address,
                       JsonArray saved_features, uint8_t* payload,
                       unsigned int length) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("set_setpoint - deserializeJson() failed: ");
    Serial.println(error.f_str());
    return;
  }

  JsonArray featureValues = doc.as<JsonArray>();
  for (int i = 0; i < featureValues.size(); i++) {
    JsonObject featureValue = featureValues[i];
    const char* f_device_uuid = featureValue["deviceUuid"];
    const char* f_mac = featureValue["mac"];
    const char* f_model = featureValue["model"];
    const char* f_feature_uuid = featureValue["featureUuid"];
    const char* f_feature_name = featureValue["featureName"];
    JsonObject payloadObj = featureValue["payload"];
    float f_feature_value = payloadObj["value"];
    Serial.printf("\n---------------------------\n");
    Serial.printf("set_configuration - f_device_uuid = %s\n", f_device_uuid);
    Serial.printf("set_configuration - f_mac = %s\n", f_mac);
    Serial.printf("set_configuration - f_model = %s\n", f_model);
    Serial.printf("set_configuration - f_feature_uuid = %s\n", f_feature_uuid);
    Serial.printf("set_configuration - f_feature_name = %s\n", f_feature_name);
    Serial.printf("set_configuration - f_feature_value = %.2f\n", f_feature_value);
    Serial.printf("---------------------------\n");

    // validate infos
    if (f_model == nullptr || f_device_uuid == nullptr || f_mac == nullptr ||
        f_feature_uuid == nullptr || f_feature_name == nullptr || payloadObj.isNull()) {
      Serial.println("set_configuration - error missing required fields in payload");
      return;
    }
    if (!verify_command_signature(featureValue)) {
      return;
    }
    if (strcmp(f_model, MODEL) != 0) {
      Serial.println("set_configuration - error model doesn't match");
      return;
    }
    if (strcmp(f_device_uuid, saved_device_uuid) != 0 ||
        strcmp(f_mac, saved_mac_address) != 0) {
      Serial.println("set_configuration - error device identity doesn't match");
      return;
    }
    if (!payload_matches_registered_feature(saved_features, f_feature_uuid,
                                            f_feature_name)) {
      Serial.println("set_configuration - error feature_uuid is not registered for this device");
      return;
    }
    if (strcmp(f_feature_name, "setpoint") == 0) {
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
    featureValue["value"] = f_feature_value;
  }
  // save feature values in Preferences
  storage_set_feature_values(featureValues);
}
