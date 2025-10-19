#include <ArduinoJson.h>

size_t storage_get_uuid(char* saved_uuid);

size_t storage_set_uuid(const char* uuid);

void storage_get_features(JsonArray features);

size_t storage_set_features(JsonArray features);