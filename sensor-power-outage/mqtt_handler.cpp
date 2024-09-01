// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>
// include Json library
#include <ArduinoJson.h>
// include MQTT library (https://pubsubclient.knolleary.net/api)
#include <PubSubClient.h>

#include "secrets.h"

const char* mqtt_url = MQTT_URL;
const int mqtt_port = MQTT_PORT;

PubSubClient mqtt_client;
int mqtt_retries = 0;

void mqtt_init(Client& wifi_client) {
  mqtt_client.setServer(mqtt_url, mqtt_port);
  mqtt_client.setClient(wifi_client);
}

void mqtt_connect(String uuid) { 
  while (!mqtt_client.connected()) {
    Serial.println("mqtt_connect - attempting MQTT connection...");
    mqtt_client.setBufferSize(4096);
    
    // here you can use the version with `connect(const char* id, const char* user, const char* pass)` with authentication
    const char* id_client = uuid.c_str();
    Serial.print("mqtt_connect - connecting to MQTT with client id = ");
    Serial.println(id_client);

    bool connected = false;
    # if MQTT_AUTH==true
      Serial.println("mqtt_connect - connecting to MQTT with authentication");
      const char* mqtt_username = MQTT_USERNAME; 
      const char* mqtt_password = MQTT_PASSWORD;
      connected = mqtt_client.connect(id_client, mqtt_username, mqtt_password);
    # else
      Serial.println("mqtt_connect - connecting to MQTT without authentication");
      connected = mqtt_client.connect(id_client);
    # endif

    if (connected) {
      Serial.print("mqtt_connect - connected and subscribing with uuid: ");
      Serial.println(uuid);
      mqtt_retries = 0;
    } else {
      Serial.print("mqtt_connect - failed, rc=");
      Serial.print(mqtt_client.state());
      mqtt_retries++;
      Serial.println(" - try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
    // after 100 retries (100 * 5 = 500 seconds) without success, reboot this device
    if (mqtt_retries > 100) {
      ESP.restart();
    }
  }
}

// void mqtt_notify_value(String uuid, String type, float value) {
//   Serial.print("mqtt_notify_value - called with uuid = ");
//   Serial.print(uuid);
//   Serial.printf(", type = %s, value = %.2f\n", type, value);

//   char payload_to_send[562];
//   DynamicJsonDocument inner_payload_msg(50);
//   inner_payload_msg["value"] = value;
//   DynamicJsonDocument payloadMsg(512);
//   payloadMsg["uuid"] = uuid;
//   payloadMsg["apiToken"] = API_TOKEN;
//   payloadMsg["payload"] = inner_payload_msg;
//   serializeJson(payloadMsg, payload_to_send);

//   String topic = "sensors/" + uuid + "/" + type;
//   Serial.println("mqtt_notify_value - publishing topic=" + topic);
//   mqtt_client.publish(topic.c_str(), payload_to_send);
// }


void mqtt_notify_online(String uuid) {
  Serial.print("mqtt_notify_online - called with uuid = ");
  Serial.println(uuid);

 char payload_to_send[562];
  DynamicJsonDocument inner_payload_msg(50);
  inner_payload_msg["value"] = 1;
  DynamicJsonDocument payloadMsg(512);
  payloadMsg["uuid"] = uuid;
  payloadMsg["apiToken"] = API_TOKEN;
  payloadMsg["payload"] = inner_payload_msg;
  serializeJson(payloadMsg, payload_to_send);

  String topic = "online/" + uuid;
  Serial.println("mqtt_notify_online - publishing topic=" + topic);
  mqtt_client.publish(topic.c_str(), payload_to_send);
}