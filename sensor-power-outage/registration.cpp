// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>
// include json library (https://github.com/bblanchon/ArduinoJson)
#include <ArduinoJson.h>
// include wifi library
#include <WiFi.h>
// include http library (also required to use 'WiFiClientSecure')
#include <HTTPClient.h>

#include "secrets.h"

// include all local files
#include "storage.h"

// private local functions
int register_server(WiFiClient& wifi_client, String mac_address, String features);

int register_secure_server(WiFiClientSecure& wifi_client, String mac_address, String features) {
  return register_server(wifi_client, mac_address, features);
}

int register_insecure_server(WiFiClient& wifi_client, String mac_address, String features) {
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
int register_server(WiFiClient& wifi_client, String mac_address, String features) {
  HTTPClient http;

  # if SSL==true
  String http_protocol = "https://";
  # else 
  String http_protocol = "http://";
  # endif

  String register_url = http_protocol + SERVER_DOMAIN + ":" + SERVER_PORT + SERVER_PATH;
  Serial.print("register_server - register_url = ");
  Serial.println(register_url);

  http.begin(wifi_client, register_url);
  http.addHeader("Content-Type", "application/json; charset=utf-8");

  String register_payload = "{\"mac\": \"" + mac_address + 
    "\",\"manufacturer\": \"" + MANUFACTURER +
    "\",\"model\": \"" + MODEL +
    "\",\"apiToken\": \"" + API_TOKEN +   
    "\",\"features\": " + features + "}";

  Serial.print("register_server - register with payload: ");
  Serial.println(register_payload);
  const int http_response_code = http.POST(register_payload);
  if (http_response_code <= 0) {
    Serial.print("register_server - error on sending POST with http_response_code = ");
    Serial.println(http_response_code);
    http.end();

    Serial.println("register_server - retrying in 60 seconds...");
    delay(60000);
    // call again register_server() recursively after the delay
    return register_server(wifi_client, mac_address, features);
  }

  Serial.print("register_server - http_response_code = ");
  Serial.println(http_response_code);

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
      const char* uuid_value = static_doc["uuid"];
      const char* mac_value = static_doc["mac"];
      const char* manufacturer_value = static_doc["manufacturer"];
      const char* model_value = static_doc["model"];
      Serial.print("register_server - uuid_value: ");
      Serial.println(uuid_value);
      Serial.print("register_server - mac_value: ");
      Serial.println(mac_value);
      Serial.print("register_server - manufacturer_value: ");
      Serial.println(manufacturer_value);
      Serial.print("register_server - model_value: ");
      Serial.println(model_value);

      if (!String(mac_value).equals(mac_address) || !String(manufacturer_value).equals(MANUFACTURER) || !String(model_value).equals(MODEL)) {
        Serial.println("register_server - error request and response data don't match");
        return 4;
      }

      // save UUID in Preferences
      String uuid_str = String(uuid_value);
      size_t len = storage_set_uuid(uuid_str);
      if (len != uuid_str.length()) {
        Serial.println("************* ERROR **************");
        Serial.println("register_server - Cannot SAVE UUID in Preferences");
        Serial.println("**********************************");
        return 2;
      }
    } else {
      Serial.println("register_server - cannot deserialize register JSON payload");
      return 3;
    }
  } else if (http_response_code == HTTP_CODE_CONFLICT) {
    // this is not an error, it appears every reboot after the first registration
    Serial.println("register_server - HTTP_CODE_CONFLICT - already registered");
  }
  return 0; // OK - registered without errors
}