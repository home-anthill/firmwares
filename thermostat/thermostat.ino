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

// use GPIO ESP32 library for advanced GPIO control
#include <driver/gpio.h>

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
#define DISPLAY_BUTTON_PIN 42

char mac_address[18];

// private functions
void mqtt_callback(char *topic, uint8_t *payload, unsigned int length);
bool get_feature_uuid_by_name(char *featureUuid, size_t max_len, const char *name);
void feature_values_load_saved();
void feature_values_init_saved_features();
void record_command_values(uint8_t* payload, unsigned int length);
void read_temp_sensor_value();
void send_online_status();
void publish_sensor_value(const char* feature_name, float value);
void alarms_init();
void alarm_temperature_enable();
void alarm_online_enable();
void alarm_online_disable();
void alarms_disable();
void configure_output_off(uint8_t pin, bool active_low);
void outputs_init();
void init_sensors();
JsonDocument buildFeatures();
uint8_t output_level(bool active_low, bool active);
void write_heat_output(bool active);
void write_cold_output(bool active);
void write_fan_output(bool active);
void write_pump_output(bool active);
#if OPERATING_MODE == 0
void cooling_safety_start(float temp);
void cooling_safety_reset();
bool cooling_safety_temperature_not_rising(float temp);
void cooling_fault_stop();
#endif

// alarms used to periodically read values from sensors
AlarmID_t alarm_temp;
AlarmID_t alarm_online;

// device_uuid global variable
char saved_device_uuid[37];
// features array global variable
JsonDocument doc_features;
JsonArray saved_features = doc_features.to<JsonArray>();

// Thermostat state values are kept separate from OPERATING_MODE, which only
// selects whether this firmware build controls COLD or HEAT.
constexpr int THERMOSTAT_MODE_COOLING_FAULT = -1;
constexpr int THERMOSTAT_MODE_SLEEP = 0;
constexpr int THERMOSTAT_MODE_COLD = 1;
constexpr int THERMOSTAT_MODE_HEAT = 2;

int thermostat_mode = THERMOSTAT_MODE_SLEEP;
int prev_thermostat_mode = THERMOSTAT_MODE_SLEEP;

#if OPERATING_MODE == 0
// A COLD output may drive a conventional cooler or a Peltier module. A Peltier
// with reversed polarity, a damaged thermal path, or inadequate hot-side heat
// rejection can warm the controlled fluid while cooling is requested. These
// two baselines detect that unsafe rise without requiring a minimum cooling
// rate: a flat or decreasing temperature remains valid.
bool cooling_safety_monitoring = false;
bool cooling_short_rise_check_done = false;
unsigned long cooling_started_at_ms = 0;
unsigned long cooling_wide_window_started_at_ms = 0;
float cooling_start_temp = NAN;
float cooling_wide_window_start_temp = NAN;
#endif

// outputs
#if defined(HOME_ANTHILL_TEST_OLED_DISPLAY)
  #define HEAT 4
  #define COLD 5
  #define FAN 6
  #define PUMP 7
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  #define HEAT 5
  #define COLD 6
  #define FAN 7
  #define PUMP 15
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  #define HEAT 5
  #define COLD 6
  #define FAN 7
  #define PUMP 15
#elif defined(CONFIG_IDF_TARGET_ESP32)
  // TODO: test this on a real older ESP32 model
  #define HEAT 25
  #define COLD 26
  #define FAN 27
  #define PUMP 32
#endif

// HARDWARE SAFETY REQUIREMENT: each relay/driver input must be externally held
// at its inactive level while the ESP32 GPIO is high-impedance during reset,
// bootloader execution, USB flashing, and startup. Use a pull-up resistor (10 KOhm) for every
// active-low output and a pull-down (10 KOhm) for every active-high output.

#if OPERATING_MODE != 0 && OPERATING_MODE != 1
#error "OPERATING_MODE must be defined and 0 (cooling) or 1 (heating)"
#endif
#if FAN_TURN_OFF_DELAY_SECONDS < 0
#error "FAN_TURN_OFF_DELAY_SECONDS must be greater or equals than 0"
#endif
#if COOLING_SHORT_RISE_CHECK_SECONDS <= 0
#error "COOLING_SHORT_RISE_CHECK_SECONDS must be greater than 0"
#endif
#if COOLING_WIDE_RISE_CHECK_SECONDS <= 0
#error "COOLING_WIDE_RISE_CHECK_SECONDS must be greater than 0"
#endif

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

// millis() timestamp: when the cooldown period ends (ESP.restart())
unsigned long conn_cooldown_until_ms = 0;

bool fan_requested_active = false;
unsigned long fan_turn_off_at_ms = 0;

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
// Offline cooldown: ~12 hours; device reboots afterwards for a fresh network tack
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
  set_configuration(saved_device_uuid, mac_address, saved_features, payload, length);
  record_command_values(payload, length);
  display_show_message("Command", "Received");
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
  setpointSpec["max"] = 35;
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

  JsonObject mode = array.add<JsonObject>();
  mode["type"] = "sensor";
  mode["name"] = "mode";
  mode["enable"] = true;
  mode["order"] = 4;
  mode["unit"] = "-";
  JsonObject modeSpec = mode["spec"].to<JsonObject>();
  modeSpec["format"] = "int";
  modeSpec["min"] = -1; // fault
  modeSpec["max"] = 2;  // heating
  modeSpec["step"] = 1;

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

void publish_sensor_value(const char* feature_name, float value) {
  if (!mqtt_client.connected()) {
    return;
  }
  char feature_uuid[37];
  if (get_feature_uuid_by_name(feature_uuid, sizeof(feature_uuid), feature_name)) {
    mqtt_notify_value(saved_device_uuid, feature_uuid, feature_name, value);
  } else {
    Serial.printf("publish_sensor_value - feature uuid not found for %s\n", feature_name);
  }
}

void read_temp_sensor_value() {
  Serial.println("read_temp_sensor_value - called");
  float temp = temp_get_temperature();
  if (isnan(temp)) {
    Serial.println("read_temp_sensor_value - error reading temperature!");
  } else {
    const int mode_before_update = thermostat_mode;

    feature_values_set("temperature", temp);
    publish_sensor_value("temperature", temp);

    float setpoint = get_setpoint();
    float tolerance = get_tolerance();

    Serial.printf("read_temp_sensor_value - temperature: %.2f °C\n", temp);
    Serial.printf("read_temp_sensor_value - setpoint: %.2f °C\n", setpoint);
    Serial.printf("read_temp_sensor_value - tolerance: %.2f °C\n", tolerance);
    Serial.printf("read_temp_sensor_value - thermostat_mode: %d\n", thermostat_mode);

    // The configured operating mode bootstraps at the setpoint. After its first
    // cycle, it restarts only at the corresponding tolerance boundary.
#if OPERATING_MODE == 1
    // HEAT OPERATING MODE
    const bool first_start = prev_thermostat_mode != THERMOSTAT_MODE_HEAT && temp < setpoint;
    const bool hysteresis_restart =
        prev_thermostat_mode == THERMOSTAT_MODE_HEAT && temp < setpoint &&
        temp <= (setpoint - tolerance);

    if (thermostat_mode == THERMOSTAT_MODE_HEAT && temp >= setpoint) {
      Serial.printf("read_temp_sensor_value - HEATING reached setpoint - %.2f >= %.2f\n",
                    temp, setpoint);
      thermostat_mode = THERMOSTAT_MODE_SLEEP;
    } else if (thermostat_mode == THERMOSTAT_MODE_SLEEP &&
               (first_start || hysteresis_restart)) {
      Serial.printf("read_temp_sensor_value - HEATING required - %.2f < %.2f\n",
                    temp, setpoint);
      thermostat_mode = THERMOSTAT_MODE_HEAT;
      prev_thermostat_mode = THERMOSTAT_MODE_HEAT;
    } else if (thermostat_mode == THERMOSTAT_MODE_SLEEP) {
      Serial.println("read_temp_sensor_value - HEATING sleep");
    }

    write_heat_output(thermostat_mode == THERMOSTAT_MODE_HEAT);
#else
    // COOL OPERATING MODE
    if (thermostat_mode == THERMOSTAT_MODE_COOLING_FAULT) {
      // The fault is latched until reboot. Continue requesting safe output
      // levels on every sample so the FAN cooldown can also reach its deadline.
      write_cold_output(false);
      write_pump_output(false);
      write_fan_output(false);
    } else {
      const bool cooling_was_active = thermostat_mode == THERMOSTAT_MODE_COLD;
      const bool first_start = prev_thermostat_mode != THERMOSTAT_MODE_COLD && temp > setpoint;
      const bool hysteresis_restart =
          prev_thermostat_mode == THERMOSTAT_MODE_COLD && temp > setpoint &&
          temp >= (setpoint + tolerance);

      if (cooling_was_active && temp <= setpoint) {
        Serial.printf("read_temp_sensor_value - COOLING reached setpoint - %.2f <= %.2f\n", temp, setpoint);
        thermostat_mode = THERMOSTAT_MODE_SLEEP;
        cooling_safety_reset();
      } else if (thermostat_mode == THERMOSTAT_MODE_SLEEP && (first_start || hysteresis_restart)) {
        Serial.printf("read_temp_sensor_value - COOLING required - %.2f > %.2f\n", temp, setpoint);
        thermostat_mode = THERMOSTAT_MODE_COLD;
        prev_thermostat_mode = THERMOSTAT_MODE_COLD;
        cooling_safety_start(temp);
      } else if (thermostat_mode == THERMOSTAT_MODE_SLEEP) {
        Serial.println("read_temp_sensor_value - COOLING sleep");
      }

      if (thermostat_mode == THERMOSTAT_MODE_COLD && !cooling_safety_temperature_not_rising(temp)) {
        cooling_fault_stop();
      } else {
        const bool cooling_active = thermostat_mode == THERMOSTAT_MODE_COLD;
        write_cold_output(cooling_active);
        write_pump_output(cooling_active);
        write_fan_output(cooling_active);
      }
    }
#endif

    if (thermostat_mode != mode_before_update) {
      feature_values_set("mode", thermostat_mode);
      publish_sensor_value("mode", static_cast<float>(thermostat_mode));
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
  publish_sensor_value(feature_name, 1.0f);
}

void publish_initial_values() {
  Serial.println("publish_initial_values - called");
  read_temp_sensor_value();
  send_online_status();
}

void alarms_init() {
  alarm_temp = Alarm.timerRepeat(5, read_temp_sensor_value);
  Alarm.disable(alarm_temp);
  alarm_online = Alarm.timerRepeat(60, send_online_status);
  Alarm.disable(alarm_online);
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

void alarms_disable() {
  Alarm.disable(alarm_temp);
  Alarm.disable(alarm_online);
}

uint8_t output_level(bool active_low, bool active) {
  return active ? (active_low ? LOW : HIGH) : (active_low ? HIGH : LOW);
}

#if OPERATING_MODE == 0
void cooling_safety_start(float temp) {
  const unsigned long now = millis();
  Serial.printf("cooling_safety_start - now = %lu, temp = %.2f\n", now, temp);

  cooling_safety_monitoring = true;
  cooling_short_rise_check_done = false;
  cooling_started_at_ms = now;
  cooling_wide_window_started_at_ms = now;
  cooling_start_temp = temp;
  cooling_wide_window_start_temp = temp;
}

void cooling_safety_reset() {
  Serial.println("cooling_safety_reset");
  cooling_safety_monitoring = false;
  cooling_short_rise_check_done = false;
  cooling_started_at_ms = 0;
  cooling_wide_window_started_at_ms = 0;
  cooling_start_temp = NAN;
  cooling_wide_window_start_temp = NAN;
}

/**
 * Check whether the temperature has avoided rising during active cooling.
 *
 * At the configured short and recurring wide deadlines, this compares the
 * latest valid sensor temperature with the applicable cooling baseline. A
 * successful wide check advances that window's time and temperature baseline.
 * If monitoring is inactive or no check is due, the sample passes unchanged.
 *
 * @param temp Latest valid temperature sample, in degrees Celsius.
 * @return true if no due check detected a temperature rise; false if the
 *         temperature rose above a due check's baseline, signaling the caller
 *         to latch the cooling fault and stop the cooling outputs.
 */
bool cooling_safety_temperature_not_rising(float temp) {
  Serial.printf("cooling_safety_temperature_not_rising - temp = %.2f\n", temp);
  if (!cooling_safety_monitoring) {
    return true;
  }

  const unsigned long now = millis();
  const unsigned long short_check_ms =
      static_cast<unsigned long>(COOLING_SHORT_RISE_CHECK_SECONDS) * 1000UL;
  const unsigned long wide_check_ms =
      static_cast<unsigned long>(COOLING_WIDE_RISE_CHECK_SECONDS) * 1000UL;

  // The first check catches an incorrectly wired or failing Peltier that heats
  // the fluid soon after COLD is enabled. Equal temperature is safe here: this
  // check guards against heating and does not validate cooling performance.
  if (!cooling_short_rise_check_done && now - cooling_started_at_ms >= short_check_ms) {
    cooling_short_rise_check_done = true;
    if (temp > cooling_start_temp) {
      Serial.printf("cooling_safety - temperature rose during short check: %.2f > %.2f\n", temp, cooling_start_temp);
      return false;
    }
  }

  // The recurring wide window catches a later Peltier or thermal-path failure.
  // A successful window becomes the baseline for the next wide window.
  if (now - cooling_wide_window_started_at_ms >= wide_check_ms) {
    if (temp > cooling_wide_window_start_temp) {
      Serial.printf("cooling_safety - temperature rose during wide check: %.2f > %.2f\n", temp, cooling_wide_window_start_temp);
      return false;
    }

    cooling_wide_window_started_at_ms = now;
    cooling_wide_window_start_temp = temp;
  }

  return true;
}

void cooling_fault_stop() {
  // Latch a distinct error state so hysteresis cannot restart COLD. COLD and
  // PUMP stop immediately; FAN follows its existing delayed shutdown policy.
  cooling_safety_reset();
  thermostat_mode = THERMOSTAT_MODE_COOLING_FAULT;
  write_cold_output(false);
  write_pump_output(false);
  write_fan_output(false);
  Serial.println("cooling_fault_stop - COLD disabled because temperature rose");
}
#endif

void write_heat_output(bool active) {
  digitalWrite(HEAT, output_level(HOT_ACTIVE_LOW, active));
}

void write_cold_output(bool active) {
  digitalWrite(COLD, output_level(COLD_ACTIVE_LOW, active));
}

void write_fan_output(bool active) {
  const unsigned long fan_turn_off_delay_ms =
      static_cast<unsigned long>(FAN_TURN_OFF_DELAY_SECONDS) * 1000UL;

  if (active) {
    fan_requested_active = true;
    fan_turn_off_at_ms = 0;
    digitalWrite(FAN, output_level(FAN_ACTIVE_LOW, true));
    return;
  }

  if (fan_turn_off_delay_ms == 0) {
    fan_requested_active = false;
    fan_turn_off_at_ms = 0;
    digitalWrite(FAN, output_level(FAN_ACTIVE_LOW, false));
    return;
  }

  // When the control loop asks to stop the fan, keep it physically on for a
  // short cooldown. This protects heatsinks from heat soak after cooling stops.
  if (fan_requested_active) {
    fan_requested_active = false;
    fan_turn_off_at_ms = millis() + fan_turn_off_delay_ms;
    Serial.printf("write_fan_output - delaying FAN off for %d second(s)\n",
                  FAN_TURN_OFF_DELAY_SECONDS);
  }

  if (fan_turn_off_at_ms != 0 &&
      static_cast<long>(fan_turn_off_at_ms - millis()) > 0) {
    digitalWrite(FAN, output_level(FAN_ACTIVE_LOW, true));
    return;
  }

  fan_turn_off_at_ms = 0;
  digitalWrite(FAN, output_level(FAN_ACTIVE_LOW, false));
}

void write_pump_output(bool active) {
  digitalWrite(PUMP, output_level(PUMP_ACTIVE_LOW, active));
}

void configure_output_off(uint8_t pin, bool active_low) {
  const uint8_t inactive_level = output_level(active_low, false);

  // Preload the ESP32 output latch before enabling the output driver.
  gpio_set_level(static_cast<gpio_num_t>(pin), inactive_level);
  pinMode(pin, OUTPUT);

  // Reinforce the requested level through the Arduino GPIO API.
  digitalWrite(pin, inactive_level);
}

void outputs_init() {
  fan_requested_active = false;
  fan_turn_off_at_ms = 0;

  configure_output_off(HEAT, HOT_ACTIVE_LOW);
  configure_output_off(COLD, COLD_ACTIVE_LOW);
  configure_output_off(FAN, FAN_ACTIVE_LOW);
  configure_output_off(PUMP, PUMP_ACTIVE_LOW);
}

void init_sensors() {
  temp_init_sensor();
}

void setup() {
  // 0. Force every output inactive before any potentially blocking work.
  //    This must be the first hardware action in setup().
  outputs_init();

  Serial.begin(115200);
  delay(1000);

  Serial.println("setup - starting...");

  // 1. configure remaining hardware
  rgbLedWrite(BOARD_RGB_LED_PIN, 0, 0, 0); 
  // set time to Saturday 00:00:00am Jan 1 2025
  setTime(0, 0, 0, 1, 1, 25);

  // 2. init sensors — thermostat is not yet controlling, but sensor is ready
  Serial.println("setup - init sensors");
  init_sensors();

  // 3. init display
  Serial.println("setup - init display");
  init_display(DISPLAY_BUTTON_PIN);

  // Load any persisted feature definitions so offline temperature reads can
  // still use the same display path as received commands.
  Serial.println("setup - loading saved features for display");
  storage_get_features(saved_features);
  feature_values_init_saved_features();

  // 4. instantiate alarms (disabled)
  Serial.println("setup - init alarms (still disabled)...");
  alarms_init();

  // 5. enable alarms — thermostat is OPERATIONAL offline
  //    Temperature reading + hysteretic control will fire every 5s via
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
  Serial.println("setup - starting WiFi (non-blocking, state machine continues in loop)...");
  display_set_connectivity_status(false, false);
  update_display();
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
      display_set_connectivity_status(true, false);
      display_show_message("WiFi status", "Online");
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
        Serial.println("handle_connectivity - no UUID in preferences, need to register");
        conn_state = CONN_REGISTERING;
      } else {
        Serial.println("handle_connectivity - UUID found, skipping registration");
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
      display_set_connectivity_status(false, false);
      update_display();
      Serial.println("handle_connectivity - WiFi lost during registration, restarting WiFi");
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
          Serial.println("handle_connectivity - registered successfully, connecting to MQTT");
          conn_attempts = 0;
          conn_next_attempt_ms = 0;
          feature_values_init_saved_features();
          conn_state = CONN_MQTT_TRYING;
        } else {
          Serial.println("handle_connectivity - registration OK but failed to read preferences");
          conn_attempts++;
          conn_next_attempt_ms = now + CONN_REG_RETRY_MS;
          if (conn_attempts >= CONN_MAX_REG_ATTEMPTS) {
            Serial.println("handle_connectivity - registration max attempts reached, entering cooldown");
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
          Serial.println("handle_connectivity - registration max attempts reached, entering cooldown");
          conn_cooldown_until_ms = now + CONN_COOLDOWN_MS;
          conn_state = CONN_COOLDOWN;
        }
      }
    }
    break;

  case CONN_MQTT_TRYING:
    if (wifi_get_status() != WL_CONNECTED) {
      display_set_connectivity_status(false, false);
      update_display();
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
    display_set_connectivity_status(true, false);
    if (mqtt_try_connect_once(saved_device_uuid)) {
      Serial.println("handle_connectivity - MQTT connected, going online");
      display_set_connectivity_status(true, true);
      display_show_message("MQTT status", "Online");
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
      display_set_connectivity_status(false, false);
      update_display();
      alarm_online_disable();
      conn_attempts = 0;
      wifi_start_connect();
      conn_state = CONN_WIFI_WAITING;
    } else if (!mqtt_client.connected()) {
      Serial.println("handle_connectivity - MQTT dropped, reconnecting...");
      display_set_connectivity_status(true, false);
      update_display();
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
      display_set_connectivity_status(wifi_get_status() == WL_CONNECTED, false);
      update_display();
      alarm_online_disable();
    }
  }

  update_display();
  Alarm.delay(100);
}
