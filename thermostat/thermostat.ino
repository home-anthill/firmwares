// include wifi library
#include <WiFi.h>
#include <WiFiClientSecure.h>
// include http library (also required to use 'WiFiClientSecure')
#include <HTTPClient.h>
// include json library (https://github.com/bblanchon/ArduinoJson)
#include <ArduinoJson.h>
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
#include "temp_sensor.h"
#include "controller.h"
#include "display.h"

char mac_address[18];

// alarms used to periodically read values from sensors
AlarmID_t alarm_temp;

// private functions
void mqtt_callback(char* topic, byte* payload, unsigned int length);

// device_uuid global variable
char saved_device_uuid[37];
// features array global variable
JsonDocument doc_features;
JsonArray saved_features = doc_features.to<JsonArray>();

// outputs
#define HEAT 33
#define COLD 34
#define FAN 35
#define PUMP 36


void get_feature_uuid_by_name(char* featureUuid, const char* name) {
  for (int i = 0; i < saved_features.size(); i++) {
    JsonObject feature = saved_features[i];
    const char* uuidval = feature["uuid"];
    const char* nameval = feature["name"];
    if (strcmp(name, nameval) == 0) {
      strcpy(featureUuid, uuidval);
      return;
    }
  }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("mqtt_callback - called");
  set_configuration(saved_device_uuid, saved_features, payload);
}

JsonDocument buildFeatures() {
  JsonDocument root;
  JsonArray array = root.to<JsonArray>();
  JsonDocument f1;
  f1["type"] = "controller";
  f1["name"] = "setpoint";
  f1["enable"] = true;
  f1["order"] = 1;
  f1["unit"] = "°C";
  array.add(f1);
  JsonDocument f2;
  f2["type"] = "controller";
  f2["name"] = "tolerance";
  f2["enable"] = true;
  f2["order"] = 2;
  f2["unit"] = "°C";
  array.add(f2);
  JsonDocument f3;
  f3["type"] = "sensor";
  f3["name"] = "temperature";
  f3["enable"] = true;
  f3["order"] = 3;
  f3["unit"] = "°C";
  array.add(f3);
  return root;
}

void read_temp_sensor_value() {
  Serial.println("read_temp_sensor_value - called");
  float temp = temp_get_temperature();
  if (isnan(temp)) {
      Serial.println("read_temp_sensor_value - error reading temperature!");
  } else {
      Serial.printf("read_temp_sensor_value - temperature: %.2f °C\n", temp);
      update_display(temp);

      if (mqtt_client.connected()) {
        const char* feature_name = "temperature";
        char temp_feature_uuid[37];
        get_feature_uuid_by_name(temp_feature_uuid, feature_name);
        mqtt_notify_value(saved_device_uuid, temp_feature_uuid, feature_name, temp);
      }

      float setpoint = get_setpoint();
      Serial.printf("read_temp_sensor_value - setpoint: %.2f °C\n", setpoint);
      float tolerance = get_tolerance();
      Serial.printf("read_temp_sensor_value - tolerance: %.2f °C\n", tolerance);

      //      ---+----------------+---------------+----
      //         ^                ^               ^
      //  setpoint-tolerance    setpoint      setpoint+tolerance

      if (temp > (setpoint + tolerance)) {
        Serial.printf("read_temp_sensor_value - it's too hot => %.2f > %.2f\n", temp, setpoint + tolerance);
        digitalWrite(HEAT, LOW);
        digitalWrite(COLD, HIGH);
        digitalWrite(PUMP, HIGH);
        digitalWrite(FAN, HIGH);
      } else if (temp < (setpoint - tolerance)) {
        Serial.printf("read_temp_sensor_value - it's too cold => %.2f < %.2f\n", temp, setpoint - tolerance);
        digitalWrite(HEAT, HIGH);
        digitalWrite(COLD, LOW);
        digitalWrite(PUMP, LOW);
        digitalWrite(FAN, LOW);
      } else {
        Serial.println("read_temp_sensor_value - temp in range");
        digitalWrite(HEAT, LOW);
        digitalWrite(COLD, LOW);
        digitalWrite(PUMP, LOW);
        digitalWrite(FAN, LOW);
      }
  }
}

void alarms_init() {
  alarm_temp = Alarm.timerRepeat(10, read_temp_sensor_value);
  Alarm.disable(alarm_temp);
}

void alarms_enable() {
  Alarm.enable(alarm_temp);
}

void alarms_disable() {
  Alarm.disable(alarm_temp);
}

void init_sensors() {
  temp_init_sensor();
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
  // set time to Saturday 00:00:00am Jan 1 2025
  setTime(0,0,0,1,1,25);

  // 1 init sensors
  Serial.println("setup - init sensors");
  init_sensors();

  // 2 init display
  Serial.println("setup - init display");
  init_display();

  // 3. instantiate alarms
  Serial.println("setup - init alarms (still disabled)...");
  alarms_init();

  // 4. starts alarms to read sensors values
  Serial.println("setup - enable alarms");
  alarms_enable();

  // main thermostat output (heat)
  pinMode(HEAT, OUTPUT);
  // main thermostat output (cold)
  pinMode(COLD, OUTPUT);
  // optional fan output
  pinMode(FAN, OUTPUT);
  // optional pump output
  pinMode(PUMP, OUTPUT);

  // 5. prepare wifi_client
  //    If SSL is enabled, add root ca to wifi_client to be used for secure connections
  //    This root ca will be used by all network components like mqtt and htpp
  # if SSL==true
    Serial.println("setup - running with SSL enabled");
    wifi_init_ca();
  # else 
    Serial.println("setup - running WITHOUT SSL");
  # endif

  // 6. init MQTT client
  //    init mqtt_client with wifi_client (previously configured)
  Serial.println("setup - init mqtt...");
  mqtt_init(wifi_client, mqtt_callback);

  // 7. connect to wifi
  Serial.println("setup - connect wifi...");
  wifi_connect(mac_address);

  // 8. register to the server
  Serial.println("setup - registering this device...");
  JsonDocument features = buildFeatures();
  int result = -999;
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

  // 9. read device UUID from preferences
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

  // 10. read features array from preferences
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
}

void loop() {
  // if 'saved_device_uuid' is not defined, it's an unregistered device
  if (saved_device_uuid == NULL || strlen(saved_device_uuid) == 0) {
    Serial.println("loop - saved_device_uuid NOT FOUND, cannot continue...");
    Alarm.delay(60000);
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

  mqtt_client.loop();

  Alarm.delay(1000);
}