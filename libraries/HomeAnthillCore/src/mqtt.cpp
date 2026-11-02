// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>
// include Json library
#include <ArduinoJson.h>
// include MQTT library (https://pubsubclient.knolleary.net/api)
#include <PubSubClient.h>
#include <cmath>
#include <cstring>
#include <ctime>

#include "anthill/mqtt.h"
#include "anthill/signing.h"

PubSubClient mqtt_client;

namespace ha {
static int mqtt_retries = 0;
void mqtt_subscribe(const char* uuid, const char* command);

static const long MIN_VALID_EPOCH_SECS = 1700000000L;

static bool format_json_value(float value, char* out, size_t out_len, bool integer_value) {
  if (!std::isfinite(value) || out_len == 0) {
    if (out_len > 0) {
      out[0] = '\0';
    }
    return false;
  }
  int len;
  if (integer_value) {
    len = snprintf(out, out_len, "%ld", static_cast<long>(roundf(value)));
  } else {
    len = snprintf(out, out_len, "%.4f", value);
  }
  if (len < 0 || static_cast<size_t>(len) >= out_len) {
    out[0] = '\0';
    return false;
  }

  char* decimal = strchr(out, '.');
  if (decimal == nullptr) {
    return true;
  }
  char* end = out + strlen(out) - 1;
  while (end > decimal + 1 && *end == '0') {
    *end = '\0';
    end--;
  }
  return true;
}

void mqtt_init(Client& wifi_client, MqttCallback callback, const MqttConfig& config) {
  mqtt_client.setServer(config.host, config.port);
  mqtt_client.setClient(wifi_client);
  mqtt_client.setBufferSize(4096);
  mqtt_client.setCallback(callback);
  // bound TLS/TCP handshake so a partially-reachable broker cannot hang forever
  mqtt_client.setSocketTimeout(15);
}

void mqtt_connect(const char* uuid, const MqttConfig& config) {
  while (!mqtt_client.connected()) {
    Serial.printf("mqtt_connect - attempting MQTT connection with client id = device_uuid = %s\n", uuid);

    bool connected = false;
    if (config.authenticate) {
      connected = mqtt_client.connect(uuid, config.username, config.password);
    } else {
      connected = mqtt_client.connect(uuid);
    }

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

static void mqtt_publish_signed_value(const char* device_uuid, const char* feature_uuid, const char* feature_name,
                               float value, const char* topic, const MqttConfig& config, bool integer_value) {
  Serial.printf("mqtt_publish_signed_value - called with device_uuid=%s, feature_uuid=%s, feature_name=%s, value=%.2f\n",
                device_uuid, feature_uuid, feature_name, value);

  char payload_to_send[768];
  char value_json[24];
  if (!format_json_value(value, value_json, sizeof(value_json), integer_value)) {
    Serial.println("mqtt_publish_signed_value - invalid numeric value, skipping publish");
    return;
  }
  char payload_json[96];
  int payload_json_len = snprintf(payload_json, sizeof(payload_json), "{\"value\":%s}", value_json);
  if (payload_json_len < 0 || static_cast<size_t>(payload_json_len) >= sizeof(payload_json)) {
    Serial.println("mqtt_publish_signed_value - payload JSON truncated, skipping publish");
    return;
  }
  long timestamp = static_cast<long>(time(nullptr));
  if (timestamp < MIN_VALID_EPOCH_SECS) {
    Serial.printf("mqtt_publish_signed_value - invalid system time (%ld), skipping signed publish\n", timestamp);
    return;
  }
  char nonce[33];
  if (!random_hex(nonce, 16)) {
    Serial.println("mqtt_publish_signed_value - failed to generate nonce, skipping publish");
    return;
  }
  char signed_payload[256];
  int signed_payload_len = snprintf(signed_payload, sizeof(signed_payload), "%s\n%s\n%s\n%ld\n%s\n%s",
                                    device_uuid, feature_uuid, feature_name, timestamp, nonce, payload_json);
  if (signed_payload_len < 0 || static_cast<size_t>(signed_payload_len) >= sizeof(signed_payload)) {
    Serial.println("mqtt_publish_signed_value - signed payload truncated, skipping publish");
    return;
  }
  char signature[65];
  if (!hmac_sha256_hex(config.api_token, signed_payload, signature)) {
    Serial.println("mqtt_publish_signed_value - failed to sign payload, skipping publish");
    return;
  }
  int payload_to_send_len = snprintf(payload_to_send, sizeof(payload_to_send),
                                     "{\"deviceUuid\":\"%s\",\"featureUuid\":\"%s\",\"timestamp\":%ld,\"nonce\":\"%s\",\"signature\":\"%s\",\"payload\":%s}",
                                     device_uuid, feature_uuid, timestamp, nonce, signature, payload_json);
  if (payload_to_send_len < 0 || static_cast<size_t>(payload_to_send_len) >= sizeof(payload_to_send)) {
    Serial.println("mqtt_publish_signed_value - MQTT payload truncated, skipping publish");
    return;
  }
  Serial.printf("mqtt_publish_signed_value - payload_to_send=%s\n", payload_to_send);
  Serial.printf("mqtt_publish_signed_value - publishing topic=%s\n", topic);
  if (!mqtt_client.publish(topic, payload_to_send)) {
    Serial.printf("mqtt_publish_signed_value - publish failed for topic=%s, forcing disconnect to trigger reconnect\n",
                  topic);
    // Explicitly disconnect so mqtt_client.connected() returns false and the
    // main loop re-enters mqtt_connect() on the next iteration.  Without this,
    // a stale TCP socket can keep connected() returning true indefinitely while
    // all publishes silently fail.
    mqtt_client.disconnect();
  }
}

void mqtt_notify_value(const char* device_uuid, const char* feature_uuid, const char* type, float value, const MqttConfig& config, bool integer_value, const char* topic_name) {
  char topic[128];
  if (strcmp(type, "online") == 0) {
    snprintf(topic, sizeof(topic), "online/%s/features/%s", device_uuid, feature_uuid);
  } else {
    snprintf(topic, sizeof(topic), "sensors/%s/%s", device_uuid, topic_name != nullptr ? topic_name : type);
  }
  mqtt_publish_signed_value(device_uuid, feature_uuid, type, value, topic, config, integer_value);
}

void mqtt_notify_alarm(const char* device_uuid, const char* feature_uuid, const char* feature_name,
                       const char* alarm_type, float value, const MqttConfig& config, bool integer_value) {
  char topic[160];
  snprintf(topic, sizeof(topic), "alarms/%s/features/%s/%s", device_uuid, feature_uuid, alarm_type);
  mqtt_publish_signed_value(device_uuid, feature_uuid, feature_name, value, topic, config, integer_value);
}

void mqtt_subscribe(const char* uuid, const char* command) {
  Serial.println("mqtt_subscribe - creating topic based on command...");

  char topic[128];
  snprintf(topic, sizeof(topic), "devices/%s/%s", uuid, command);

  Serial.printf("mqtt_subscribe - subscribing topic=%s\n", topic);
  mqtt_client.subscribe(topic);
}

bool mqtt_try_connect_once(const char* uuid, const MqttConfig& config) {
  Serial.printf("mqtt_try_connect_once - attempting with uuid=%s\n", uuid);

  bool connected = false;
  if (config.authenticate) {
    connected = mqtt_client.connect(uuid, config.username, config.password);
  } else {
    connected = mqtt_client.connect(uuid);
  }

  if (connected) {
    Serial.println("mqtt_try_connect_once - connected, subscribing...");
    mqtt_retries = 0;
    mqtt_subscribe(uuid, "values");
  } else {
    Serial.printf("mqtt_try_connect_once - failed, state=%d\n", mqtt_client.state());
  }
  return connected;
}

}
