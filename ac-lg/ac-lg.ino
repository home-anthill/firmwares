// include the WiFi library and HTTPClient
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
// include json library (https://github.com/bblanchon/ArduinoJson)
#include <ArduinoJson.h>
// include MQTT library
#include <PubSubClient.h>
// eeprom lib has been deprecated for esp32, the recommended way is to use Preferences
#include <Preferences.h>

// must be the first, before any internal include
#include "secrets.h"

// include all local files
#include "wifi_handler.h"
#include "mqtt_handler.h"
#include "registration.h"
#include "storage.h"
#include "ir_lg.h"

char mac_address[18];

// private functions
void mqtt_callback(char* topic, uint8_t* payload, unsigned int length);
bool get_feature_uuid_by_name(char* featureUuid, size_t max_len, const char* name);
JsonDocument buildFeatures();

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
}

JsonDocument buildFeatures() {
  JsonDocument root;
  JsonArray array = root.to<JsonArray>();

  JsonDocument on = array.add<JsonObject>();
  on["type"] = "controller";
  on["name"] = "on";
  on["enable"] = true;
  on["order"] = 1;
  on["unit"] = "-";
  JsonObject onSpec = on["spec"].to<JsonObject>();
  onSpec["format"] = "bool";
  
  JsonDocument setpoint = array.add<JsonObject>();
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
  setpointSpec["list"].to<JsonArray>();

  JsonDocument mode = array.add<JsonObject>();
  mode["type"] = "controller";
  mode["name"] = "mode";
  mode["enable"] = true;
  mode["order"] = 3;
  mode["unit"] = "-";
  JsonObject modeSpec = mode["spec"].to<JsonObject>();
  modeSpec["format"] = "list";
  JsonArray modeList = modeSpec["list"].to<JsonArray>();
  JsonObject modeItem0 = modeList.add<JsonObject>();
  modeItem0["value"] = "0";
  modeItem0["text"] = "cool";
  JsonObject modeItem1 = modeList.add<JsonObject>();
  modeItem1["value"] = "1";
  modeItem1["text"] = "auto";
  JsonObject modeItem2 = modeList.add<JsonObject>();
  modeItem2["value"] = "2";
  modeItem2["text"] = "heat";
  JsonObject modeItem3 = modeList.add<JsonObject>();
  modeItem3["value"] = "3";
  modeItem3["text"] = "fan";
  JsonObject modeItem4 = modeList.add<JsonObject>();
  modeItem4["value"] = "4";
  modeItem4["text"] = "dry";

  JsonDocument fanSpeed = array.add<JsonObject>();
  fanSpeed["type"] = "controller";
  fanSpeed["name"] = "fanSpeed";
  fanSpeed["enable"] = true;
  fanSpeed["order"] = 4;
  fanSpeed["unit"] = "-";
  JsonObject fanSpeedSpec = fanSpeed["spec"].to<JsonObject>();
  fanSpeedSpec["format"] = "list";
  JsonArray fanSpeedList = fanSpeedSpec["list"].to<JsonArray>();
  JsonObject fanSpeedItem0 = fanSpeedList.add<JsonObject>();
  fanSpeedItem0["value"] = "0";
  fanSpeedItem0["text"] = "min";
  JsonObject fanSpeedItem1 = fanSpeedList.add<JsonObject>();
  fanSpeedItem1["value"] = "1";
  fanSpeedItem1["text"] = "med";
  JsonObject fanSpeedItem2 = fanSpeedList.add<JsonObject>();
  fanSpeedItem2["value"] = "2";
  fanSpeedItem2["text"] = "max";
  JsonObject fanSpeedItem3 = fanSpeedList.add<JsonObject>();
  fanSpeedItem3["value"] = "3";
  fanSpeedItem3["text"] = "auto";
  JsonObject fanSpeedItem4 = fanSpeedList.add<JsonObject>();
  fanSpeedItem4["value"] = "4";
  fanSpeedItem4["text"] = "auto0";

  return root;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("setup - starting...");
  
  // 0. configure hardware
  //    disable ESP builtin LED
  //    but not all ESP boards have this variable defined, so I should check the existance of `LED_BUILTIN`.
  #ifdef LED_BUILTIN
    // disable ESP32 Devkit-C built-in LED
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
  #endif

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
  wifi_connect(mac_address);

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

  // 5. read device UUID from preferences
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

  // 6. read features array from preferences
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

  // 7. init IR library
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
    delay(60000);
    return;
  }

  // if not connected to the wifi, try to reconnect
  if (wifi_get_status() != WL_CONNECTED) {
    Serial.println("loop - WiFi connection lost!");
    wifi_reconnect(mac_address);
  }

  // if not connected to mqtt server, try to reconnect
  if (!mqtt_client.connected()) {
    Serial.println("loop - mqtt connecting...");
    mqtt_connect(saved_device_uuid);
  }

  // mqtt_client.loop() returns false when the connection is broken.
  // For controller firmwares that never publish, this is the only way to detect
  // a stale TCP socket after the broker restarts (there is no publish failure
  // to trigger an explicit disconnect like there is in sensor firmwares).
  if (!mqtt_client.loop()) {
    Serial.println("loop - mqtt_client.loop() returned false, forcing disconnect to trigger reconnect");
    mqtt_client.disconnect();
  }

  // 100 ms keeps MQTT responsive; bare delay() is correct here (no TimeAlarms)
  delay(100);
}
