// include wifi library
#include <WiFi.h>
#include <WiFiClientSecure.h>
// include http library (also required to use 'WiFiClientSecure')
#include <HTTPClient.h>
// include json library (https://github.com/bblanchon/ArduinoJson)
#include <ArduinoJson.h>
// include MQTT library (https://pubsubclient.knolleary.net/api)
#include <PubSubClient.h>
// include the TimeAlarms library
// (https://www.arduino.cc/reference/en/libraries/timealarms/)
#include <TimeAlarms.h>

// must be the first, before any internal include
#include "secrets.h"

// include all local files
#include "controller.h"
#include "mqtt_handler.h"
#include "registration.h"
#include "storage.h"
#include "temp_sensor.h"
#include "wifi_handler.h"
#include "display.h"
#include "feature_values.h"

// build-in RGB LED
#define BOARD_RGB_LED_PIN 38

char mac_address[18];

// private functions
void mqtt_callback(char *topic, uint8_t *payload, unsigned int length);
bool get_feature_uuid_by_name(char *featureUuid, size_t max_len, const char *name);
void feature_values_load_saved();
void feature_values_init_saved_features();
void record_command_values(uint8_t* payload, unsigned int length);
void display_show_feature(const char* name);
void command_display_start();
void command_display_next();
void read_temp_sensor_value();
void send_online_status();
void alarms_init();
void alarm_temperature_enable();
void alarm_online_enable();
void alarm_online_disable();
void alarm_command_display_enable();
void alarm_command_display_disable();
void alarms_disable();
void init_sensors();
JsonDocument buildFeatures();

// alarms used to periodically read values from sensors
AlarmID_t alarm_temp;
AlarmID_t alarm_online;
#if OLED_DISPLAY == true
AlarmID_t alarm_command_display;
extern size_t display_feature_index;
#endif

const size_t COMMAND_DISPLAY_QUEUE_MAX = 8;
char command_display_queue[COMMAND_DISPLAY_QUEUE_MAX][FEATURE_VALUE_NAME_MAX_LEN];
size_t command_display_len = 0;
size_t command_display_index = 0;
bool command_display_active = false;

// device_uuid global variable
char saved_device_uuid[37];
// features array global variable
JsonDocument doc_features;
JsonArray saved_features = doc_features.to<JsonArray>();

// outputs
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define HEAT 47
  #define COLD 48
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  #define HEAT 33
  #define COLD 34
#elif defined(CONFIG_IDF_TARGET_ESP32)
  // TODO: test this on a real older ESP32 model
  #define HEAT 2
  #define COLD 15
#endif
#define FAN 35
#define PUMP 36

// ---------------------------------------------------------------------------
// Connectivity state machine — runs one step per loop() iteration so that
// Alarm.delay() is always reached and the thermostat control loop never stalls.
//
// Phases:
//   CONN_WIFI_WAITING  — WiFi.begin() was called; polling status each tick
//   CONN_REGISTERING   — WiFi up; single HTTP registration attempt per tick
//   CONN_MQTT_TRYING   — registered; single MQTT attempt every 5 s
//   CONN_ONLINE        — fully connected; health-check only
//   CONN_COOLDOWN      — too many failures; sleep ~12 h then ESP.restart()
// ---------------------------------------------------------------------------
enum ConnState {
  CONN_WIFI_WAITING,
  CONN_REGISTERING,
  CONN_MQTT_TRYING,
  CONN_ONLINE,
  CONN_COOLDOWN
};

ConnState conn_state = CONN_WIFI_WAITING;

// Number of consecutive failed attempts in the current burst
int conn_attempts = 0;

// millis() timestamp: earliest time the next attempt is allowed
unsigned long conn_next_attempt_ms = 0;

// millis() timestamp: when the cooldown period ends (→ ESP.restart())
unsigned long conn_cooldown_until_ms = 0;

// Give WiFi extra time on boot: poll every 2 s for up to ~90 s before the
// device enters the long offline cooldown.
const int CONN_MAX_WIFI_POLLS = 45;
const unsigned long CONN_WIFI_POLL_INTERVAL_MS = 2000;
// Give up on first-boot registration after this many attempts
const int CONN_MAX_REG_ATTEMPTS = 3;
// Give up on MQTT after this many attempts
const int CONN_MAX_MQTT_ATTEMPTS = 10;
// Minimum gap between MQTT attempts
const unsigned long CONN_MQTT_RETRY_MS = 5000;
// Minimum gap between registration attempts
const unsigned long CONN_REG_RETRY_MS = 10000;
// Offline cooldown: ~12 hours; device reboots afterwards for a fresh network
// stack
const unsigned long CONN_COOLDOWN_MS = 43200000UL;

bool get_feature_uuid_by_name(char *featureUuid, size_t max_len, const char *name) {
  for (int i = 0; i < saved_features.size(); i++) {
    JsonObject feature = saved_features[i];
    const char *uuidval = feature["uuid"];
    const char *nameval = feature["name"];
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

void mqtt_callback(char *topic, uint8_t *payload, unsigned int length) {
  Serial.println("mqtt_callback - called");
  set_configuration(saved_device_uuid, mac_address, saved_features, payload,
                    length);
  record_command_values(payload, length);
}

void feature_values_load_saved() {
  JsonDocument doc;
  JsonArray featureValues = doc.to<JsonArray>();
  storage_get_feature_values(featureValues);

  for (JsonObject featureValue : featureValues) {
    const char* feature_name = featureValue["featureName"];
    if (feature_name == nullptr) {
      feature_name = featureValue["name"];
    }
    JsonVariant value = featureValue["payload"]["value"];
    if (value.isNull()) {
      value = featureValue["value"];
    }
    if (feature_name == nullptr || value.isNull() ||
        !(value.is<bool>() || value.is<int>() || value.is<float>())) {
      continue;
    }
    feature_values_set(feature_name, value.as<float>());
  }
}

void feature_values_init_saved_features() {
  if (saved_features.size() > 0) {
    feature_values_init(saved_features);
  } else {
    JsonDocument default_features = buildFeatures();
    feature_values_init(default_features.as<JsonArray>());
  }

  feature_values_load_saved();
}

void display_show_feature(const char* name) {
#if OLED_DISPLAY == true
  if (name == nullptr) {
    return;
  }

  FeatureValue value = {};
  for (size_t i = 0; i < feature_values_count(); i++) {
    if (!feature_values_get(i, &value)) {
      continue;
    }
    if (strcmp(value.name, name) == 0 && value.has_value) {
      display_feature_index = i;
      update_display();
      return;
    }
  }
#else
  (void)name;
#endif
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

  command_display_len = 0;
  command_display_index = 0;
  command_display_active = false;
  alarm_command_display_disable();

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

    if (feature_values_set(feature_name, value.as<float>())) {
      if (command_display_len < COMMAND_DISPLAY_QUEUE_MAX) {
        strncpy(command_display_queue[command_display_len], feature_name,
                FEATURE_VALUE_NAME_MAX_LEN - 1);
        command_display_queue[command_display_len][FEATURE_VALUE_NAME_MAX_LEN - 1] = '\0';
        command_display_len++;
      }
    }
  }

  command_display_start();
}

void command_display_start() {
#if OLED_DISPLAY == true
  if (command_display_len == 0) {
    return;
  }

  command_display_active = true;
  command_display_index = 0;
  display_show_feature(command_display_queue[command_display_index]);
  command_display_index++;
  alarm_command_display_enable();
#endif
}

void command_display_next() {
#if OLED_DISPLAY == true
  if (!command_display_active) {
    alarm_command_display_disable();
    return;
  }

  if (command_display_index < command_display_len) {
    display_show_feature(command_display_queue[command_display_index]);
    command_display_index++;
    return;
  }

  alarm_command_display_disable();
  command_display_active = false;
  display_show_feature("temperature");
#endif
}

JsonDocument buildFeatures() {
  JsonDocument root;
  JsonArray array = root.to<JsonArray>();

  JsonObject setpoint = array.add<JsonObject>();
  setpoint["type"] = "controller";
  setpoint["name"] = "setpoint";
  setpoint["enable"] = true;
  setpoint["order"] = 1;
  setpoint["unit"] = "°C";
  JsonObject setpointSpec = setpoint["spec"].to<JsonObject>();
  setpointSpec["format"] = "float";
  setpointSpec["min"] = 10;
  setpointSpec["max"] = 30;
  setpointSpec["step"] = 0.5;

  JsonObject tolerance = array.add<JsonObject>();
  tolerance["type"] = "controller";
  tolerance["name"] = "tolerance";
  tolerance["enable"] = true;
  tolerance["order"] = 2;
  tolerance["unit"] = "°C";
  JsonObject toleranceSpec = tolerance["spec"].to<JsonObject>();
  toleranceSpec["format"] = "float";
  toleranceSpec["min"] = 0;
  toleranceSpec["max"] = 10;
  toleranceSpec["step"] = 0.5;

  JsonObject temperature = array.add<JsonObject>();
  temperature["type"] = "sensor";
  temperature["name"] = "temperature";
  temperature["enable"] = true;
  temperature["order"] = 3;
  temperature["unit"] = "°C";
  JsonObject temperatureSpec = temperature["spec"].to<JsonObject>();
  temperatureSpec["format"] = "float";
  temperatureSpec["min"] = -40; // TODO is this right???
  temperatureSpec["max"] = 200; // TODO is this right???
  temperatureSpec["step"] = 0.01; // TODO is this right???

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

void read_temp_sensor_value() {
  Serial.println("read_temp_sensor_value - called");
  float temp = temp_get_temperature();
  if (isnan(temp)) {
    Serial.println("read_temp_sensor_value - error reading temperature!");
  } else {
    Serial.printf("read_temp_sensor_value - temperature: %.2f °C\n", temp);
    if (feature_values_set("temperature", temp) && !command_display_active) {
      display_show_feature("temperature");
    }

    if (mqtt_client.connected()) {
      const char *feature_name = "temperature";
      char temp_feature_uuid[37];
      if (get_feature_uuid_by_name(temp_feature_uuid, sizeof(temp_feature_uuid),
                                   feature_name)) {
        mqtt_notify_value(saved_device_uuid, temp_feature_uuid, feature_name,
                          temp);
      } else {
        Serial.println(
            "read_temp_sensor_value - feature uuid not found for temperature");
      }
    }

    float setpoint = get_setpoint();
    Serial.printf("read_temp_sensor_value - setpoint: %.2f °C\n", setpoint);
    float tolerance = get_tolerance();
    Serial.printf("read_temp_sensor_value - tolerance: %.2f °C\n", tolerance);

    //      ---+----------------+---------------+----
    //         ^                ^               ^
    //  setpoint-tolerance    setpoint      setpoint+tolerance

    if (temp > (setpoint + tolerance)) {
      Serial.printf("read_temp_sensor_value - it's too hot => %.2f > %.2f\n",
                    temp, setpoint + tolerance);
      digitalWrite(HEAT, LOW);
      digitalWrite(COLD, HIGH);
      digitalWrite(PUMP, HIGH);
      digitalWrite(FAN, HIGH);
    } else if (temp < (setpoint - tolerance)) {
      Serial.printf("read_temp_sensor_value - it's too cold => %.2f < %.2f\n",
                    temp, setpoint - tolerance);
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

void send_online_status() {
  Serial.println("send_online_status - called");
  if (!mqtt_client.connected()) {
    Serial.println("send_online_status - MQTT not connected, skipping");
    return;
  }

  const char* feature_name = "online";
  char feature_uuid[37];
  if (get_feature_uuid_by_name(feature_uuid, sizeof(feature_uuid), feature_name)) {
    mqtt_notify_value(saved_device_uuid, feature_uuid, feature_name, 1);
  } else {
    Serial.println("send_online_status - feature uuid not found for online");
  }
}

void publish_initial_values() {
  Serial.println("publish_initial_values - called");
  read_temp_sensor_value();
  send_online_status();
}

void alarms_init() {
  alarm_temp = Alarm.timerRepeat(10, read_temp_sensor_value);
  Alarm.disable(alarm_temp);
  alarm_online = Alarm.timerRepeat(60, send_online_status);
  Alarm.disable(alarm_online);
#if OLED_DISPLAY == true
  alarm_command_display = Alarm.timerRepeat(1, command_display_next);
  Alarm.disable(alarm_command_display);
#endif
}

void alarm_temperature_enable() {
  Alarm.enable(alarm_temp);
}

void alarm_online_enable() {
  Alarm.enable(alarm_online);
}

void alarm_online_disable() {
  Alarm.disable(alarm_online);
}

void alarm_command_display_enable() {
#if OLED_DISPLAY == true
  Alarm.enable(alarm_command_display);
#endif
}

void alarm_command_display_disable() {
#if OLED_DISPLAY == true
  Alarm.disable(alarm_command_display);
#endif
}

void alarms_disable() {
  Alarm.disable(alarm_temp);
  Alarm.disable(alarm_online);
#if OLED_DISPLAY == true
  Alarm.disable(alarm_command_display);
#endif
}

void init_sensors() {
  temp_init_sensor();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("setup - starting...");

  // 0. configure hardware
  rgbLedWrite(BOARD_RGB_LED_PIN, 0, 0, 0); 
  // set time to Saturday 00:00:00am Jan 1 2025
  setTime(0, 0, 0, 1, 1, 25);

  // 1. init sensors — thermostat is not yet controlling, but sensor is ready
  Serial.println("setup - init sensors");
  init_sensors();

  // 2. init display
  Serial.println("setup - init display");
  init_display();

  // Load any persisted feature definitions so offline temperature reads can
  // still use the same display path as received commands.
  Serial.println("setup - loading saved features for display");
  storage_get_features(saved_features);
  feature_values_init_saved_features();

  // 3. instantiate alarms (disabled)
  Serial.println("setup - init alarms (still disabled)...");
  alarms_init();

  // 4. configure output GPIOs
  pinMode(HEAT, OUTPUT);
  pinMode(COLD, OUTPUT);
  pinMode(FAN, OUTPUT);
  pinMode(PUMP, OUTPUT);

  // 5. enable alarms — thermostat is OPERATIONAL offline
  //    Temperature reading + hysteretic control will fire every 10s via
  //    Alarm.delay() in loop(), regardless of WiFi/MQTT state.
  Serial.println("setup - enable temperature alarm");
  alarm_temperature_enable();

// 6. prepare wifi_client (CA cert for TLS)
#if SSL == true
  Serial.println("setup - running with SSL enabled");
  wifi_init_ca();
#else
  Serial.println("setup - running WITHOUT SSL");
#endif

  // 7. init MQTT client
  Serial.println("setup - init mqtt...");
  mqtt_init(wifi_client, mqtt_callback);

  // 8. kick off WiFi connection — non-blocking; loop() drives the state machine
  Serial.println("setup - starting WiFi (non-blocking, state machine continues "
                 "in loop)...");
  wifi_start_connect();
  // conn_state is already CONN_WIFI_WAITING (default)
}

// ---------------------------------------------------------------------------
// handle_connectivity — called once per loop() iteration.
// Advances the connectivity state machine by exactly one step; never blocks.
// ---------------------------------------------------------------------------
void handle_connectivity() {
  unsigned long now = millis();

  switch (conn_state) {

  case CONN_WIFI_WAITING:
    if (wifi_get_status() == WL_CONNECTED) {
      wifi_populate_mac(mac_address);
      wifi_sync_time();
      conn_attempts = 0;
      conn_next_attempt_ms = 0;

      // Load persisted UUID + features; if absent this is a first boot
      size_t uuid_len = storage_get_uuid(saved_device_uuid);
      if (uuid_len == 37 && strlen(saved_device_uuid) > 0) {
        storage_get_features(saved_features);
        feature_values_init_saved_features();
      }

      if (strlen(saved_device_uuid) == 0 || saved_features.size() == 0) {
        Serial.println(
            "handle_connectivity - no UUID in preferences, need to register");
        conn_state = CONN_REGISTERING;
      } else {
        Serial.println(
            "handle_connectivity - UUID found, skipping registration");
        conn_state = CONN_MQTT_TRYING;
      }
    } else {
      if (now < conn_next_attempt_ms)
        break;

      conn_attempts++;
      conn_next_attempt_ms = now + CONN_WIFI_POLL_INTERVAL_MS;
      if (conn_attempts > CONN_MAX_WIFI_POLLS) {
        Serial.printf("handle_connectivity - WiFi: max polls (%d) reached, "
                      "entering cooldown\n",
                      CONN_MAX_WIFI_POLLS);
        conn_cooldown_until_ms = now + CONN_COOLDOWN_MS;
        conn_state = CONN_COOLDOWN;
      }
    }
    break;

  case CONN_REGISTERING:
    if (wifi_get_status() != WL_CONNECTED) {
      Serial.println("handle_connectivity - WiFi lost during registration, "
                     "restarting WiFi");
      conn_attempts = 0;
      wifi_start_connect();
      conn_state = CONN_WIFI_WAITING;
      break;
    }
    if (now < conn_next_attempt_ms)
      break;

    {
      Serial.printf("handle_connectivity - registration attempt %d/%d\n",
                    conn_attempts + 1, CONN_MAX_REG_ATTEMPTS);
      JsonDocument features = buildFeatures();
      int result = -1;
#if SSL == true
      result = register_secure_server_once(wifi_client, mac_address, features);
#else
      result =
          register_insecure_server_once(wifi_client, mac_address, features);
#endif
      Serial.printf("handle_connectivity - registration result = %d\n", result);

      if (result == 0) {
        size_t uuid_len = storage_get_uuid(saved_device_uuid);
        storage_get_features(saved_features);
        if (uuid_len == 37 && strlen(saved_device_uuid) > 0 &&
            saved_features.size() > 0) {
          Serial.println("handle_connectivity - registered successfully, "
                         "connecting to MQTT");
          conn_attempts = 0;
          conn_next_attempt_ms = 0;
          feature_values_init_saved_features();
          conn_state = CONN_MQTT_TRYING;
        } else {
          Serial.println("handle_connectivity - registration OK but failed to "
                         "read preferences");
          conn_attempts++;
          conn_next_attempt_ms = now + CONN_REG_RETRY_MS;
          if (conn_attempts >= CONN_MAX_REG_ATTEMPTS) {
            Serial.println("handle_connectivity - registration max attempts "
                           "reached, entering cooldown");
            conn_cooldown_until_ms = now + CONN_COOLDOWN_MS;
            conn_state = CONN_COOLDOWN;
          }
        }
      } else {
        conn_attempts++;
        conn_next_attempt_ms = now + CONN_REG_RETRY_MS;
        Serial.printf("handle_connectivity - registration failed (code=%d), "
                      "attempt %d/%d\n",
                      result, conn_attempts, CONN_MAX_REG_ATTEMPTS);
        if (conn_attempts >= CONN_MAX_REG_ATTEMPTS) {
          Serial.println("handle_connectivity - registration max attempts "
                         "reached, entering cooldown");
          conn_cooldown_until_ms = now + CONN_COOLDOWN_MS;
          conn_state = CONN_COOLDOWN;
        }
      }
    }
    break;

  case CONN_MQTT_TRYING:
    if (wifi_get_status() != WL_CONNECTED) {
      Serial.println("handle_connectivity - WiFi lost during MQTT connect, "
                     "restarting WiFi");
      conn_attempts = 0;
      wifi_start_connect();
      conn_state = CONN_WIFI_WAITING;
      break;
    }
    if (now < conn_next_attempt_ms)
      break;

    Serial.printf("handle_connectivity - MQTT attempt %d/%d\n",
                  conn_attempts + 1, CONN_MAX_MQTT_ATTEMPTS);
    if (mqtt_try_connect_once(saved_device_uuid)) {
      Serial.println("handle_connectivity - MQTT connected, going online");
      conn_attempts = 0;
      conn_next_attempt_ms = 0;
      alarm_online_enable();
      publish_initial_values();
      conn_state = CONN_ONLINE;
    } else {
      conn_attempts++;
      conn_next_attempt_ms = now + CONN_MQTT_RETRY_MS;
      Serial.printf("handle_connectivity - MQTT failed, attempt %d/%d\n",
                    conn_attempts, CONN_MAX_MQTT_ATTEMPTS);
      if (conn_attempts >= CONN_MAX_MQTT_ATTEMPTS) {
        Serial.println("handle_connectivity - MQTT max attempts reached, "
                       "entering cooldown");
        alarm_online_disable();
        conn_cooldown_until_ms = now + CONN_COOLDOWN_MS;
        conn_state = CONN_COOLDOWN;
      }
    }
    break;

  case CONN_ONLINE:
    if (wifi_get_status() != WL_CONNECTED) {
      Serial.println("handle_connectivity - WiFi dropped, reconnecting...");
      alarm_online_disable();
      conn_attempts = 0;
      wifi_start_connect();
      conn_state = CONN_WIFI_WAITING;
    } else if (!mqtt_client.connected()) {
      Serial.println("handle_connectivity - MQTT dropped, reconnecting...");
      alarm_online_disable();
      conn_attempts = 0;
      conn_next_attempt_ms = 0;
      conn_state = CONN_MQTT_TRYING;
    }
    break;

  case CONN_COOLDOWN:
    // Thermostat keeps controlling temperature during the entire cooldown.
    // After the cooldown expires, reboot once to get a fresh network stack;
    // this limits reconnect attempts to roughly 2x per day.
    if (now >= conn_cooldown_until_ms) {
      Serial.println("handle_connectivity - cooldown ended, rebooting to "
                     "restore network state");
      ESP.restart();
    }
    break;
  }
}

void loop() {
  handle_connectivity();

  if (mqtt_client.connected()) {
    // Defense in depth: mqtt_client.loop() returns false if the connection is
    // broken (e.g. broker restarted). Catching it here triggers disconnect()
    // immediately; handle_connectivity() will detect !connected() on the next
    // iteration and transition to CONN_MQTT_TRYING.
    if (!mqtt_client.loop()) {
      Serial.println("loop - mqtt_client.loop() returned false, forcing disconnect to trigger reconnect");
      mqtt_client.disconnect();
      alarm_online_disable();
    }
  }

  Alarm.delay(100);
}
