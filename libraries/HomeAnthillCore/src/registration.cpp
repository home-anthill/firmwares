// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>
// include json library (https://github.com/bblanchon/ArduinoJson)
#include <ArduinoJson.h>
// include wifi library
#include <WiFi.h>
#include <WiFiClientSecure.h>
// include http library (also required to use 'WiFiClientSecure')
#include <HTTPClient.h>

#include "anthill/registration.h"
#include "anthill/storage.h"

namespace ha {
static void get_register_url(char** register_url, const RegistrationConfig& config);
static void build_register_payload(char* payload, size_t max_len, const char* mac,
                                   const JsonDocument& features, const RegistrationConfig& config);

/*
* register_server function
* returns an uint:
*  0 => registered or already registered successfully
*  1 => cannot register, because http status code is not 200 (ok) or 209 (already registered)
*  2 => register success, but cannot save the response UUID in preferences
*  3 => cannot deserialize register JSON response payload (probably malformed or too big)
*  4 => response doesn't match request device (either mac, manufacturer or model are different)
*/
int register_server(WiFiClient& wifi_client, const char* mac_address, const JsonDocument& features, const RegistrationConfig& config, bool retry_network_errors) {
  HTTPClient http;
  int http_response_code = 0;
  int register_retries = 0;
  const int max_register_retries = 10;

  do {
    char* register_url;
    get_register_url(&register_url, config);
    if (register_url == nullptr) {
      Serial.println("register_server - error: failed to allocate register_url");
      return 1;
    }
    Serial.printf("register_server - register_url = %s\n", register_url);

    http.begin(wifi_client, register_url);
    free(register_url);
    http.addHeader("Content-Type", "application/json; charset=utf-8");

    char register_payload[2048];
    build_register_payload(register_payload, sizeof(register_payload), mac_address, features, config);
    Serial.printf("register_server - register with payload: %s\n", register_payload);

    http_response_code = http.POST(register_payload);
    if (http_response_code <= 0) {
      Serial.printf("register_server - error on sending POST with http_response_code = %d\n", http_response_code);
      http.end();
      if (!retry_network_errors) {
        return 1;
      }
      register_retries++;
      if (register_retries >= max_register_retries) {
        Serial.println("register_server - max retries reached, rebooting...");
        ESP.restart();
      }
      Serial.printf("register_server - retrying in 60 seconds... (attempt %d/%d)\n", register_retries, max_register_retries);
      delay(60000);
      continue;
    }
    break;
  } while (true);
  Serial.printf("register_server - http_response_code = %d\n", http_response_code);

  if (http_response_code != HTTP_CODE_OK && http_response_code != HTTP_CODE_CONFLICT) {
    Serial.println("register_server - error bad http_response_code! Cannot register this device");
    return 1;
  }

  if (http_response_code == HTTP_CODE_OK) {
    Serial.println("register_server - HTTP_CODE_OK");
    JsonDocument static_doc;
    DeserializationError err = deserializeJson(static_doc, http.getStream());
    // There is no need to check for specific reasons,
    // because err evaluates to true/false in this case,
    // as recommended by the developer of ArduinoJson
    if (!err) {
      Serial.println("register_server - deserialization succeeded!");
      serializeJsonPretty(static_doc, Serial);
      Serial.println();
      const char* uuid_value = static_doc["uuid"] | static_cast<const char*>(nullptr);
      const char* mac_value = static_doc["mac"] | static_cast<const char*>(nullptr);
      const char* manufacturer_value = static_doc["manufacturer"] | static_cast<const char*>(nullptr);
      const char* model_value = static_doc["model"] | static_cast<const char*>(nullptr);
      Serial.printf("register_server - uuid_value: %s\n", uuid_value);
      Serial.printf("register_server - mac_value: %s\n", mac_value);
      Serial.printf("register_server - manufacturer_value: %s\n", manufacturer_value);
      Serial.printf("register_server - model_value: %s\n", model_value);

      if (uuid_value == nullptr || mac_value == nullptr || manufacturer_value == nullptr || model_value == nullptr) {
        Serial.println("register_server - error missing fields in response JSON");
        return 3;
      }

      if (strcmp(mac_value, mac_address) != 0 || strcmp(manufacturer_value, config.manufacturer) != 0 || strcmp(model_value, config.model) != 0) {
        Serial.println("register_server - error request and response data don't match");
        return 4;
      }

      // print all features on terminal (debug purposes)
      JsonArray features_arr = static_doc["features"].as<JsonArray>();
      for (size_t i = 0; i < features_arr.size(); i++) {
        JsonObject feature = features_arr[i];
        const char* f_uuid_value = feature["uuid"];
        const char* f_type_value = feature["type"];
        const char* f_name_value = feature["name"];
        bool f_enable_value = feature["enable"];
        int f_order_value = feature["order"];
        const char* f_unit_value = feature["unit"];
        Serial.printf("\n---------------------------\n");
        Serial.printf("register_server - f_uuid_value = %s\n", f_uuid_value);
        Serial.printf("register_server - f_type_value = %s\n", f_type_value);
        Serial.printf("register_server - f_name_value = %s\n", f_name_value);
        Serial.printf("register_server - f_enable_value = %d\n", f_enable_value);
        Serial.printf("register_server - f_order_value = %d\n", f_order_value);
        Serial.printf("register_server - f_unit_value = %s\n", f_unit_value);
        Serial.printf("---------------------------\n");
      }

      // save device UUID in Preferences
      size_t len = storage_set_uuid(uuid_value);
      if (len != strlen(uuid_value)) {
        Serial.println("************* ERROR **************");
        Serial.println("register_server - Cannot SAVE device UUID in Preferences");
        Serial.println("**********************************");
        return 2;
      }
      // save features array in Preferences
      len = storage_set_features(static_doc["features"].as<JsonArray>());
      if (len == 0) {
        Serial.println("************* ERROR **************");
        Serial.println("register_server - Cannot SAVE features array in Preferences");
        Serial.println("**********************************");
        return 2;
      }
    } else {
      Serial.println("register_server - cannot deserialize register JSON payload");
      return 3;
    }
  } else if (http_response_code == HTTP_CODE_CONFLICT) {
    // this is not an error, it will appear every reboot after the first registration
    Serial.println("register_server - HTTP_CODE_CONFLICT - already registered");
  }
  return 0; // OK - registered without errors
}

static void get_register_url(char** register_url, const RegistrationConfig& config) {
  const char* protocol = config.tls ? "https://" : "http://";
  Serial.println("get_register_url - called");
  size_t register_url_len = strlen(protocol) + strlen(config.domain) + 1 + snprintf(NULL, 0, "%d", config.port) + strlen(config.path) + 1;
  char* result = static_cast<char*>(malloc(register_url_len));
  if (result) {
    snprintf(result, register_url_len, "%s%s:%d%s", protocol, config.domain, config.port, config.path);
    Serial.printf("get_register_url - result = %s\n", result);
  }
  /* Set output parameters */
  *register_url = result;
}

static void build_register_payload(char* register_payload, size_t max_len, const char* mac_address, const JsonDocument& features, const RegistrationConfig& config) {
  Serial.printf("build_register_payload mac_address %s\n", mac_address);
  JsonDocument doc;
  doc["mac"] = mac_address;
  doc["manufacturer"] = config.manufacturer;
  doc["model"] = config.model;
  doc["apiToken"] = config.api_token;
  doc["features"] = features;
  serializeJson(doc, register_payload, max_len);
  Serial.printf("build_register_payload result %s\n", register_payload);
}

}
