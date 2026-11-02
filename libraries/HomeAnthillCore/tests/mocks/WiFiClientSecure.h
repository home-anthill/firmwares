#pragma once

// ---------------------------------------------------------------------------
// WiFiClientSecure mock for host-side (native) unit test compilation.
// ---------------------------------------------------------------------------

#include "WiFi.h"

class WiFiClientSecure : public WiFiClient {
public:
  void setInsecure() {}
  void setCACert(const char* /*cert*/) {}
};
