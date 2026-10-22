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
