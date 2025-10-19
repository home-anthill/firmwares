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

// private local functions
int register_server(WiFiClient& wifi_client, const char* mac_address, JsonDocument features);
void get_register_url(char** register_url);
void build_register_payload(char** register_payload, const char* mac_address, JsonDocument features);

int register_secure_server(WiFiClientSecure& wifi_client, const char* mac_address, JsonDocument features) {
  return register_server(wifi_client, mac_address, features);
}

int register_insecure_server(WiFiClient& wifi_client, const char* mac_address, JsonDocument features) {
  return register_server(wifi_client, mac_address, features);
}

/*
* register_server function 
* returns an uint:
*  0 => registered or already registered successfully
*  1 => cannot register, because http status code is not 200 (ok) or 209 (already registered)
*  2 => register success, but cannot save the response UUID in preferences
*  3 => cannot deserialize register JSON response payload (probably malformed or too big)
*  4 => response doesn't match request device (either mac, manufacturer or model are different)
*/
int register_server(WiFiClient& wifi_client, const char* mac_address, JsonDocument features) {
  HTTPClient http;
  char* register_url;
  get_register_url(&register_url);
  Serial.printf("register_server - register_url = %s\n", register_url);

  http.begin(wifi_client, register_url);
  // free register_url because created in 'get_register_url' via malloc()
  free(register_url);
  http.addHeader("Content-Type", "application/json; charset=utf-8");

  char* register_payload;
  build_register_payload(&register_payload, mac_address, features);
  Serial.printf("register_server - register with payload: %s\n", register_payload);

  const int http_response_code = http.POST(register_payload);
  if (http_response_code <= 0) {
    Serial.printf("register_server - error on sending POST with http_response_code = %d\n", http_response_code);
    http.end();
    Serial.println("register_server - retrying in 60 seconds...");
    delay(60000);
    // call again register_server() recursively after the delay
    return register_server(wifi_client, mac_address, features);
  }
  Serial.printf("register_server - http_response_code = %d\n", http_response_code);

  if (http_response_code != HTTP_CODE_OK && http_response_code != HTTP_CODE_CONFLICT) {
    Serial.println("register_server - error bad http_response_code! Cannot register this device");
    return 1;
  }

  if (http_response_code == HTTP_CODE_OK) {
    Serial.println("register_server - HTTP_CODE_OK");
    StaticJsonDocument<2048> static_doc;
    DeserializationError err = deserializeJson(static_doc, http.getStream());
    // There is no need to check for specific reasons,
    // because err evaluates to true/false in this case,
    // as recommended by the developer of ArduinoJson
    if (!err) {
      Serial.println("register_server - deserialization succeeded!");
      serializeJsonPretty(static_doc, Serial);
      Serial.println();
      const char* uuid_value = static_doc["uuid"];
      const char* mac_value = static_doc["mac"];
      const char* manufacturer_value = static_doc["manufacturer"];
      const char* model_value = static_doc["model"];
      Serial.printf("register_server - uuid_value: %s\n", uuid_value);
      Serial.printf("register_server - mac_value: %s\n", mac_value);
      Serial.printf("register_server - manufacturer_value: %s\n", manufacturer_value);
      Serial.printf("register_server - model_value: %s\n", model_value);

      if (strcmp(mac_value, mac_address) != 0 || strcmp(manufacturer_value, MANUFACTURER) != 0 || strcmp(model_value, MODEL) != 0) {
        Serial.println("register_server - error request and response data don't match");
        return 4;
      }

      // print all features on terminal (debug purposes)
      JsonArray features = static_doc["features"].as<JsonArray>();
      for (int i = 0; i < features.size(); i++) {
        JsonObject feature = features[i];
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

void get_register_url(char** register_url) {
  Serial.println("get_register_url - called");
  # if SSL==true
    #define HTTP_PROTOCOL "https://"
  # else 
    #define HTTP_PROTOCOL "http://"
  # endif

  size_t register_url_len = strlen(HTTP_PROTOCOL) + strlen(SERVER_DOMAIN) + 1 + strlen(SERVER_PORT) + strlen(SERVER_PATH) + 1;
  char* result = (char*)malloc(register_url_len);
  if (result) {
    snprintf(result, register_url_len, "%s%s:%s%s", HTTP_PROTOCOL, SERVER_DOMAIN, SERVER_PORT, SERVER_PATH);
  }
  Serial.printf("get_register_url - result = %s\n", result);
  /* Set output parameters */
  *register_url = result;
}

void build_register_payload(char** register_payload, const char* mac_address, JsonDocument features) {
  Serial.printf("build_register_payload mac_address %s\n", mac_address);
  JsonDocument doc;
  doc["mac"] = mac_address;
  doc["manufacturer"] = MANUFACTURER;
  doc["model"] = MODEL;
  doc["apiToken"] = API_TOKEN;
  doc["features"] = features;
  char result[2048];
  serializeJson(doc, result);
  Serial.printf("build_register_payload result %s\n", result);
  *register_payload = result;
}