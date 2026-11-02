#pragma once

#include <HomeAnthillCore.h>

// Optional sketch-side configuration bridge. Include after local settings.
// Library implementations must not include this header.
#ifndef SECRET_SSID
#error "Include your local secrets.h before HomeAnthillConfig.h"
#endif

inline const ha::WifiConfig wifi_config{SECRET_SSID, SECRET_PASS};

#if MQTT_AUTH==true
inline const ha::MqttConfig mqtt_config{
  MQTT_URL, MQTT_PORT, true, MQTT_USERNAME, MQTT_PASSWORD, API_TOKEN
};
#else
inline const ha::MqttConfig mqtt_config{
  MQTT_URL, MQTT_PORT, false, nullptr, nullptr, API_TOKEN
};
#endif

inline const ha::RegistrationConfig registration_config{
  MANUFACTURER, MODEL, API_TOKEN, SERVER_DOMAIN, SERVER_PORT, SERVER_PATH, SSL
};
