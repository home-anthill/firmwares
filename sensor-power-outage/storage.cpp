// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>

// include Preferences, because eeprom lib has been deprecated for esp32
#include <Preferences.h>

Preferences preferences;

String storage_get_uuid() {
  preferences.begin("device", false); 
  String saved_uuid = preferences.getString("uuid", "");
  preferences.end();
  return saved_uuid;
}

size_t storage_set_uuid(String uuid) {
  preferences.begin("device", false); 
  size_t len = preferences.putString("uuid", uuid);
  preferences.end();
  return len;
}