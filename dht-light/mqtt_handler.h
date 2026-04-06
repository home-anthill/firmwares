#pragma once

#include <functional>

extern PubSubClient mqtt_client;

void mqtt_init(Client& wifi_client, std::function<void (char *, uint8_t *, unsigned int)> callback);

void mqtt_connect(const char* uuid);

void mqtt_notify_value(const char* device_uuid, const char* feature_uuid, const char* type, float value);