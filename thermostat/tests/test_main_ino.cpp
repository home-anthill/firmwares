#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

// Mock headers — must come before any production include.
#include <Arduino.h>
#include "WiFi.h"
#include "WiFiClientSecure.h"
#include "PubSubClient.h"
#include "TimeAlarms.h"

#include <ArduinoJson.h>

// secrets.h must come before any firmware header that uses SSL/MODEL/API_TOKEN.
#include "secrets.h"

#ifndef HOT_ACTIVE_LOW
#define HOT_ACTIVE_LOW false
#endif

#ifndef COLD_ACTIVE_LOW
#define COLD_ACTIVE_LOW true
#endif

#ifndef FAN_ACTIVE_LOW
#define FAN_ACTIVE_LOW false
#endif

#ifndef PUMP_ACTIVE_LOW
#define PUMP_ACTIVE_LOW false
#endif

// Headers from the firmware (needed for function signatures only).
#include "wifi_handler.h"
#include "mqtt_handler.h"
#include "registration.h"
#include "storage.h"
#include "temp_sensor.h"
#include "display.h"
#include "feature_values.h"
#include "controller.h"

// =============================================================================
// Stubs — provide every external symbol that thermostat.ino references.
//
// None of the dependent .cpp files are compiled into this target.  These stubs
// link cleanly AND let tests control the observable behaviour of each boundary.
// setup() and loop() compile and link but are never called from tests.
// =============================================================================

// --- Globals defined in wifi_handler.cpp and mqtt_handler.cpp ---------------

#if SSL == true
WiFiClientSecure wifi_client;
#else
WiFiClient wifi_client;
#endif
PubSubClient mqtt_client;

// --- wifi_handler -----------------------------------------------------------

void wifi_init_ca()              {}
void wifi_start_connect()        {}
void wifi_connect(char* mac)     { strncpy(mac, "aa:bb:cc:dd:ee:ff", 17); mac[17] = '\0'; }
void wifi_reconnect(char* /*mac*/) {}
static int g_wifi_status = WL_CONNECTED;
static int g_wifi_sync_time_calls = 0;
int  wifi_get_status()           { return g_wifi_status; }
void wifi_sync_time()            { g_wifi_sync_time_calls++; }

void wifi_populate_mac(char* mac) {
  strncpy(mac, "aa:bb:cc:dd:ee:ff", 17);
  mac[17] = '\0';
}

// --- registration -----------------------------------------------------------

int register_insecure_server(WiFiClient& /*c*/, const char* /*mac*/,
                              const JsonDocument& /*f*/) { return 0; }
int register_secure_server(WiFiClientSecure& /*c*/, const char* /*mac*/,
                            const JsonDocument& /*f*/) { return 0; }
int register_insecure_server_once(WiFiClient& /*c*/, const char* /*mac*/,
                                  const JsonDocument& /*f*/) { return 0; }
int register_secure_server_once(WiFiClientSecure& /*c*/, const char* /*mac*/,
                                const JsonDocument& /*f*/) { return 0; }

// --- mqtt_handler -----------------------------------------------------------

void mqtt_init(Client& /*c*/,
               std::function<void(char*, uint8_t*, unsigned int)> /*cb*/) {}
void mqtt_connect(const char* /*uuid*/) {}
bool mqtt_try_connect_once(const char* /*uuid*/) {
  auto& state = MqttMockState::instance();
  state.connected_val = state.connect_result;
  return state.connect_result;
}

// Capture every mqtt_notify_value() call so tests can inspect it.
struct NotifyCall {
  std::string device_uuid;
  std::string feature_uuid;
  std::string type;
  float       value;
};

struct MqttNotifyCapture {
  std::vector<NotifyCall> calls;

  static MqttNotifyCapture& instance() {
    static MqttNotifyCapture s;
    return s;
  }
  static void reset() { instance().calls.clear(); }
};

void mqtt_notify_value(const char* device_uuid, const char* feature_uuid,
                       const char* type, float value) {
  MqttNotifyCapture::instance().calls.push_back({
    device_uuid  ? device_uuid  : "",
    feature_uuid ? feature_uuid : "",
    type         ? type         : "",
    value
  });
}

// --- storage ----------------------------------------------------------------

void   storage_get_features(JsonArray /*arr*/)     {}
size_t storage_set_features(JsonArray /*arr*/)     { return 0; }
void   storage_get_feature_values(JsonArray /*arr*/) {}
size_t storage_set_feature_values(JsonArray /*arr*/) { return 0; }

static char   g_stored_uuid[37] = {};
static size_t g_stored_uuid_len  = 0;

size_t storage_get_uuid(char* buf) {
  if (buf) { strncpy(buf, g_stored_uuid, 36); buf[36] = '\0'; }
  return g_stored_uuid_len;
}
size_t storage_set_uuid(const char* uuid) {
  strncpy(g_stored_uuid, uuid, 36);
  g_stored_uuid[36] = '\0';
  g_stored_uuid_len = strlen(g_stored_uuid);
  return g_stored_uuid_len;
}

// --- feature_values ---------------------------------------------------------

struct FeatureValuesCapture {
  std::vector<NotifyCall> values;

  static FeatureValuesCapture& instance() {
    static FeatureValuesCapture s;
    return s;
  }
  static void reset() { instance().values.clear(); }
};

void feature_values_init(JsonArray /*features*/) {}
void feature_values_clear() {}
size_t feature_values_count() {
  return FeatureValuesCapture::instance().values.size();
}
bool feature_values_get(size_t index, FeatureValue* value) {
  auto& values = FeatureValuesCapture::instance().values;
  if (value == nullptr || index >= values.size()) {
    return false;
  }

  memset(value, 0, sizeof(*value));
  strncpy(value->name, values[index].type.c_str(), sizeof(value->name) - 1);
  strncpy(value->unit, "°C", sizeof(value->unit) - 1);
  value->value = values[index].value;
  value->has_value = true;
  return true;
}
bool feature_values_set(const char* name, float value) {
  FeatureValuesCapture::instance().values.push_back({
    "",
    "",
    name ? name : "",
    value
  });
  return true;
}

// --- temp_sensor (controllable return values) --------------------------------

static float g_temp = NAN;
void  temp_init_sensor()      {}
float temp_get_temperature()  { return g_temp; }

// --- display (no-op; update_display calls are counted) ----------------------

size_t display_feature_index = 0;
static int g_display_message_calls = 0;
static std::string g_last_display_message_title;
static std::string g_last_display_message_detail;

struct UpdateDisplayCapture {
  std::string last_name;
  float last_value{NAN};
  std::string last_unit;
  int   call_count{0};
  static UpdateDisplayCapture& instance() {
    static UpdateDisplayCapture s;
    return s;
  }
  static void reset() { instance() = UpdateDisplayCapture{}; }
};

void init_display(uint8_t)     {}
void update_display() {
  auto& values = FeatureValuesCapture::instance().values;
  if (display_feature_index < values.size()) {
    UpdateDisplayCapture::instance().last_name = values[display_feature_index].type;
    UpdateDisplayCapture::instance().last_value = values[display_feature_index].value;
    UpdateDisplayCapture::instance().last_unit = "°C";
  }
  UpdateDisplayCapture::instance().call_count++;
}
void display_force_update() {
  update_display();
}
void display_show_message(const char* title, const char* detail) {
  g_display_message_calls++;
  g_last_display_message_title = title == nullptr ? "" : title;
  g_last_display_message_detail = detail == nullptr ? "" : detail;
}
void display_set_connectivity_status(bool, bool) {}
void display_sleep_for(unsigned long) {}

// --- controller (controllable setpoint/tolerance; set_configuration captured) ---

static float g_setpoint  = 20.0f;
static float g_tolerance  = 5.0f;

float get_setpoint()  { return g_setpoint; }
float get_tolerance() { return g_tolerance; }

struct SetConfigCall {
  std::string payload;
  unsigned int length;
};
struct SetConfigCapture {
  std::vector<SetConfigCall> calls;
  static SetConfigCapture& instance() {
    static SetConfigCapture s;
    return s;
  }
  static void reset() { instance().calls.clear(); }
};

void set_configuration(const char* /*uuid*/, const char* /*mac*/,
                       JsonArray /*features*/,
                       uint8_t* payload, unsigned int length) {
  SetConfigCapture::instance().calls.push_back({
    payload ? std::string(reinterpret_cast<char*>(payload), length) : "",
    length
  });
}

// =============================================================================
// Forward-declare functions defined in thermostat.ino (no header).
// =============================================================================

bool         get_feature_uuid_by_name(char* featureUuid, size_t max_len, const char* name);
JsonDocument buildFeatures();
void         read_temp_sensor_value();
void         send_online_status();
void         alarms_init();
void         alarm_temperature_enable();
void         alarm_online_enable();
void         alarm_online_disable();
void         outputs_init();
void         outputs_all_off();

// Forward-declare mqtt_callback (defined in thermostat.ino, no header).
void mqtt_callback(char* topic, uint8_t* payload, unsigned int length);

void         handle_connectivity();

enum ConnState {
  CONN_WIFI_WAITING,
  CONN_REGISTERING,
  CONN_MQTT_TRYING,
  CONN_ONLINE,
  CONN_COOLDOWN
};

extern ConnState     conn_state;
extern int           conn_attempts;
extern unsigned long conn_next_attempt_ms;
extern unsigned long conn_cooldown_until_ms;

static constexpr int WIFI_POLL_LIMIT = 45;
static constexpr unsigned long WIFI_POLL_INTERVAL_MS = 2000;

// Expose thermostat.ino globals so the fixture can reset them between tests.
extern JsonDocument doc_features;
extern JsonArray    saved_features;
extern char         saved_device_uuid[37];
extern AlarmID_t    alarm_temp;
extern AlarmID_t    alarm_online;

// =============================================================================
// Constants — pin numbers matching thermostat.ino #defines
// =============================================================================

static constexpr uint8_t PIN_HEAT = 4;
static constexpr uint8_t PIN_COLD = 5;
static constexpr uint8_t PIN_FAN  = 6;
static constexpr uint8_t PIN_PUMP = 7;

static constexpr uint8_t outputLevel(bool active_low, bool active) {
  return active ? (active_low ? LOW : HIGH) : (active_low ? HIGH : LOW);
}

static constexpr uint8_t HEAT_ON = outputLevel(HOT_ACTIVE_LOW, true);
static constexpr uint8_t HEAT_OFF = outputLevel(HOT_ACTIVE_LOW, false);
static constexpr uint8_t COLD_ON = outputLevel(COLD_ACTIVE_LOW, true);
static constexpr uint8_t COLD_OFF = outputLevel(COLD_ACTIVE_LOW, false);
static constexpr uint8_t FAN_ON = outputLevel(FAN_ACTIVE_LOW, true);
static constexpr uint8_t FAN_OFF = outputLevel(FAN_ACTIVE_LOW, false);
static constexpr uint8_t PUMP_ON = outputLevel(PUMP_ACTIVE_LOW, true);
static constexpr uint8_t PUMP_OFF = outputLevel(PUMP_ACTIVE_LOW, false);

// =============================================================================
// Fixture
// =============================================================================

class MainInoTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Reset .ino globals.
    doc_features.clear();
    saved_features = doc_features.to<JsonArray>();
    memset(saved_device_uuid, 0, sizeof(saved_device_uuid));

    // Reset stubs.
    MqttNotifyCapture::reset();
    UpdateDisplayCapture::reset();
    FeatureValuesCapture::reset();
    SetConfigCapture::reset();
    MqttMockState::reset();
    GpioMockState::reset();
    Alarm.reset();

    g_temp        = NAN;
    g_setpoint    = 20.0f;
    g_tolerance   =  5.0f;
    g_wifi_status = WL_CONNECTED;
    g_wifi_sync_time_calls = 0;
    g_stored_uuid_len = 0;
    memset(g_stored_uuid, 0, sizeof(g_stored_uuid));
    conn_state = CONN_WIFI_WAITING;
    conn_attempts = 0;
    conn_next_attempt_ms = 0;
    conn_cooldown_until_ms = 0;
    display_feature_index = 0;
    g_display_message_calls = 0;
    g_last_display_message_title.clear();
    g_last_display_message_detail.clear();
    mock_set_millis(0);
  }

  void addFeature(const char* name, const char* uuid) {
    JsonObject f = saved_features.add<JsonObject>();
    f["name"] = name;
    f["uuid"] = uuid;
  }
};

TEST_F(MainInoTest, OutputsInitStartsAllOutputsOff) {
  outputs_init();

  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_HEAT], HEAT_OFF);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_COLD], COLD_OFF);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_PUMP], PUMP_OFF);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_FAN],  FAN_OFF);
}

// =============================================================================
// get_feature_uuid_by_name
// =============================================================================

TEST_F(MainInoTest, GetFeatureUuidByNameFindsAllFeatures) {
  addFeature("setpoint",    "sp-uuid-0000-0000-000000000001");
  addFeature("tolerance",   "tl-uuid-0000-0000-000000000002");
  addFeature("temperature", "tm-uuid-0000-0000-000000000003");
  addFeature("online",      "on-uuid-0000-0000-000000000004");

  char buf[37] = {};
  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "setpoint"));
  EXPECT_STREQ(buf, "sp-uuid-0000-0000-000000000001");

  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "tolerance"));
  EXPECT_STREQ(buf, "tl-uuid-0000-0000-000000000002");

  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "temperature"));
  EXPECT_STREQ(buf, "tm-uuid-0000-0000-000000000003");

  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "online"));
  EXPECT_STREQ(buf, "on-uuid-0000-0000-000000000004");
}

TEST_F(MainInoTest, GetFeatureUuidByNameReturnsFalseWhenNotFound) {
  addFeature("setpoint", "sp-uuid-0000-0000-000000000001");

  char buf[37] = {};
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "humidity"));
  EXPECT_STREQ(buf, "");
}

TEST_F(MainInoTest, GetFeatureUuidByNameReturnsFalseOnEmptyFeatures) {
  char buf[37] = {};
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "temperature"));
  EXPECT_STREQ(buf, "");
}

TEST_F(MainInoTest, GetFeatureUuidByNameSkipsEntriesWithNullFields) {
  // Entry with name but no uuid → skipped.
  JsonObject bad = saved_features.add<JsonObject>();
  bad["name"] = "setpoint";

  addFeature("tolerance", "tl-uuid-0000-0000-000000000002");

  char buf[37] = {};
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "setpoint"));
  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "tolerance"));
}

TEST_F(MainInoTest, GetFeatureUuidByNameTruncatesUuidToBufferSize) {
  addFeature("setpoint", "sp-uuid-0000-0000-000000000001");

  char buf[5] = {};
  get_feature_uuid_by_name(buf, sizeof(buf), "setpoint");
  EXPECT_EQ(buf[sizeof(buf) - 1], '\0');
  EXPECT_EQ(strnlen(buf, sizeof(buf)), 4u);
}

// =============================================================================
// buildFeatures
// =============================================================================

TEST_F(MainInoTest, BuildFeaturesReturnsFourFeatures) {
  JsonDocument result = buildFeatures();
  JsonArray    arr    = result.as<JsonArray>();

  ASSERT_EQ(arr.size(), 4u);
  EXPECT_STREQ(arr[0]["name"].as<const char*>(), "setpoint");
  EXPECT_STREQ(arr[1]["name"].as<const char*>(), "tolerance");
  EXPECT_STREQ(arr[2]["name"].as<const char*>(), "temperature");
  EXPECT_STREQ(arr[3]["name"].as<const char*>(), "online");
}

TEST_F(MainInoTest, BuildFeaturesHasCorrectFieldsPerEntry) {
  JsonDocument result = buildFeatures();
  JsonArray    arr    = result.as<JsonArray>();

  // setpoint — controller
  EXPECT_STREQ(arr[0]["type"].as<const char*>(), "controller");
  EXPECT_STREQ(arr[0]["unit"].as<const char*>(), "°C");
  EXPECT_TRUE(arr[0]["enable"].as<bool>());
  EXPECT_EQ(arr[0]["order"].as<int>(), 1);

  // tolerance — controller
  EXPECT_STREQ(arr[1]["type"].as<const char*>(), "controller");
  EXPECT_STREQ(arr[1]["unit"].as<const char*>(), "°C");
  EXPECT_TRUE(arr[1]["enable"].as<bool>());
  EXPECT_EQ(arr[1]["order"].as<int>(), 2);

  // temperature — sensor
  EXPECT_STREQ(arr[2]["type"].as<const char*>(), "sensor");
  EXPECT_STREQ(arr[2]["unit"].as<const char*>(), "°C");
  EXPECT_TRUE(arr[2]["enable"].as<bool>());
  EXPECT_EQ(arr[2]["order"].as<int>(), 3);

  // online — sensor
  EXPECT_STREQ(arr[3]["type"].as<const char*>(), "sensor");
  EXPECT_STREQ(arr[3]["unit"].as<const char*>(), "-");
  EXPECT_TRUE(arr[3]["enable"].as<bool>());
  EXPECT_EQ(arr[3]["order"].as<int>(), 4);
}

TEST_F(MainInoTest, BuildFeaturesIncludesAdmissionSpecs) {
  JsonDocument result = buildFeatures();
  JsonArray    arr    = result.as<JsonArray>();

  JsonObject setpointSpec = arr[0]["spec"].as<JsonObject>();
  ASSERT_FALSE(setpointSpec.isNull());
  EXPECT_STREQ(setpointSpec["format"].as<const char*>(), "float");
  EXPECT_FLOAT_EQ(setpointSpec["min"].as<float>(), 10.0f);
  EXPECT_FLOAT_EQ(setpointSpec["max"].as<float>(), 30.0f);
  EXPECT_FLOAT_EQ(setpointSpec["step"].as<float>(), 0.5f);
  EXPECT_TRUE(setpointSpec["list"].isNull());

  JsonObject toleranceSpec = arr[1]["spec"].as<JsonObject>();
  ASSERT_FALSE(toleranceSpec.isNull());
  EXPECT_STREQ(toleranceSpec["format"].as<const char*>(), "float");
  EXPECT_FLOAT_EQ(toleranceSpec["min"].as<float>(), 0.0f);
  EXPECT_FLOAT_EQ(toleranceSpec["max"].as<float>(), 10.0f);
  EXPECT_FLOAT_EQ(toleranceSpec["step"].as<float>(), 0.5f);
  EXPECT_TRUE(toleranceSpec["list"].isNull());

  JsonObject temperatureSpec = arr[2]["spec"].as<JsonObject>();
  ASSERT_FALSE(temperatureSpec.isNull());
  EXPECT_STREQ(temperatureSpec["format"].as<const char*>(), "float");
  EXPECT_FLOAT_EQ(temperatureSpec["min"].as<float>(), -40.0f);
  EXPECT_FLOAT_EQ(temperatureSpec["max"].as<float>(), 200.0f);
  EXPECT_FLOAT_EQ(temperatureSpec["step"].as<float>(), 0.01f);
  EXPECT_TRUE(temperatureSpec["list"].isNull());

  JsonObject onlineSpec = arr[3]["spec"].as<JsonObject>();
  ASSERT_FALSE(onlineSpec.isNull());
  EXPECT_STREQ(onlineSpec["format"].as<const char*>(), "bool");
  EXPECT_TRUE(onlineSpec["list"].isNull());
}

// =============================================================================
// read_temp_sensor_value — NaN guard
// =============================================================================

TEST_F(MainInoTest, ReadTempSkipsEverythingWhenNan) {
  g_temp = NAN;

  read_temp_sensor_value();

  EXPECT_EQ(MqttNotifyCapture::instance().calls.size(), 0u);
  EXPECT_EQ(UpdateDisplayCapture::instance().call_count, 0);
  EXPECT_TRUE(GpioMockState::instance().pin_values.empty());
}

// =============================================================================
// read_temp_sensor_value — display update
// =============================================================================

TEST_F(MainInoTest, ReadTempCachesDisplayValueWithoutForcingRefresh) {
  g_temp = 21.5f;

  read_temp_sensor_value();

  EXPECT_EQ(UpdateDisplayCapture::instance().call_count, 0);
  ASSERT_EQ(FeatureValuesCapture::instance().values.size(), 1u);
  EXPECT_EQ(FeatureValuesCapture::instance().values[0].type, "temperature");
  EXPECT_FLOAT_EQ(FeatureValuesCapture::instance().values[0].value, 21.5f);
}

// =============================================================================
// read_temp_sensor_value — MQTT publish
// =============================================================================

TEST_F(MainInoTest, ReadTempPublishesWhenConnectedAndFeatureFound) {
  MqttMockState::instance().connected_val = true;
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("temperature", "tm-uuid-0000-0000-000000000003");
  g_temp = 22.0f;

  read_temp_sensor_value();

  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1u);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "temperature");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[0].value, 22.0f);
}

TEST_F(MainInoTest, ReadTempSkipsPublishWhenNotConnected) {
  MqttMockState::instance().connected_val = false;
  addFeature("temperature", "tm-uuid-0000-0000-000000000003");
  g_temp = 22.0f;

  read_temp_sensor_value();

  EXPECT_EQ(MqttNotifyCapture::instance().calls.size(), 0u);
}

TEST_F(MainInoTest, ReadTempSkipsPublishWhenFeatureNotFound) {
  MqttMockState::instance().connected_val = true;
  // No features registered → UUID lookup fails.
  g_temp = 22.0f;

  read_temp_sensor_value();

  EXPECT_EQ(MqttNotifyCapture::instance().calls.size(), 0u);
}

// =============================================================================
// send_online_status and alarms
// =============================================================================

TEST_F(MainInoTest, SendOnlineSkipsPublishWhenNotConnected) {
  MqttMockState::instance().connected_val = false;
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("online", "on-uuid-0000-0000-000000000004");

  send_online_status();

  EXPECT_EQ(MqttNotifyCapture::instance().calls.size(), 0u);
}

TEST_F(MainInoTest, SendOnlinePublishesWhenConnectedAndFeatureFound) {
  MqttMockState::instance().connected_val = true;
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("online", "on-uuid-0000-0000-000000000004");

  send_online_status();

  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1u);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "online");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[0].value, 1.0f);
}

TEST_F(MainInoTest, TemperatureAlarmCanBeEnabledWithoutOnlineAlarm) {
  alarms_init();

  alarm_temperature_enable();

  EXPECT_TRUE(Alarm.isEnabled(alarm_temp));
  EXPECT_FALSE(Alarm.isEnabled(alarm_online));
}

TEST_F(MainInoTest, MqttConnectEnablesOnlineAlarmAndPublishesInitialOnline) {
  alarms_init();
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("online", "on-uuid-0000-0000-000000000004");
  conn_state = CONN_MQTT_TRYING;

  handle_connectivity();

  EXPECT_EQ(conn_state, CONN_ONLINE);
  EXPECT_TRUE(Alarm.isEnabled(alarm_online));
  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1u);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "online");
}

TEST_F(MainInoTest, MqttDropDisablesOnlineAlarm) {
  alarms_init();
  alarm_online_enable();
  conn_state = CONN_ONLINE;
  MqttMockState::instance().connected_val = false;

  handle_connectivity();

  EXPECT_EQ(conn_state, CONN_MQTT_TRYING);
  EXPECT_FALSE(Alarm.isEnabled(alarm_online));
}

// =============================================================================
// read_temp_sensor_value — hysteretic GPIO control
// =============================================================================

TEST_F(MainInoTest, ReadTempTooHotActivatesCooling) {
  // temp(28) > setpoint(20) + tolerance(5) = 25 → cooling branch
  g_temp      = 28.0f;
  g_setpoint  = 20.0f;
  g_tolerance =  5.0f;

  read_temp_sensor_value();

  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_HEAT], HEAT_OFF);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_COLD], COLD_ON);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_PUMP], PUMP_ON);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_FAN],  FAN_ON);
}

TEST_F(MainInoTest, ReadTempTooColdActivatesHeating) {
  // temp(12) < setpoint(20) - tolerance(5) = 15 → heating branch
  g_temp      = 12.0f;
  g_setpoint  = 20.0f;
  g_tolerance =  5.0f;

  read_temp_sensor_value();

  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_HEAT], HEAT_ON);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_COLD], COLD_OFF);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_PUMP], PUMP_OFF);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_FAN],  FAN_OFF);
}

TEST_F(MainInoTest, ReadTempInRangeTurnsAllOff) {
  // temp(20) in [15, 25] → idle branch
  g_temp      = 20.0f;
  g_setpoint  = 20.0f;
  g_tolerance =  5.0f;

  read_temp_sensor_value();

  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_HEAT], HEAT_OFF);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_COLD], COLD_OFF);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_PUMP], PUMP_OFF);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_FAN],  FAN_OFF);
}

TEST_F(MainInoTest, ReadTempExactlyAtUpperBoundIsInRange) {
  // temp == setpoint + tolerance → NOT in "too hot" branch (condition is >)
  g_temp      = 25.0f;  // exactly setpoint(20) + tolerance(5)
  g_setpoint  = 20.0f;
  g_tolerance =  5.0f;

  read_temp_sensor_value();

  // Falls into the idle (else) branch.
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_HEAT], HEAT_OFF);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_COLD], COLD_OFF);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_PUMP], PUMP_OFF);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_FAN],  FAN_OFF);
}

TEST_F(MainInoTest, ReadTempExactlyAtLowerBoundIsInRange) {
  // temp == setpoint - tolerance → NOT in "too cold" branch (condition is <)
  g_temp      = 15.0f;  // exactly setpoint(20) - tolerance(5)
  g_setpoint  = 20.0f;
  g_tolerance =  5.0f;

  read_temp_sensor_value();

  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_HEAT], HEAT_OFF);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_COLD], COLD_OFF);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_PUMP], PUMP_OFF);
  EXPECT_EQ(GpioMockState::instance().pin_values[PIN_FAN],  FAN_OFF);
}

// =============================================================================
// mqtt_callback — delegates to set_configuration
// =============================================================================

TEST_F(MainInoTest, MqttCallbackDelegatesToSetConfiguration) {
  const char* topic   = "devices/dev-uuid/values";
  const char* payload = R"([{"featureName":"setpoint","value":22}])";
  auto*        p      = reinterpret_cast<uint8_t*>(const_cast<char*>(payload));
  unsigned int len    = static_cast<unsigned int>(strlen(payload));

  mqtt_callback(const_cast<char*>(topic), p, len);

  ASSERT_EQ(SetConfigCapture::instance().calls.size(), 1u);
  EXPECT_NE(SetConfigCapture::instance().calls[0].payload.find("setpoint"),
            std::string::npos);
  EXPECT_EQ(SetConfigCapture::instance().calls[0].length, len);
  ASSERT_EQ(FeatureValuesCapture::instance().values.size(), 1u);
  EXPECT_EQ(FeatureValuesCapture::instance().values[0].type, "setpoint");
  EXPECT_FLOAT_EQ(FeatureValuesCapture::instance().values[0].value, 22.0f);
  EXPECT_EQ(g_display_message_calls, 1);
  EXPECT_EQ(g_last_display_message_title, "Command");
  EXPECT_EQ(g_last_display_message_detail, "Received");
}

TEST_F(MainInoTest, MqttCallbackRecordsCommandValuesAndShowsCommandMessage) {
  const char* topic   = "devices/dev-uuid/values";
  const char* payload = R"([{"featureName":"setpoint","value":22},{"featureName":"tolerance","value":1.5}])";
  auto*        p      = reinterpret_cast<uint8_t*>(const_cast<char*>(payload));

  alarms_init();
  mqtt_callback(const_cast<char*>(topic), p,
                static_cast<unsigned int>(strlen(payload)));

  ASSERT_EQ(FeatureValuesCapture::instance().values.size(), 2u);
  EXPECT_EQ(FeatureValuesCapture::instance().values[0].type, "setpoint");
  EXPECT_FLOAT_EQ(FeatureValuesCapture::instance().values[0].value, 22.0f);
  EXPECT_EQ(FeatureValuesCapture::instance().values[1].type, "tolerance");
  EXPECT_FLOAT_EQ(FeatureValuesCapture::instance().values[1].value, 1.5f);
  EXPECT_EQ(g_display_message_calls, 1);
  EXPECT_EQ(g_last_display_message_title, "Command");
  EXPECT_EQ(g_last_display_message_detail, "Received");
}

TEST_F(MainInoTest, TemperatureReadUpdatesCacheAfterCommandMessage) {
  const char* topic   = "devices/dev-uuid/values";
  const char* payload = R"([{"featureName":"setpoint","value":22},{"featureName":"tolerance","value":1.5}])";
  auto*        p      = reinterpret_cast<uint8_t*>(const_cast<char*>(payload));

  alarms_init();
  mqtt_callback(const_cast<char*>(topic), p,
                static_cast<unsigned int>(strlen(payload)));
  ASSERT_EQ(g_display_message_calls, 1);

  g_temp = 19.25f;
  read_temp_sensor_value();

  ASSERT_EQ(FeatureValuesCapture::instance().values.size(), 3u);
  EXPECT_EQ(FeatureValuesCapture::instance().values[0].type, "setpoint");
  EXPECT_EQ(FeatureValuesCapture::instance().values[1].type, "tolerance");
  EXPECT_EQ(FeatureValuesCapture::instance().values[2].type, "temperature");
  EXPECT_FLOAT_EQ(FeatureValuesCapture::instance().values[2].value, 19.25f);
}

TEST_F(MainInoTest, WifiConnectedSyncsTimeBeforeRegistrationOrMqtt) {
  handle_connectivity();

  EXPECT_EQ(g_wifi_sync_time_calls, 1);
  EXPECT_NE(conn_state, CONN_WIFI_WAITING);
}

TEST_F(MainInoTest, WifiPollingDoesNotBurnAttemptsBeforeRetryWindowElapses) {
  g_wifi_status = 0;

  handle_connectivity();
  EXPECT_EQ(conn_attempts, 1);

  for (int i = 0; i < 19; ++i) {
    mock_advance_millis(100);
    handle_connectivity();
  }

  EXPECT_EQ(conn_attempts, 1);
  EXPECT_EQ(conn_state, CONN_WIFI_WAITING);
  EXPECT_EQ(conn_cooldown_until_ms, 0UL);
}

TEST_F(MainInoTest, WifiPollingEntersCooldownOnlyAfterTimedPollBudgetIsSpent) {
  g_wifi_status = 0;

  handle_connectivity();
  EXPECT_EQ(conn_attempts, 1);

  for (int i = 0; i < WIFI_POLL_LIMIT - 1; ++i) {
    mock_advance_millis(WIFI_POLL_INTERVAL_MS);
    handle_connectivity();
  }

  EXPECT_EQ(conn_attempts, WIFI_POLL_LIMIT);
  EXPECT_EQ(conn_state, CONN_WIFI_WAITING);
  EXPECT_EQ(conn_cooldown_until_ms, 0UL);

  mock_advance_millis(WIFI_POLL_INTERVAL_MS);
  handle_connectivity();

  EXPECT_EQ(conn_attempts, WIFI_POLL_LIMIT + 1);
  EXPECT_EQ(conn_state, CONN_COOLDOWN);
  EXPECT_GT(conn_cooldown_until_ms, millis());
}
