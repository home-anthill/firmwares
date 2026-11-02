#pragma once

#include <ArduinoJson.h>
#include <WiFi.h>

namespace ha {
struct RegistrationConfig {
  const char* manufacturer;
  const char* model;
  const char* api_token;
  const char* domain;
  int port;
  const char* path;
  bool tls;
};
// retry_network_errors preserves the sensor/controller blocking retry policy.
// false performs exactly one HTTP attempt (thermostat state machine).
int register_server(WiFiClient& client, const char* mac_address,
                    const JsonDocument& features, const RegistrationConfig& config,
                    bool retry_network_errors);
}
