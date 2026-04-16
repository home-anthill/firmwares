#pragma once

float get_setpoint();
float get_tolerance();

void set_configuration(const char* saved_device_uuid, const char* saved_mac_address,
                       JsonArray saved_features, uint8_t* payload,
                       unsigned int length);
