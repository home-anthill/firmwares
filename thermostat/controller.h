#pragma once

float get_setpoint();
float get_tolerance();

void set_configuration(char* saved_device_uuid, JsonArray saved_features, uint8_t* payload, unsigned int length);