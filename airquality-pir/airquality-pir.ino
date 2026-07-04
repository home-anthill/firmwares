// include wifi library
#include <WiFi.h>
#include <WiFiClientSecure.h>
// include http library (also required to use 'WiFiClientSecure')
#include <HTTPClient.h>
// include MQTT library (https://pubsubclient.knolleary.net/api)
#include <PubSubClient.h>
// include the TimeAlarms library (https://www.arduino.cc/reference/en/libraries/timealarms/)
#include <TimeAlarms.h>

// must be the first, before any internal include
#include "secrets.h"

// include all local files
#include "wifi_handler.h"
#include "mqtt_handler.h"
#include "registration.h"
#include "storage.h"
#include "airquality_sensor.h"
#include "pir_sensor.h"
#include "display.h"
#include "feature_values.h"

// build-in RGB LED
#define BOARD_RGB_LED_PIN 38
#define DISPLAY_BUTTON_PIN 42

char mac_address[18];

// private functions
void mqtt_callback(char* topic, uint8_t* payload, unsigned int length);
bool get_feature_uuid_by_name(char* featureUuid, size_t max_len, const char* name);
void read_and_send_airquality_value();
void read_and_send_pir_value();
void send_online_status();
void alarms_init();
void alarms_enable();
void alarms_disable();
void init_sensors();
JsonDocument buildFeatures();

// alarms used to periodically read values from sensors
AlarmID_t alarm_airquality;
AlarmID_t alarm_pir;
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

void read_and_send_airquality_value() {
  Serial.println("read_and_send_airquality_value - called");
  bool hasNewValue = airquality_has_newvalue();
  int value = airquality_get_value();
  Serial.printf("read_and_send_airquality_value - hasNewValue=%d\n", hasNewValue);
  // notifiy only if the value is changed to prevent useless notifications
  if (hasNewValue) {
    const char* feature_name = "airquality";
    feature_values_set(feature_name, value);
    char feature_uuid[37];
    if (get_feature_uuid_by_name(feature_uuid, sizeof(feature_uuid), feature_name)) {
      mqtt_notify_value(saved_device_uuid, feature_uuid, feature_name, value);
    } else {
      Serial.println("read_and_send_airquality_value - feature uuid not found for airquality");
    }
  }
}

void read_and_send_pir_value() {
  Serial.println("read_and_send_pir_value - called");
  int pre_value = pir_get_prev_value();
  int new_value = pir_get_value();
  Serial.printf("read_and_send_pir_value - new_value read = %d\n", new_value);
  // notifiy only if the value is changed to prevent useless notifications
  if (pre_value != new_value) {
    const char* feature_name = "motion";
    feature_values_set(feature_name, new_value);
    char feature_uuid[37];
    if (get_feature_uuid_by_name(feature_uuid, sizeof(feature_uuid), feature_name)) {
      mqtt_notify_value(saved_device_uuid, feature_uuid, feature_name, new_value);
    } else {
      Serial.println("read_and_send_pir_value - feature uuid not found for motion");
    }
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
  read_and_send_airquality_value();
  read_and_send_pir_value();
  send_online_status();
}

void alarms_init() {
  alarm_airquality = Alarm.timerRepeat(50, read_and_send_airquality_value);
  Alarm.disable(alarm_airquality);
  alarm_pir = Alarm.timerRepeat(30, read_and_send_pir_value);
  Alarm.disable(alarm_pir);
  alarm_online = Alarm.timerRepeat(60, send_online_status);
  Alarm.disable(alarm_online);
}

void alarms_enable() {
  Alarm.enable(alarm_airquality);
  Alarm.enable(alarm_pir);
  Alarm.enable(alarm_online);
}

void alarms_disable() {
  Alarm.disable(alarm_airquality);
  Alarm.disable(alarm_pir);
  Alarm.disable(alarm_online);
}

void init_sensors() {
  airquality_init_sensor();
  pir_init_sensor();
}

void mqtt_callback(char* topic, uint8_t* payload, unsigned int length) {
  Serial.println("mqtt_callback - called");
  display_sleep_for(30000);
  // not used for this sensor device
}

JsonDocument buildFeatures() {
  JsonDocument root;
  JsonArray array = root.to<JsonArray>();

  JsonObject airquality = array.add<JsonObject>();
  airquality["type"] = "sensor";
  airquality["name"] = "airquality";
  airquality["enable"] = true;
  airquality["order"] = 1;
  airquality["unit"] = "-";
  JsonObject airqualitySpec = airquality["spec"].to<JsonObject>();
  airqualitySpec["format"] = "int";
  airqualitySpec["min"] = 0;
  airqualitySpec["max"] = 3;
  airqualitySpec["step"] = 1;

  JsonObject motion = array.add<JsonObject>();
  motion["type"] = "sensor";
  motion["name"] = "motion";
  motion["enable"] = true;
  motion["order"] = 2;
  motion["unit"] = "-";
  JsonObject motionSpec = motion["spec"].to<JsonObject>();
  motionSpec["format"] = "bool";

  JsonObject online = array.add<JsonObject>();
  online["type"] = "sensor";
  online["name"] = "online";
  online["enable"] = true;
  online["order"] = 4;
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

  // 8. init sensors
  init_sensors();

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
    // starts alarms
    alarms_enable();
  }

  // Defense in depth: mqtt_client.loop() returns false if the connection is
  // broken (e.g. broker restarted). For sensors, publish failure already
  // triggers disconnect(), but that only fires at the next alarm tick (30-50 s).
  // Checking here catches stale connections via PINGREQ timeout (~15 s)
  // without waiting for the next publish.
  if (!mqtt_client.loop()) {
    Serial.println("loop - mqtt_client.loop() returned false, forcing disconnect to trigger reconnect");
    mqtt_client.disconnect();
    display_set_connectivity_status(wifi_get_status() == WL_CONNECTED, false);
    update_display();
  }

  update_display();
  Alarm.delay(100);
}
