// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>
// include Json library
#include <ArduinoJson.h>
// include MQTT library (https://pubsubclient.knolleary.net/api)
#include <PubSubClient.h>

#include "secrets.h"

// private functions
void mqtt_subscribe(const char* uuid, const char* command);

const char* mqtt_url = MQTT_URL;
const int mqtt_port = MQTT_PORT;

PubSubClient mqtt_client;
int mqtt_retries = 0;

void mqtt_init(Client& wifi_client, std::function<void (char *, uint8_t *, unsigned int)> callback) {
  mqtt_client.setServer(mqtt_url, mqtt_port);
  mqtt_client.setClient(wifi_client);
  mqtt_client.setBufferSize(4096);
  mqtt_client.setCallback(callback);
}

void mqtt_connect(const char* uuid) { 
  while (!mqtt_client.connected()) {
    Serial.printf("mqtt_connect - attempting MQTT connection with client id = device_uuid = %s\n", uuid);

    bool connected = false;
    # if MQTT_AUTH==true
      Serial.println("mqtt_connect - connecting to MQTT with authentication");
      const char* mqtt_username = MQTT_USERNAME; 
      const char* mqtt_password = MQTT_PASSWORD;
      // here you can use `connect(const char* id, const char* user, const char* pass)` with authentication
      connected = mqtt_client.connect(uuid, mqtt_username, mqtt_password);
    # else
      Serial.println("mqtt_connect - connecting to MQTT without authentication");
      connected = mqtt_client.connect(uuid);
    # endif

    if (connected) {
      Serial.printf("mqtt_connect - connected and subscribed with id = device_uuid = %s\n", uuid);
      mqtt_retries = 0;
      // subscribe
      mqtt_subscribe(uuid, "values");
    } else {
      Serial.printf("mqtt_connect - failed, mqtt_state=%d, mqtt_retries=%d - trying again in 5 seconds...\n", mqtt_client.state(), mqtt_retries);
      mqtt_retries++;
      // Wait 5 seconds before retrying
      delay(5000);
    }
    // after 100 retries (100 * 5 = 500 seconds) without success, reboot this device
    if (mqtt_retries > 100) {
      ESP.restart();
    }
  }
}

void mqtt_notify_value(const char* device_uuid, const char* feature_uuid, const char* type, float value) {
  Serial.printf("mqtt_notify_value - called with device_uuid=%s, feature_uuid=%s, type=%s, value=%.2f\n", device_uuid, feature_uuid, type, value);
  
  char payload_to_send[562];
  JsonDocument inner_payload_msg;
  inner_payload_msg["value"] = value;
  JsonDocument payloadMsg;
  payloadMsg["apiToken"] = API_TOKEN;
  payloadMsg["deviceUuid"] = device_uuid;
  payloadMsg["featureUuid"] = feature_uuid;
  payloadMsg["payload"] = inner_payload_msg;
  serializeJson(payloadMsg, payload_to_send, sizeof(payload_to_send));
  Serial.printf("mqtt_notify_value - payload_to_send=%s\n", payload_to_send);

  char topic[128];
  if (strcmp(type, "online") == 0) {
    snprintf(topic, sizeof(topic), "online/%s/features/%s", device_uuid, feature_uuid);
  } else {
    snprintf(topic, sizeof(topic), "sensors/%s/%s", device_uuid, type);
  }
  Serial.printf("mqtt_notify_value - publishing topic=%s\n", topic);
  if (!mqtt_client.publish(topic, payload_to_send)) {
    Serial.printf("mqtt_notify_value - publish failed for topic=%s\n", topic);
  }
}

void mqtt_subscribe(const char* uuid, const char* command) {
  Serial.println("mqtt_subscribe - creating topic based on command...");

  char topic[128];
  snprintf(topic, sizeof(topic), "devices/%s/%s", uuid, command);

  Serial.printf("mqtt_subscribe - subscribing topic=%s\n", topic);
  mqtt_client.subscribe(topic);
}
