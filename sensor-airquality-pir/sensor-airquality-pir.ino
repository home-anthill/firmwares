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

// alarms used to periodically read values from sensors
AlarmID_t alarm_airquality;
AlarmID_t alarm_pir;

String saved_uuid;

void read_airquality_sensor_value() {
  Serial.println("read_airquality_sensor_value - called");
  int pre_value = airquality_get_value();
  int new_value = airquality_get_prev_value();
  Serial.printf("read_airquality_sensor_value - new_value read = %d\n", new_value);
  // notifiy only if the value is changed to prevent useless notifications
  if (pre_value != new_value && new_value != -1) {
    Serial.println("read_airquality_sensor_value - notifying value...");
    mqtt_notify_value(saved_uuid, "airquality", new_value);
  }
}

void read_pir_sensor_value() {
  Serial.println("read_pir_sensor_value - called");
  int pre_value = pir_get_prev_value();
  int new_value = pir_get_value();
  Serial.printf("read_pir_sensor_value - new_value read = %d\n", new_value);
  // notifiy only if the value is changed to prevent useless notifications
  if (pre_value != new_value) {
    Serial.println("read_pir_sensor_value - notifying value...");
    mqtt_notify_value(saved_uuid, "motion", new_value);
  }
}

void alarms_init() {
  alarm_airquality = Alarm.timerRepeat(50, read_airquality_sensor_value);
  Alarm.disable(alarm_airquality);
  alarm_pir = Alarm.timerRepeat(30, read_pir_sensor_value);
  Alarm.disable(alarm_pir);
}

void alarms_enable() {
  Alarm.enable(alarm_airquality);
  Alarm.enable(alarm_pir);
}

void alarms_disable() {
  Alarm.disable(alarm_airquality);
  Alarm.disable(alarm_pir);
}

void init_sensors() {
  airquality_init_sensor();
  pir_init_sensor();
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
  setTime(0,0,0,1,1,21);

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
  mqtt_init(wifi_client);

  // 3. connect to wifi
  Serial.println("setup - connect wifi...");
  wifi_connect();

  // 4. register to the server
  Serial.println("setup - registering this device...");
  String features = "[";
  features += "{\"type\": \"sensor\",\"name\": \"airquality\",\"enable\": true,\"order\": 1,\"unit\": \"-\"},";
  features += "{\"type\": \"sensor\",\"name\": \"motion\",\"enable\": true,\"order\": 2,\"unit\": \"-\"}";
  features += "]";
  int result = -999;
  # if SSL==true
    result = register_secure_server(wifi_client, mac_address, features);
  # else 
    result = register_insecure_server(wifi_client, mac_address, features);
  # endif
  Serial.print("setup - register returned result=");
  Serial.println(result);
  if (result != 0) {
    Serial.printf("setup - register error, returned result = %d\n", result);
    return;
  }

  // 5. instantiate alarms, but disable them
  Serial.println("setup - init alarms (still disabled)...");
  alarms_init();

  // 6. read UUID from preferences
  //    if it's the first boot, it will be the same already stored in global variable 'saved_uuid',
  //    because already filled during the registration process.
  //    otherwise, it reads the existing UUID form preferences,
  //    because registration won't return the UUID again.
  Serial.println("setup - getting saved UUID from preferences...");
  saved_uuid = storage_get_uuid();
  if (saved_uuid.equals("")) {
    Serial.println("************* ERROR **************");
    Serial.println("setup - Cannot read saved UUID from Preferences");
    Serial.println("**********************************");
    return;
  }

  // 7. init sensors
  init_sensors();

  delay(1000);
}

void loop() {
  // if 'saved_uuid' is not defined, it's an unregistered device
  if (saved_uuid == NULL || saved_uuid.length() == 0) {
    Serial.println("loop - saved_uuid NOT FOUND, cannot continue...");
    delay(60000);
    return;
  }

  // if not connected to the wifi, try to reconnect
  if (wifi_get_status() != WL_CONNECTED) {
    alarms_disable();
    Serial.println("loop - WiFi connection lost!");
    wifi_reconnect();
  }

  // if not connected to mqtt server, try to reconnect
  if (!mqtt_client.connected()) {
    Serial.println("loop - mqtt connecting...");
    mqtt_connect(saved_uuid);
    // starts alarms to read sensors values
    alarms_enable();
  }

  mqtt_client.loop();

  Alarm.delay(1000);
}