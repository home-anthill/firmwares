// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>
// include json library (https://github.com/bblanchon/ArduinoJson)
#include <ArduinoJson.h>

// include libraries
// - IRremoteESP8266: https://github.com/crankyoldgit/IRremoteESP8266
#include <IRac.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
// Import the specific implementation to use LG protocol to control LG ACs
#include <ir_LG.h>
#include <ctime>

#include "secrets.h"

// private functions
void ir_send_signal();
bool hmac_sha256_hex(const char* key, const char* message, char* out);

static const long COMMAND_MAX_SKEW_SECS = 300;
static const size_t HMAC_SHA256_HEX_LEN = 64;
static const size_t COMMAND_NONCE_HEX_LEN = 32;
static const size_t RECENT_COMMAND_NONCE_COUNT = 32;

struct CommandNonceEntry {
  char nonce[COMMAND_NONCE_HEX_LEN + 1];
  long accepted_at;
  bool used;
};

static CommandNonceEntry recent_command_nonces[RECENT_COMMAND_NONCE_COUNT] = {};

#if defined(HOME_ANTHILL_HOST_TEST)
void reset_command_nonce_cache_for_test() {
  memset(recent_command_nonces, 0, sizeof(recent_command_nonces));
}
#endif

static bool claim_command_nonce(const char* nonce) {
  if (nonce == nullptr || strlen(nonce) != COMMAND_NONCE_HEX_LEN) {
    return false;
  }

  long now = static_cast<long>(time(nullptr));
  if (now <= 0) {
    return false;
  }

  int free_index = -1;
  int oldest_index = 0;
  long oldest_accepted_at = now;
  for (size_t i = 0; i < RECENT_COMMAND_NONCE_COUNT; i++) {
    CommandNonceEntry& entry = recent_command_nonces[i];
    if (entry.used && labs(now - entry.accepted_at) > COMMAND_MAX_SKEW_SECS) {
      entry.used = false;
      entry.nonce[0] = '\0';
    }
    if (entry.used && strcmp(entry.nonce, nonce) == 0) {
      return false;
    }
    if (!entry.used && free_index < 0) {
      free_index = static_cast<int>(i);
    }
    if (entry.used && entry.accepted_at <= oldest_accepted_at) {
      oldest_accepted_at = entry.accepted_at;
      oldest_index = static_cast<int>(i);
    }
  }

  int claim_index = free_index >= 0 ? free_index : oldest_index;
  strncpy(recent_command_nonces[claim_index].nonce, nonce, COMMAND_NONCE_HEX_LEN + 1);
  recent_command_nonces[claim_index].nonce[COMMAND_NONCE_HEX_LEN] = '\0';
  recent_command_nonces[claim_index].accepted_at = now;
  recent_command_nonces[claim_index].used = true;
  return true;
}

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

static bool verify_command_signature(JsonObject mqttFeature) {
  const char* deviceUuidval  = mqttFeature["deviceUuid"];
  const char* macval         = mqttFeature["mac"];
  const char* modelval       = mqttFeature["model"];
  const char* featureUuidval = mqttFeature["featureUuid"];
  const char* featureNameval = mqttFeature["featureName"];
  const char* nonceval       = mqttFeature["nonce"];
  const char* signatureval   = mqttFeature["signature"];
  long timestampval          = mqttFeature["timestamp"] | 0L;
  JsonObject payloadObj      = mqttFeature["payload"];

  if (deviceUuidval == nullptr || macval == nullptr || modelval == nullptr ||
      featureUuidval == nullptr || featureNameval == nullptr ||
      nonceval == nullptr || signatureval == nullptr ||
      timestampval <= 0 || payloadObj.isNull()) {
    Serial.println("ir_send_command - skipping entry with missing signed command field");
    return false;
  }

  long now = static_cast<long>(time(nullptr));
  if (now <= 0 || labs(now - timestampval) > COMMAND_MAX_SKEW_SECS) {
    Serial.println("ir_send_command - command timestamp outside allowed window");
    return false;
  }

  char payload_json[96];
  serializeJson(payloadObj, payload_json, sizeof(payload_json));
  char signed_payload[512];
  snprintf(signed_payload, sizeof(signed_payload), "%s\n%s\n%s\n%s\n%s\n%ld\n%s\n%s",
           deviceUuidval, macval, modelval, featureUuidval, featureNameval,
           timestampval, nonceval, payload_json);
  char expected_signature[65];
  if (!hmac_sha256_hex(API_TOKEN, signed_payload, expected_signature)) {
    Serial.println("ir_send_command - failed to verify command signature");
    return false;
  }
  if (!signature_equals(expected_signature, signatureval)) {
    Serial.println("ir_send_command - command signature mismatch");
    return false;
  }
  return true;
}

// ------------------------------------------------------
// ------------------ IRremoteESP8266 -------------------
// GPIO pin to use to send IR signals
#define IR_SEND_PIN 4
// ------------------------------------------------------
// ------------------ LG protocol -----------------------
#define SEND_LG
// Temoerature ranges
#define TEMP_MIN kLgAcMinTemp // forced to 16
#define TEMP_MAX kLgAcMaxTemp // forced to 30
// Mode possibile values (defined in ir_LG.h)
#define MODE_COOL kLgAcCool // 0
#define MODE_DRY kLgAcDry // 1
#define MODE_FAN kLgAcFan // 2
#define MODE_AUTO kLgAcAuto // 3
#define MODE_HEAT kLgAcHeat // 4
// Fan values (defined in ir_LG.h)
#define FAN_MAX kLgAcFanHigh // 10
#define FAN_MED kLgAcFanMedium // 2
#define FAN_MIN kLgAcFanLowest // 0
#define FAN_AUTO kLgAcFanAuto // 5
 
 // Create a A/C object using GPIO to sending messages with
IRLgAc ac(IR_SEND_PIN);
// ------------------------------------------------------
// ------------------------------------------------------

void ir_init() {
  // set model version
  // based on your remote controller
  ac.setModel(lg_ac_remote_model_t::AKB74955603);

  ac.calibrate();
  delay(1000);
  // Start AC
  ac.begin();
}

void ir_send_command(const char* saved_device_uuid,
                     const char* saved_mac_address, JsonArray saved_features,
                     char* topic, uint8_t* payload, unsigned int length) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("ir_send_command - deserializeJson() failed: ");
    Serial.println(error.f_str());
    return;
  }

  JsonArray mqttFeatures = doc.as<JsonArray>();
  for (size_t i = 0; i < mqttFeatures.size(); i++) {
    JsonObject mqttFeature = mqttFeatures[i];
    const char* deviceUuidval  = mqttFeature["deviceUuid"];
    const char* macval         = mqttFeature["mac"];
    const char* modelval       = mqttFeature["model"];
    const char* featureUuidval = mqttFeature["featureUuid"];
    const char* featureNameval = mqttFeature["featureName"];
    const char* nonceval       = mqttFeature["nonce"];
    JsonObject payloadObj      = mqttFeature["payload"];
    float valueval             = payloadObj["value"];

    // Validate required fields before use
    if (deviceUuidval == nullptr || macval == nullptr || modelval == nullptr ||
        featureUuidval == nullptr || featureNameval == nullptr || payloadObj.isNull()) {
      Serial.println("ir_send_command - skipping entry with null required field");
      continue;
    }
    if (!verify_command_signature(mqttFeature)) {
      return;
    }
    if (strcmp(modelval, MODEL) != 0) {
      Serial.println("ir_send_command - model mismatch, ignoring command");
      return;
    }
    if (strcmp(deviceUuidval, saved_device_uuid) != 0 ||
        strcmp(macval, saved_mac_address) != 0) {
      Serial.println("ir_send_command - device identity mismatch, ignoring command");
      return;
    }
    if (!payload_matches_registered_feature(saved_features, featureUuidval,
                                            featureNameval)) {
      Serial.println("ir_send_command - feature_uuid is not registered for this device");
      return;
    }
    if (!claim_command_nonce(nonceval)) {
      Serial.println("ir_send_command - replayed command nonce, ignoring command");
      return;
    }

    Serial.printf("ir_send_command - deviceUuidval: %s\n", deviceUuidval ? deviceUuidval : "(null)");
    Serial.printf("ir_send_command - macval: %s\n", macval ? macval : "(null)");
    Serial.printf("ir_send_command - modelval: %s\n", modelval);
    Serial.printf("ir_send_command - featureUuidval: %s\n", featureUuidval ? featureUuidval : "(null)");
    Serial.printf("ir_send_command - featureNameval: %s\n", featureNameval);
    Serial.printf("ir_send_command - valueval: %.2f\n", valueval);

    int cmd = (int)roundf(valueval);

    if (strcmp(featureNameval, "on") == 0) {
      if (cmd == 1) {
        Serial.println("ir_send_command - setting On");
        ac.on();
      } else if (cmd == 0) {
        Serial.println("ir_send_command - setting Off");
        ac.off();
        // because OFF is a special fixed command, and you cannot set any other parameters
        ir_send_signal();
        return;
      }
    }
    if (strcmp(featureNameval, "setpoint") == 0) {
      if (valueval < TEMP_MIN || valueval > TEMP_MAX) {
        Serial.printf("ir_send_command - skipping setpoint: out of range (%.2f, must be %d-%d)\n", valueval, TEMP_MIN, TEMP_MAX);
        continue;
      }
      Serial.println("ir_send_command - setting temperature");
      ac.setTemp(valueval);
    }
    if (strcmp(featureNameval, "mode") == 0) {
      switch (cmd) {
        case 0: Serial.println("ir_send_command - setting mode to Cool"); ac.setMode(MODE_COOL); break;
        case 1: Serial.println("ir_send_command - setting mode to Dry");  ac.setMode(MODE_DRY);  break;
        case 2: Serial.println("ir_send_command - setting mode to Fan");  ac.setMode(MODE_FAN);  break;
        case 3: Serial.println("ir_send_command - setting mode to Auto"); ac.setMode(MODE_AUTO); break;
        case 4: Serial.println("ir_send_command - setting mode to Heat"); ac.setMode(MODE_HEAT); break;
        default: Serial.println("ir_send_command - cannot set mode. Unsupported value!"); break;
      }
    }
    if (strcmp(featureNameval, "fanSpeed") == 0) {
      switch (cmd) {
        case 10: Serial.println("ir_send_command - setting fan speed to Max");  ac.setFan(FAN_MAX);  break;
        case 2: Serial.println("ir_send_command - setting fan speed to Med");  ac.setFan(FAN_MED);  break;
        case 0: Serial.println("ir_send_command - setting fan speed to Min");  ac.setFan(FAN_MIN);  break;
        case 5: Serial.println("ir_send_command - setting fan speed to Auto"); ac.setFan(FAN_AUTO); break;
        default: Serial.println("ir_send_command - cannot set fan speed. Unsupported fan value!"); break;
      }
    }
  }
  ir_send_signal();
}

void ir_send_signal() {
  Serial.println("ir_send_signal - sending value via IR...");
  ac.send();
  Serial.println("ir_send_signal - value sent successfully!");
}
