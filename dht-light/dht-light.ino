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
#include "dht_sensor.h"
#include "light_sensor.h"

char mac_address[18];

// private functions
void mqtt_callback(char* topic, uint8_t* payload, unsigned int length);
bool get_feature_uuid_by_name(char* featureUuid, size_t max_len, const char* name);
void read_dht_sensor_value();
void read_light_sensor_value();
void alarms_init();
void alarms_enable();
void alarms_disable();
void init_sensors();
JsonDocument buildFeatures();

// alarms used to periodically read values from sensors
AlarmID_t alarm_dht;
AlarmID_t alarm_light;

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

void read_dht_sensor_value() {
  Serial.println("read_dht_sensor_value - called");
  float temp = dht_get_temperature();
  if (isnan(temp)) {
      Serial.println("read_dht_sensor_value - error reading temperature!");
  } else {
      Serial.printf("read_dht_sensor_value - temperature: %.2f °C\n", temp);
      const char* feature_name = "temperature";
      char temp_feature_uuid[37];
      if (get_feature_uuid_by_name(temp_feature_uuid, sizeof(temp_feature_uuid), feature_name)) {
        mqtt_notify_value(saved_device_uuid, temp_feature_uuid, feature_name, temp);
      } else {
        Serial.println("read_dht_sensor_value - feature uuid not found for temperature");
      }
  }

  float hum = dht_get_humidity();
  if (isnan(hum)) {
      Serial.println("read_dht_sensor_value - error reading humidity!");
  } else {
      Serial.printf("read_dht_sensor_value - humidity: %.2f %%\n", hum);
      const char* feature_name = "humidity";
      char hum_feature_uuid[37];
      if (get_feature_uuid_by_name(hum_feature_uuid, sizeof(hum_feature_uuid), feature_name)) {
        mqtt_notify_value(saved_device_uuid, hum_feature_uuid, feature_name, hum);
      } else {
        Serial.println("read_dht_sensor_value - feature uuid not found for humidity");
      }
  }
}

void read_light_sensor_value() {
  long value = light_get_value();
  Serial.printf("read_light_sensor_value - light: %ld lux\n", value);
  const char* feature_name = "light";
  char light_feature_uuid[37];
  if (get_feature_uuid_by_name(light_feature_uuid, sizeof(light_feature_uuid), feature_name)) {
    mqtt_notify_value(saved_device_uuid, light_feature_uuid, feature_name, value);
  } else {
    Serial.println("read_light_sensor_value - feature uuid not found for light");
  }
}

void alarms_init() {
  alarm_dht = Alarm.timerRepeat(30, read_dht_sensor_value);
  Alarm.disable(alarm_dht);
  alarm_light = Alarm.timerRepeat(45, read_light_sensor_value);
  Alarm.disable(alarm_light);
}

void alarms_enable() {
  Alarm.enable(alarm_dht);
  Alarm.enable(alarm_light);
}

void alarms_disable() {
  Alarm.disable(alarm_dht);
  Alarm.disable(alarm_light);
}

void init_sensors() {
  dht_init_sensor();
  light_init_sensor();
}

void mqtt_callback(char* topic, uint8_t* payload, unsigned int length) {
  Serial.println("mqtt_callback - called");
  // not used for this sensor device
}

JsonDocument buildFeatures() {
  JsonDocument root;
  JsonArray array = root.to<JsonArray>();

  JsonObject temperature = array.add<JsonObject>();
  temperature["type"] = "sensor";
  temperature["name"] = "temperature";
  temperature["enable"] = true;
  temperature["order"] = 1;
  temperature["unit"] = "°C";
  JsonObject temperatureSpec = temperature["spec"].to<JsonObject>();
  temperatureSpec["format"] = "float";
  temperatureSpec["min"] = -40; // specified in DHT22 doc
  temperatureSpec["max"] = 80; // specified in DHT22 doc
  temperatureSpec["step"] = 0.05; // specified in DHT22 doc

  JsonObject humidity = array.add<JsonObject>();
  humidity["type"] = "sensor";
  humidity["name"] = "humidity";
  humidity["enable"] = true;
  humidity["order"] = 2;
  humidity["unit"] = "%";
  JsonObject humiditySpec = humidity["spec"].to<JsonObject>();
  humiditySpec["format"] = "float";
  humiditySpec["min"] = 0;
  humiditySpec["max"] = 100;
  humiditySpec["step"] = 2.5; // specified in DHT22 doc

  JsonObject light = array.add<JsonObject>();
  light["type"] = "sensor";
  light["name"] = "light";
  light["enable"] = true;
  light["order"] = 3;
  light["unit"] = "lux";
  JsonObject lightSpec = light["spec"].to<JsonObject>();
  lightSpec["format"] = "int";
  lightSpec["min"] = 0;
  lightSpec["max"] = 40000; // specified in TSL2561 doc
  lightSpec["step"] = 1;

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
  // set time to Saturday 00:00:00am Jan 1 2021
  setTime(0,0,0,1,1,25);

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
    alarms_disable();
    Serial.println("loop - WiFi connection lost!");
    wifi_reconnect(mac_address);
  }

  // if not connected to mqtt server, try to reconnect
  if (!mqtt_client.connected()) {
    Serial.println("loop - mqtt connecting...");
    mqtt_connect(saved_device_uuid);
    // starts alarms to read sensors values
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
  }

  Alarm.delay(100);
}