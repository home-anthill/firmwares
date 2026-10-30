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
// include the TimeAlarms library (https://www.arduino.cc/reference/en/libraries/timealarms/)
#include <TimeAlarms.h>

// must be the first, before any internal include
#include "config.h"
#include "secrets.h"

// include all local files
#include "wifi_handler.h"
#include "mqtt_handler.h"
#include "registration.h"
#include "storage.h"
#include "barometer_sensor.h"
#include "co2_sensor.h"
#include "display.h"
#include "feature_values.h"
char mac_address[18];

// private functions
void mqtt_callback(char* topic, uint8_t* payload, unsigned int length);
bool get_feature_uuid_by_name(char* featureUuid, size_t max_len, const char* name);
void read_and_send_barometer_value();
void read_and_send_co2_value();
void send_online_status();
void publish_initial_values();
void alarms_init();
void alarms_enable();
void alarms_disable();
void init_sensors();
JsonDocument buildFeatures();

// alarms used to periodically read values from sensors
AlarmID_t alarm_barometer;
AlarmID_t alarm_co2;
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

void read_and_send_barometer_value() {
  Serial.println("read_and_send_barometer_value - called");
  const BarometerReading reading = barometer_read();
  float altitude = reading.altitude;
  if (!isnan(altitude)) {
    Serial.printf("read_and_send_barometer_value - altitude: %.2f m\n", altitude);
    const char* feature_name = "altitude";
    feature_values_set(feature_name, altitude);
    char altitude_feature_uuid[37];
    if (get_feature_uuid_by_name(altitude_feature_uuid, sizeof(altitude_feature_uuid), feature_name)) {
      mqtt_notify_value(saved_device_uuid, altitude_feature_uuid, feature_name, altitude);
    } else {
      Serial.println("read_and_send_barometer_value - feature uuid not found for altitude");
    }
  }
  if (isfinite(reading.iaq)) {
    feature_values_set("iaq", reading.iaq);
    char feature_uuid[37];
    if (get_feature_uuid_by_name(feature_uuid, sizeof(feature_uuid), "iaq")) {
      mqtt_notify_value(saved_device_uuid, feature_uuid, "iaq", reading.iaq);
    } else {
      Serial.println("read_and_send_barometer_value - feature uuid not found for iaq");
    }
  }
  if (isfinite(reading.voc)) {
    feature_values_set("voc", reading.voc);
    char feature_uuid[37];
    if (get_feature_uuid_by_name(feature_uuid, sizeof(feature_uuid), "voc")) {
      mqtt_notify_value(saved_device_uuid, feature_uuid, "voc", reading.voc);
    } else {
      Serial.println("read_and_send_barometer_value - feature uuid not found for voc");
    }
  }
  if (isfinite(reading.eco2)) {
    feature_values_set("eco2", reading.eco2);
    char feature_uuid[37];
    if (get_feature_uuid_by_name(feature_uuid, sizeof(feature_uuid), "eco2")) {
      mqtt_notify_value(saved_device_uuid, feature_uuid, "eco2", reading.eco2);
    } else {
      Serial.println("read_and_send_barometer_value - feature uuid not found for eco2");
    }
  }
  float airpressure = reading.airpressure;
  if (!isnan(airpressure)) {
    Serial.printf("read_and_send_barometer_value - airpressure: %.2f hPa\n", airpressure);
    const char* feature_name = "airpressure";
    feature_values_set(feature_name, airpressure);
    char airpressure_feature_uuid[37];
    if (get_feature_uuid_by_name(airpressure_feature_uuid, sizeof(airpressure_feature_uuid), feature_name)) {
      mqtt_notify_value(saved_device_uuid, airpressure_feature_uuid, feature_name, airpressure);
    } else {
      Serial.println("read_and_send_barometer_value - feature uuid not found for airpressure");
    }
  }
}

void read_and_send_co2_value() {
  const float value = co2_read();
  if (!isfinite(value)) {
    return;
  }
  feature_values_set("co2", value);
  char feature_uuid[37];
  if (get_feature_uuid_by_name(feature_uuid, sizeof(feature_uuid), "co2")) {
    mqtt_notify_value(saved_device_uuid, feature_uuid, "co2", value);
  } else {
    Serial.println("read_and_send_co2_value - feature uuid not found for co2");
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
  read_and_send_barometer_value();
  read_and_send_co2_value();
  send_online_status();
}

void alarms_init() {
  alarm_barometer = Alarm.timerRepeat(30, read_and_send_barometer_value);
  Alarm.disable(alarm_barometer);
  alarm_co2 = Alarm.timerRepeat(5, read_and_send_co2_value);
  Alarm.disable(alarm_co2);
  alarm_online = Alarm.timerRepeat(60, send_online_status);
  Alarm.disable(alarm_online);
}

void alarms_enable() {
  Alarm.enable(alarm_barometer);
  Alarm.enable(alarm_co2);
  Alarm.enable(alarm_online);
}

void alarms_disable() {
  Alarm.disable(alarm_barometer);
  Alarm.disable(alarm_co2);
  Alarm.disable(alarm_online);
}

void init_sensors() {
  barometer_init_sensor();
  co2_init_sensor();
}

void mqtt_callback(char* topic, uint8_t* payload, unsigned int length) {
  Serial.println("mqtt_callback - called");
  // not used for this sensor device
}

JsonDocument buildFeatures() {
  JsonDocument root;
  JsonArray array = root.to<JsonArray>();

  JsonObject co2 = array.add<JsonObject>();
  co2["type"] = "sensor";
  co2["name"] = "co2";
  co2["enable"] = true;
  co2["order"] = 1;
  co2["unit"] = "ppm";
  JsonObject co2Spec = co2["spec"].to<JsonObject>();
  co2Spec["format"] = "int";
  co2Spec["min"] = 0;
  co2Spec["max"] = 40000;
  co2Spec["step"] = 1;

  JsonObject airpressure = array.add<JsonObject>();
  airpressure["type"] = "sensor";
  airpressure["name"] = "airpressure";
  airpressure["enable"] = true;
  airpressure["order"] = 2;
  airpressure["unit"] = "hPa";
  JsonObject airpressureSpec = airpressure["spec"].to<JsonObject>();
  airpressureSpec["format"] = "float";
  airpressureSpec["min"] = 300;
  airpressureSpec["max"] = 1100;
  airpressureSpec["step"] = 0.01;

  JsonObject altitude = array.add<JsonObject>();
  altitude["type"] = "sensor";
  altitude["name"] = "altitude";
  altitude["enable"] = true;
  altitude["order"] = 3;
  altitude["unit"] = "m";
  JsonObject altitudeSpec = altitude["spec"].to<JsonObject>();
  altitudeSpec["format"] = "float";
  altitudeSpec["min"] = -1000;
  altitudeSpec["max"] = 10000;
  altitudeSpec["step"] = 0.5;

  JsonObject iaq = array.add<JsonObject>();
  iaq["type"] = "sensor";
  iaq["name"] = "iaq";
  iaq["enable"] = true;
  iaq["order"] = 4;
  iaq["unit"] = "IAQ";
  JsonObject iaqSpec = iaq["spec"].to<JsonObject>();
  iaqSpec["format"] = "float";
  iaqSpec["min"] = 0;
  iaqSpec["max"] = 500;
  iaqSpec["step"] = 1;

  JsonObject voc = array.add<JsonObject>();
  voc["type"] = "sensor";
  voc["name"] = "voc";
  voc["enable"] = true;
  voc["order"] = 5;
  voc["unit"] = "ppm";
  JsonObject vocSpec = voc["spec"].to<JsonObject>();
  vocSpec["format"] = "float";
  vocSpec["min"] = 0;
  vocSpec["max"] = 1000;
  vocSpec["step"] = 0.001;

  JsonObject eco2 = array.add<JsonObject>();
  eco2["type"] = "sensor";
  eco2["name"] = "eco2";
  eco2["enable"] = true;
  eco2["order"] = 6;
  eco2["unit"] = "ppm";
  JsonObject eco2Spec = eco2["spec"].to<JsonObject>();
  eco2Spec["format"] = "float";
  eco2Spec["min"] = 0;
  eco2Spec["max"] = 40000;
  eco2Spec["step"] = 1;

  JsonObject online = array.add<JsonObject>();
  online["type"] = "sensor";
  online["name"] = "online";
  online["enable"] = true;
  online["order"] = 7;
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
  // BSEC owns the BME688 and must run frequently, independently of publishing.
  barometer_update();
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
  // triggers disconnect(), but that only fires at the next alarm tick (30-45 s).
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
