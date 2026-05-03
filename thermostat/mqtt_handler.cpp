// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>
// include Json library
#include <ArduinoJson.h>
// include MQTT library (https://pubsubclient.knolleary.net/api)
#include <PubSubClient.h>
#include <cmath>
#include <cstring>
#include <ctime>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_system.h>
#include <mbedtls/md.h>
#elif !defined(HOME_ANTHILL_HOST_TEST)
#error "MQTT signing requires ESP32 mbedTLS support"
#endif

#include "secrets.h"

// private functions
void mqtt_subscribe(const char* uuid, const char* command);

const char* mqtt_url = MQTT_URL;
const int mqtt_port = MQTT_PORT;

PubSubClient mqtt_client;
int mqtt_retries = 0;

static const long MIN_VALID_EPOCH_SECS = 1700000000L;

bool format_json_float(float value, char* out, size_t out_len) {
  if (!isfinite(value) || out_len == 0) {
    if (out_len > 0) {
      out[0] = '\0';
    }
    return false;
  }
  int len = snprintf(out, out_len, "%.4f", value);
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

void bytes_to_hex(const uint8_t* bytes, size_t len, char* out) {
  static const char* hex = "0123456789abcdef";
  for (size_t i = 0; i < len; i++) {
    out[i * 2] = hex[bytes[i] >> 4];
    out[i * 2 + 1] = hex[bytes[i] & 0x0f];
  }
  out[len * 2] = '\0';
}

bool random_hex(char* out, size_t byte_len) {
  uint8_t bytes[32];
  if (byte_len > sizeof(bytes)) {
    out[0] = '\0';
    return false;
  }
#if defined(ARDUINO_ARCH_ESP32)
  esp_fill_random(bytes, byte_len);
#elif defined(HOME_ANTHILL_HOST_TEST)
  // Deterministic host-test nonce only. Production builds never use this branch.
  for (size_t i = 0; i < byte_len; i++) {
    bytes[i] = static_cast<uint8_t>(i);
  }
#else
#error "MQTT nonce generation requires ESP32 entropy"
#endif
  bytes_to_hex(bytes, byte_len, out);
  return true;
}

bool hmac_sha256_hex(const char* key, const char* message, char* out) {
#if defined(ARDUINO_ARCH_ESP32)
  uint8_t digest[32];
  const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (md_info == nullptr || mbedtls_md_hmac(md_info,
                                            reinterpret_cast<const unsigned char*>(key), strlen(key),
                                            reinterpret_cast<const unsigned char*>(message), strlen(message),
                                            digest) != 0) {
    out[0] = '\0';
    return false;
  }
  bytes_to_hex(digest, sizeof(digest), out);
  return true;
#elif defined(HOME_ANTHILL_HOST_TEST)
  // Deterministic host-test signature only. Production builds never use this branch.
  strncpy(out, "aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899", 65);
  return true;
#else
#error "MQTT payload signing requires ESP32 mbedTLS support"
#endif
}

void mqtt_init(Client& wifi_client, std::function<void (char *, uint8_t *, unsigned int)> callback) {
  mqtt_client.setServer(mqtt_url, mqtt_port);
  mqtt_client.setClient(wifi_client);
  mqtt_client.setBufferSize(4096);
  mqtt_client.setCallback(callback);
  // bound TLS/TCP handshake so a partially-reachable broker cannot hang forever
  mqtt_client.setSocketTimeout(15);
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
  
  char payload_to_send[768];
  char value_json[24];
  if (!format_json_float(value, value_json, sizeof(value_json))) {
    Serial.println("mqtt_notify_value - invalid numeric value, skipping publish");
    return;
  }
  char payload_json[96];
  int payload_json_len = snprintf(payload_json, sizeof(payload_json), "{\"value\":%s}", value_json);
  if (payload_json_len < 0 || static_cast<size_t>(payload_json_len) >= sizeof(payload_json)) {
    Serial.println("mqtt_notify_value - payload JSON truncated, skipping publish");
    return;
  }
  long timestamp = static_cast<long>(time(nullptr));
  if (timestamp < MIN_VALID_EPOCH_SECS) {
    Serial.printf("mqtt_notify_value - invalid system time (%ld), skipping signed publish\n", timestamp);
    return;
  }
  char nonce[33];
  if (!random_hex(nonce, 16)) {
    Serial.println("mqtt_notify_value - failed to generate nonce, skipping publish");
    return;
  }
  char signed_payload[256];
  int signed_payload_len = snprintf(signed_payload, sizeof(signed_payload), "%s\n%s\n%ld\n%s\n%s", device_uuid, feature_uuid, timestamp, nonce, payload_json);
  if (signed_payload_len < 0 || static_cast<size_t>(signed_payload_len) >= sizeof(signed_payload)) {
    Serial.println("mqtt_notify_value - signed payload truncated, skipping publish");
    return;
  }
  char signature[65];
  if (!hmac_sha256_hex(API_TOKEN, signed_payload, signature)) {
    Serial.println("mqtt_notify_value - failed to sign payload, skipping publish");
    return;
  }
  int payload_to_send_len = snprintf(payload_to_send, sizeof(payload_to_send),
                                     "{\"deviceUuid\":\"%s\",\"featureUuid\":\"%s\",\"timestamp\":%ld,\"nonce\":\"%s\",\"signature\":\"%s\",\"payload\":%s}",
                                     device_uuid, feature_uuid, timestamp, nonce, signature, payload_json);
  if (payload_to_send_len < 0 || static_cast<size_t>(payload_to_send_len) >= sizeof(payload_to_send)) {
    Serial.println("mqtt_notify_value - MQTT payload truncated, skipping publish");
    return;
  }
  Serial.printf("mqtt_notify_value - payload_to_send=%s\n", payload_to_send);

  char topic[128];
  if (strcmp(type, "online") == 0) {
    snprintf(topic, sizeof(topic), "online/%s/features/%s", device_uuid, feature_uuid);
  } else {
    snprintf(topic, sizeof(topic), "sensors/%s/%s", device_uuid, type);
  }
  Serial.printf("mqtt_notify_value - publishing topic=%s\n", topic);
  if (!mqtt_client.publish(topic, payload_to_send)) {
    Serial.printf("mqtt_notify_value - publish failed for topic=%s, forcing disconnect to trigger reconnect\n", topic);
    // Explicitly disconnect so mqtt_client.connected() returns false and the
    // main loop re-enters mqtt_connect() on the next iteration.  Without this,
    // a stale TCP socket can keep connected() returning true indefinitely while
    // all publishes silently fail.
    mqtt_client.disconnect();
  }
}

void mqtt_subscribe(const char* uuid, const char* command) {
  Serial.println("mqtt_subscribe - creating topic based on command...");

  char topic[128];
  snprintf(topic, sizeof(topic), "devices/%s/%s", uuid, command);

  Serial.printf("mqtt_subscribe - subscribing topic=%s\n", topic);
  mqtt_client.subscribe(topic);
}

bool mqtt_try_connect_once(const char* uuid) {
  Serial.printf("mqtt_try_connect_once - attempting with uuid=%s\n", uuid);

  bool connected = false;
  # if MQTT_AUTH==true
    const char* mqtt_username = MQTT_USERNAME;
    const char* mqtt_password = MQTT_PASSWORD;
    connected = mqtt_client.connect(uuid, mqtt_username, mqtt_password);
  # else
    connected = mqtt_client.connect(uuid);
  # endif

  if (connected) {
    Serial.println("mqtt_try_connect_once - connected, subscribing...");
    mqtt_retries = 0;
    mqtt_subscribe(uuid, "values");
  } else {
    Serial.printf("mqtt_try_connect_once - failed, state=%d\n", mqtt_client.state());
  }
  return connected;
}
