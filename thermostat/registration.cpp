// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>
// include json library (https://github.com/bblanchon/ArduinoJson)
#include <ArduinoJson.h>
// include wifi library
#include <WiFi.h>
#include <WiFiClientSecure.h>
// include http library (also required to use 'WiFiClientSecure')
#include <HTTPClient.h>

#include "secrets.h"

// include all local files
#include "storage.h"

// HTTP protocol prefix at file scope (not inside a function)
#if SSL==true
  #define HTTP_PROTOCOL "https://"
#else
  #define HTTP_PROTOCOL "http://"
#endif

// private local functions
int register_once_impl(WiFiClient& wifi_client, const char* mac_address, const JsonDocument& features);
int register_server(WiFiClient& wifi_client, const char* mac_address, const JsonDocument& features);
void get_register_url(char** register_url);
void build_register_payload(char* register_payload, size_t max_len, const char* mac_address, const JsonDocument& features);

int register_secure_server(WiFiClientSecure& wifi_client, const char* mac_address, const JsonDocument& features) {
  return register_server(wifi_client, mac_address, features);
}

int register_insecure_server(WiFiClient& wifi_client, const char* mac_address, const JsonDocument& features) {
  return register_server(wifi_client, mac_address, features);
}

int register_secure_server_once(WiFiClientSecure& wifi_client, const char* mac_address, const JsonDocument& features) {
  return register_once_impl(wifi_client, mac_address, features);
}

int register_insecure_server_once(WiFiClient& wifi_client, const char* mac_address, const JsonDocument& features) {
  return register_once_impl(wifi_client, mac_address, features);
}

/*
* register_once_impl — one HTTP POST attempt, no retries, no delays, no ESP.restart().
* Callers are responsible for retry/backoff logic.
* Returns same codes as register_server:
*  0 => registered or already registered successfully
*  1 => network error or unexpected HTTP status
*  2 => register success, but cannot save UUID/features in preferences
*  3 => cannot deserialize register JSON response
*  4 => response doesn't match request device
*/
int register_once_impl(WiFiClient& wifi_client, const char* mac_address, const JsonDocument& features) {
  HTTPClient http;

  char* register_url;
  get_register_url(&register_url);
  if (register_url == nullptr) {
    Serial.println("register_once_impl - error: failed to allocate register_url");
    return 1;
  }
  Serial.printf("register_once_impl - register_url = %s\n", register_url);

  http.begin(wifi_client, register_url);
  free(register_url);
  http.addHeader("Content-Type", "application/json; charset=utf-8");

  char register_payload[2048];
  build_register_payload(register_payload, sizeof(register_payload), mac_address, features);
  Serial.printf("register_once_impl - register with payload: %s\n", register_payload);

  int http_response_code = http.POST(register_payload);
  if (http_response_code <= 0) {
    Serial.printf("register_once_impl - HTTP error: %d\n", http_response_code);
    http.end();
    return 1;
  }
  Serial.printf("register_once_impl - http_response_code = %d\n", http_response_code);

  if (http_response_code != HTTP_CODE_OK && http_response_code != HTTP_CODE_CONFLICT) {
    Serial.println("register_once_impl - error bad http_response_code! Cannot register this device");
    http.end();
    return 1;
  }

  if (http_response_code == HTTP_CODE_OK) {
    Serial.println("register_once_impl - HTTP_CODE_OK");
    JsonDocument static_doc;
    DeserializationError err = deserializeJson(static_doc, http.getStream());
    if (!err) {
      Serial.println("register_once_impl - deserialization succeeded!");
      serializeJsonPretty(static_doc, Serial);
      Serial.println();
      const char* uuid_value = static_doc["uuid"] | static_cast<const char*>(nullptr);
      const char* mac_value = static_doc["mac"] | static_cast<const char*>(nullptr);
      const char* manufacturer_value = static_doc["manufacturer"] | static_cast<const char*>(nullptr);
      const char* model_value = static_doc["model"] | static_cast<const char*>(nullptr);
      Serial.printf("register_once_impl - uuid_value: %s\n", uuid_value);
      Serial.printf("register_once_impl - mac_value: %s\n", mac_value);
      Serial.printf("register_once_impl - manufacturer_value: %s\n", manufacturer_value);
      Serial.printf("register_once_impl - model_value: %s\n", model_value);

      if (uuid_value == nullptr || mac_value == nullptr || manufacturer_value == nullptr || model_value == nullptr) {
        Serial.println("register_once_impl - error missing fields in response JSON");
        return 3;
      }

      if (strcmp(mac_value, mac_address) != 0 || strcmp(manufacturer_value, MANUFACTURER) != 0 || strcmp(model_value, MODEL) != 0) {
        Serial.println("register_once_impl - error request and response data don't match");
        return 4;
      }

      JsonArray features_arr = static_doc["features"].as<JsonArray>();
      for (size_t i = 0; i < features_arr.size(); i++) {
        JsonObject feature = features_arr[i];
        Serial.printf("\n---------------------------\n");
        Serial.printf("register_once_impl - f_uuid_value = %s\n", (const char*)feature["uuid"]);
        Serial.printf("register_once_impl - f_type_value = %s\n", (const char*)feature["type"]);
        Serial.printf("register_once_impl - f_name_value = %s\n", (const char*)feature["name"]);
        Serial.printf("---------------------------\n");
      }

      size_t len = storage_set_uuid(uuid_value);
      if (len != strlen(uuid_value)) {
        Serial.println("register_once_impl - Cannot SAVE device UUID in Preferences");
        return 2;
      }
      len = storage_set_features(static_doc["features"].as<JsonArray>());
      if (len == 0) {
        Serial.println("register_once_impl - Cannot SAVE features array in Preferences");
        return 2;
      }
    } else {
      Serial.println("register_once_impl - cannot deserialize register JSON payload");
      return 3;
    }
  } else if (http_response_code == HTTP_CODE_CONFLICT) {
    Serial.println("register_once_impl - HTTP_CODE_CONFLICT - already registered");
  }
  return 0;
}

/*
* register_server — wraps register_once_impl with retry/backoff (blocking).
* After max_register_retries network failures it calls ESP.restart().
* Returns same codes as register_once_impl.
*/
int register_server(WiFiClient& wifi_client, const char* mac_address, const JsonDocument& features) {
  int register_retries = 0;
  const int max_register_retries = 10;

  while (true) {
    int result = register_once_impl(wifi_client, mac_address, features);
    if (result == 1) {
      register_retries++;
      if (register_retries >= max_register_retries) {
        Serial.println("register_server - max retries reached, rebooting...");
        ESP.restart();
      }
      Serial.printf("register_server - retrying in 60 seconds... (attempt %d/%d)\n", register_retries, max_register_retries);
      delay(60000);
      continue;
    }
    return result;
  }
}

void get_register_url(char** register_url) {
  Serial.println("get_register_url - called");
  size_t register_url_len = strlen(HTTP_PROTOCOL) + strlen(SERVER_DOMAIN) + 1 + snprintf(NULL, 0, "%d", SERVER_PORT) + strlen(SERVER_PATH) + 1;
  char* result = static_cast<char*>(malloc(register_url_len));
  if (result) {
    snprintf(result, register_url_len, "%s%s:%d%s", HTTP_PROTOCOL, SERVER_DOMAIN, SERVER_PORT, SERVER_PATH);
    Serial.printf("get_register_url - result = %s\n", result);
  }
  /* Set output parameters */
  *register_url = result;
}

void build_register_payload(char* register_payload, size_t max_len, const char* mac_address, const JsonDocument& features) {
  Serial.printf("build_register_payload mac_address %s\n", mac_address);
  JsonDocument doc;
  doc["mac"] = mac_address;
  doc["manufacturer"] = MANUFACTURER;
  doc["model"] = MODEL;
  doc["apiToken"] = API_TOKEN;
  doc["features"] = features;
  serializeJson(doc, register_payload, max_len);
  Serial.printf("build_register_payload result %s\n", register_payload);
}
