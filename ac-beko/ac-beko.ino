// include the WiFi library and HTTPClient
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
// include json library (https://github.com/bblanchon/ArduinoJson)
#include <ArduinoJson.h>
// include MQTT library
#include <PubSubClient.h>
// include the TimeAlarms library (https://www.arduino.cc/reference/en/libraries/timealarms/)
#include <TimeAlarms.h>
// include COOLIX protocol constants used for registration values
#ifndef SEND_COOLIX
#define SEND_COOLIX 1
#endif
#include <ir_Coolix.h>
// eeprom lib has been deprecated for esp32, the recommended way is to use Preferences
#include <Preferences.h>

// must be the first, before any internal include
#include "secrets.h"

// include all local files
#include "wifi_handler.h"
#include "mqtt_handler.h"
#include "registration.h"
#include "storage.h"
#include "ir_beko_controller.h"
#include "display.h"
#include "feature_values.h"

// build-in RGB LED
#define BOARD_RGB_LED_PIN 38
#define DISPLAY_BUTTON_PIN 42

char mac_address[18];

// private functions
void mqtt_callback(char* topic, uint8_t* payload, unsigned int length);
bool get_feature_uuid_by_name(char* featureUuid, size_t max_len, const char* name);
void record_command_values(uint8_t* payload, unsigned int length);
void send_online_status();
void publish_initial_values();
void alarms_init();
void alarms_enable();
void alarms_disable();
JsonDocument buildFeatures();

// alarms used to periodically publish values
AlarmID_t alarm_online;

// device_uuid global variable
char saved_device_uuid[37];
// features array global variable
JsonDocument doc_features;
JsonArray saved_features = doc_features.to<JsonArray>();

bool get_feature_uuid_by_name(char* featureUuid, size_t max_len, const char* name) {
  for (int i = 0; i < saved_features.size(); i++) {
    JsonObject feature = saved_features[i];
    const char* uuidval = feature["uuid"];
    const char* nameval = feature["name"];
    if (uuidval == nullptr || nameval == nullptr) {
      continue;
    }
    if (strcmp(name, nameval) == 0) {
      strncpy(featureUuid, uuidval, max_len - 1);
      featureUuid[max_len - 1] = '\0';
      return true;
    }
  }
  featureUuid[0] = '\0';
  return false;
}

void mqtt_callback(char* topic, uint8_t* payload, unsigned int length) {
  Serial.println("mqtt_callback - called");
  ir_send_command(saved_device_uuid, mac_address, saved_features, topic, payload,
                  length);
  record_command_values(payload, length);
  display_show_message("Command", "Received");
}

void record_command_values(uint8_t* payload, unsigned int length) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("record_command_values - deserializeJson failed: ");
    Serial.println(error.c_str());
    return;
  }

  JsonArray command_values = doc.as<JsonArray>();
  if (command_values.isNull()) {
    return;
  }

  for (JsonObject command_value : command_values) {
    const char* feature_name = command_value["featureName"];
    if (feature_name == nullptr) {
      continue;
    }

    JsonVariant value = command_value["payload"]["value"];
    if (value.isNull()) {
      value = command_value["value"];
    }
    if (value.isNull() || !(value.is<bool>() || value.is<int>() || value.is<float>())) {
      continue;
    }

    feature_values_set(feature_name, value.as<float>());
  }
}

void send_online_status() {
  Serial.println("send_online_status - called");
  const char* feature_name = "online";
  feature_values_set(feature_name, 1);
  char feature_uuid[37];
  if (get_feature_uuid_by_name(feature_uuid, sizeof(feature_uuid), feature_name)) {
    mqtt_notify_value(saved_device_uuid, feature_uuid, feature_name, 1);
  } else {
    Serial.println("send_online_status - feature uuid not found for online");
  }
}

void publish_initial_values() {
  Serial.println("publish_initial_values - called");
  send_online_status();
}

void alarms_init() {
  alarm_online = Alarm.timerRepeat(60, send_online_status);
  Alarm.disable(alarm_online);
}

void alarms_enable() {
  Alarm.enable(alarm_online);
}

void alarms_disable() {
  Alarm.disable(alarm_online);
}

JsonDocument buildFeatures() {
  JsonDocument root;
  JsonArray array = root.to<JsonArray>();

  JsonObject on = array.add<JsonObject>();
  on["type"] = "controller";
  on["name"] = "on";
  on["enable"] = true;
  on["order"] = 1;
  on["unit"] = "-";
  JsonObject onSpec = on["spec"].to<JsonObject>();
  onSpec["format"] = "bool";
  
  JsonObject setpoint = array.add<JsonObject>();
  setpoint["type"] = "controller";
  setpoint["name"] = "setpoint";
  setpoint["enable"] = true;
  setpoint["order"] = 2;
  setpoint["unit"] = "°C";
  JsonObject setpointSpec = setpoint["spec"].to<JsonObject>();
  setpointSpec["format"] = "int";
  setpointSpec["min"] = 17;
  setpointSpec["max"] = 30;
  setpointSpec["step"] = 1;

  JsonObject mode = array.add<JsonObject>();
  mode["type"] = "controller";
  mode["name"] = "mode";
  mode["enable"] = true;
  mode["order"] = 3;
  mode["unit"] = "-";
  JsonObject modeSpec = mode["spec"].to<JsonObject>();
  modeSpec["format"] = "list";
  JsonArray modeList = modeSpec["list"].to<JsonArray>();
  JsonObject modeItem0 = modeList.add<JsonObject>();
  modeItem0["value"] = kCoolixCool;
  modeItem0["text"] = "Cool";
  JsonObject modeItem1 = modeList.add<JsonObject>();
  modeItem1["value"] = kCoolixDry;
  modeItem1["text"] = "Dry";
  JsonObject modeItem2 = modeList.add<JsonObject>();
  modeItem2["value"] = kCoolixAuto;
  modeItem2["text"] = "Auto";
  JsonObject modeItem3 = modeList.add<JsonObject>();
  modeItem3["value"] = kCoolixHeat;
  modeItem3["text"] = "Heat";
  JsonObject modeItem4 = modeList.add<JsonObject>();
  modeItem4["value"] = kCoolixFan;
  modeItem4["text"] = "Fan";

  JsonObject fanSpeed = array.add<JsonObject>();
  fanSpeed["type"] = "controller";
  fanSpeed["name"] = "fanSpeed";
  fanSpeed["enable"] = true;
  fanSpeed["order"] = 4;
  fanSpeed["unit"] = "-";
  JsonObject fanSpeedSpec = fanSpeed["spec"].to<JsonObject>();
  fanSpeedSpec["format"] = "list";
  JsonArray fanSpeedList = fanSpeedSpec["list"].to<JsonArray>();
  JsonObject fanSpeedItem0 = fanSpeedList.add<JsonObject>();
  fanSpeedItem0["value"] = kCoolixFanAuto0;
  fanSpeedItem0["text"] = "Auto0";
  JsonObject fanSpeedItem1 = fanSpeedList.add<JsonObject>();
  fanSpeedItem1["value"] = kCoolixFanMax;
  fanSpeedItem1["text"] = "Max";
  JsonObject fanSpeedItem2 = fanSpeedList.add<JsonObject>();
  fanSpeedItem2["value"] = kCoolixFanMed;
  fanSpeedItem2["text"] = "Med";
  JsonObject fanSpeedItem3 = fanSpeedList.add<JsonObject>();
  fanSpeedItem3["value"] = kCoolixFanMin;
  fanSpeedItem3["text"] = "Min";
  JsonObject fanSpeedItem4 = fanSpeedList.add<JsonObject>();
  fanSpeedItem4["value"] = kCoolixFanAuto;
  fanSpeedItem4["text"] = "Auto";

  JsonObject online = array.add<JsonObject>();
  online["type"] = "sensor";
  online["name"] = "online";
  online["enable"] = true;
  online["order"] = 5;
  online["unit"] = "-";
  JsonObject onlineSpec = online["spec"].to<JsonObject>();
  onlineSpec["format"] = "bool";

  return root;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("setup - starting...");
  init_display(DISPLAY_BUTTON_PIN);

  // 0. configure hardware
  rgbLedWrite(BOARD_RGB_LED_PIN, 0, 0, 0); 
  // set time to Saturday 00:00:00am Jan 1 2025
  setTime(0, 0, 0, 1, 1, 25);

  // 1. prepare wifi_client
  //    If SSL is enabled, add root ca to wifi_client to be used for secure connections
  //    This root ca will be used by all network components like mqtt and htpp
  # if SSL==true
    Serial.println("setup - running with SSL enabled");
    wifi_init_ca();
  # else 
    Serial.println("setup - running WITHOUT SSL");
  # endif

  // 2. init MQTT client
  //    init mqtt_client with wifi_client (previously configured)
  Serial.println("setup - init mqtt...");
  mqtt_init(wifi_client, mqtt_callback);

  // 3. connect to wifi
  Serial.println("setup - connect wifi...");
  display_set_connectivity_status(false, false);
  update_display();
  wifi_connect(mac_address);
  display_set_connectivity_status(true, false);
  display_show_message("WiFi status", "Online");

  // 4. register to the server
  Serial.println("setup - registering this device...");
  JsonDocument features = buildFeatures();
  int result = -1;
  # if SSL==true
    result = register_secure_server(wifi_client, mac_address, features);
  # else 
    result = register_insecure_server(wifi_client, mac_address, features);
  # endif
  Serial.printf("setup - register returned result = %d\n", result);
  if (result != 0) {
    Serial.printf("setup - register error, returned result = %d\n", result);
    return;
  }

  // 5. instantiate alarms, but disable them
  Serial.println("setup - init alarms (still disabled)...");
  alarms_init();

  // 6. read device UUID from preferences
  //    if it's the first boot, it will be the same already stored in global variable 'saved_device_uuid',
  //    because already filled during the registration process.
  //    Otherwise, it reads the existing UUID form preferences,
  //    because registration won't return the UUID again.
  Serial.println("setup - getting saved UUID from preferences...");
  size_t uuid_len = storage_get_uuid(saved_device_uuid);
  Serial.printf("setup - saved_device_uuid = %s\n", saved_device_uuid);
  if (uuid_len != 37 || strlen(saved_device_uuid) == 0) {
    Serial.println("************* ERROR **************");
    Serial.println("setup - Cannot read saved UUID from Preferences");
    Serial.println("**********************************");
    return;
  }

  // 7. read features array from preferences
  //    if it's the first boot, it will be the same already stored in global variable 'saved_features',
  //    because already filled during the registration process. 
  //    Otherwise, it reads the existing features array form preferences,
  //    because registration won't return the UUID again.
  Serial.println("setup - getting features array from preferences...");
  storage_get_features(saved_features);
  if (saved_features.size() == 0) {
    Serial.println("************* ERROR **************");
    Serial.println("setup - Cannot read saved features array from Preferences");
    Serial.println("**********************************");
    return;
  }
  feature_values_init(saved_features);

  // 8. init IR library
  // Run the calibration to calculate uSec timing offsets for this platform.
  // This will produce a 65ms IR signal pulse at 38kHz.
  // Only ever needs to be run once per object instantiation, if at all.
  ir_init();
  
  delay(1000);
}

void loop() {
  // if 'saved_device_uuid' is not defined, it's an unregistered device
  if (strlen(saved_device_uuid) == 0) {
    Serial.println("loop - saved_device_uuid NOT FOUND, cannot continue...");
    Alarm.delay(60000);
    return;
  }

  // if not connected to the wifi, try to reconnect
  if (wifi_get_status() != WL_CONNECTED) {
    display_set_connectivity_status(false, false);
    update_display();
    alarms_disable();
    Serial.println("loop - WiFi connection lost!");
    wifi_reconnect(mac_address);
    display_set_connectivity_status(true, mqtt_client.connected());
    display_show_message("WiFi status", "Online");
  }

  // if not connected to mqtt server, try to reconnect
  if (!mqtt_client.connected()) {
    display_set_connectivity_status(true, false);
    update_display();
    Serial.println("loop - mqtt connecting...");
    mqtt_connect(saved_device_uuid);
    display_set_connectivity_status(true, true);
    display_show_message("MQTT status", "Online");
    publish_initial_values();
    alarms_enable();
  }

  // mqtt_client.loop() returns false when the connection is broken.
  // For controller firmwares that never publish, this is the only way to detect
  // a stale TCP socket after the broker restarts (there is no publish failure
  // to trigger an explicit disconnect like there is in sensor firmwares).
  if (!mqtt_client.loop()) {
    Serial.println("loop - mqtt_client.loop() returned false, forcing disconnect to trigger reconnect");
    mqtt_client.disconnect();
    display_set_connectivity_status(wifi_get_status() == WL_CONNECTED, false);
    update_display();
  }

  update_display();
  Alarm.delay(100);
}
