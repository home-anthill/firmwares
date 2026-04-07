#pragma once

#include <ArduinoJson.h>

// Blocking variants — retry with delay, ESP.restart() after max failures.
// Used during initial setup on other firmwares.
int register_secure_server(WiFiClientSecure& wifi_client, const char* mac_address, const JsonDocument& features);

int register_insecure_server(WiFiClient& wifi_client, const char* mac_address, const JsonDocument& features);

// Non-blocking single-attempt variants — no retry, no delay, no ESP.restart().
// Used by the thermostat offline-first state machine.
int register_secure_server_once(WiFiClientSecure& wifi_client, const char* mac_address, const JsonDocument& features);

int register_insecure_server_once(WiFiClient& wifi_client, const char* mac_address, const JsonDocument& features);
