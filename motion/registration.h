#pragma once

#include <ArduinoJson.h>

int register_secure_server(WiFiClientSecure& wifi_client, const char* mac_address, const JsonDocument& features);

int register_insecure_server(WiFiClient& wifi_client, const char* mac_address, const JsonDocument& features);
