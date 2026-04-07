#pragma once

#include <functional>

extern PubSubClient mqtt_client;

void mqtt_init(Client& wifi_client, std::function<void (char *, uint8_t *, unsigned int)> callback);

void mqtt_connect(const char* uuid);

// Single MQTT connection attempt with no retry loop or blocking delay.
// Returns true on success (also subscribes); false otherwise.
// Used by the thermostat offline-first state machine.
bool mqtt_try_connect_once(const char* uuid);

void mqtt_notify_value(const char* device_uuid, const char* feature_uuid, const char* type, float value);