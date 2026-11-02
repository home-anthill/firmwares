#pragma once

#include <PubSubClient.h>
#include <functional>

extern PubSubClient mqtt_client;

namespace ha {
struct MqttConfig {
  const char* host;
  int port;
  bool authenticate;
  const char* username;
  const char* password;
  const char* api_token;
};
using MqttCallback = std::function<void(char*, uint8_t*, unsigned int)>;
void mqtt_init(Client& client, MqttCallback callback, const MqttConfig& config);
void mqtt_connect(const char* uuid, const MqttConfig& config);
bool mqtt_try_connect_once(const char* uuid, const MqttConfig& config);
void mqtt_notify_value(const char* device_uuid, const char* feature_uuid,
                       const char* feature_name, float value, const MqttConfig& config,
                       bool integer_value = false, const char* topic_name = nullptr);
void mqtt_notify_alarm(const char* device_uuid, const char* feature_uuid,
                       const char* feature_name, const char* alarm_type, float value,
                       const MqttConfig& config, bool integer_value = false);
}
