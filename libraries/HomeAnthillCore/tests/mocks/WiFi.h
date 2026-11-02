#pragma once

// ---------------------------------------------------------------------------
// WiFi mock for host-side (native) unit test compilation.
// Provides Client and WiFiClient stubs used by registration and mqtt_handler.
// ---------------------------------------------------------------------------

#include <cstdint>

// Arduino's Client abstract base — required by PubSubClient and HTTPClient.
class Client {
public:
  virtual ~Client() = default;
};

class WiFiClient : public Client {};

// WiFi connection status codes.
#define WL_CONNECTED 3

#include <Arduino.h>

struct IPAddress {
  String toString() const {
    return "127.0.0.1";
  }
};

struct WifiMock {
  std::string ssid;
  std::string password;
  int begin_count = 0;
  void begin(const char* network, const char* pass) {
    ssid = network;
    password = pass;
    begin_count++;
  }
  int status() const {
    return WL_CONNECTED;
  }
  IPAddress localIP() const {
    return {};
  }
  String macAddress() const {
    return "aa:bb:cc:dd:ee:ff";
  }
};
inline WifiMock WiFi;
