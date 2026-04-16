#pragma once

void ir_init();

void ir_send_command(const char* saved_device_uuid, const char* saved_mac_address,
                     JsonArray saved_features, char* topic, uint8_t* payload,
                     unsigned int length);
