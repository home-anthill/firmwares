#pragma once

void ir_init();

void ir_send_command(char* topic, uint8_t* payload, unsigned int length);