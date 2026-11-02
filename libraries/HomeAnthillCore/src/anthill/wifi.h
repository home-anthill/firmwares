#pragma once

#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace ha {
struct WifiConfig {
  const char* ssid;
  const char* password;
};
extern const char* const ca_cert;
void wifi_sync_time();
void wifi_start_connect(const WifiConfig& config);
void wifi_populate_mac(char* mac_address);
void wifi_connect(const WifiConfig& config, char* mac_address);
void wifi_reconnect(const WifiConfig& config, char* mac_address);
int wifi_get_status();
}
