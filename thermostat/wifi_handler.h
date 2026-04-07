#pragma once

# if SSL==true
extern WiFiClientSecure wifi_client;
# else 
extern WiFiClient wifi_client;
# endif

void wifi_init_ca();

void wifi_connect(char* mac_address);

void wifi_reconnect(char* mac_address);

int wifi_get_status();

// Non-blocking helpers for the thermostat offline-first state machine.
// wifi_start_connect() initiates the WiFi association without blocking;
// wifi_populate_mac() reads the MAC address once the connection is up.
void wifi_start_connect();

void wifi_populate_mac(char* mac_address);