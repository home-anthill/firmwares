#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <functional>
#include <vector>

// Mock headers — must come before any production include.
#include <Arduino.h>
#include "WiFi.h"
#include "WiFiClientSecure.h"
#include "PubSubClient.h"
#include "TimeAlarms.h"

#include <ArduinoJson.h>

// secrets.h must come before any firmware header that uses SSL/MANUFACTURER/MODEL.
#include "config.h"
#include "secrets.h"

// Headers from the firmware (needed for function signatures only).
#include "wifi_handler.h"
#include "mqtt_handler.h"
#include "registration.h"
#include "storage.h"
#include "water_level_sensor.h"
#include "feature_values.h"

// =============================================================================
// Stubs — provide every external symbol that <main-project-arduino-file>.ino references.
//
// None of the dependent .cpp files are compiled into this target.  These stubs
// link cleanly AND let tests control the observable behaviour of each boundary.
// setup() and loop() compile and link but are never called from tests: they
// orchestrate full I/O sequences that are not meaningfully unit-testable.
// =============================================================================

// --- Globals defined in wifi_handler.cpp and mqtt_handler.cpp ---------------

#if SSL == true
WiFiClientSecure wifi_client;
#else
WiFiClient wifi_client;
#endif
PubSubClient     mqtt_client;

// --- wifi_handler -----------------------------------------------------------

void wifi_init_ca()             {}
void wifi_reconnect(char* /*mac*/) {}
static int g_wifi_status = WL_CONNECTED;
void wifi_set_status_for_test(int status) { g_wifi_status = status; }
int wifi_get_status() { return g_wifi_status; }

void wifi_connect(char* mac) {
  strncpy(mac, "aa:bb:cc:dd:ee:ff", 17);
  mac[17] = '\0';
}

// --- registration -----------------------------------------------------------

int register_secure_server(WiFiClientSecure& /*c*/, const char* /*mac*/,
                            const JsonDocument& /*f*/) { return 0; }
int register_insecure_server(WiFiClient& /*c*/, const char* /*mac*/,
                             const JsonDocument& /*f*/) { return 0; }

// --- mqtt_handler -----------------------------------------------------------

void mqtt_init(Client& /*c*/,
               std::function<void(char*, uint8_t*, unsigned int)> /*cb*/) {}
void mqtt_connect(const char* /*uuid*/) {
  MqttMockState::instance().connected_val = true;
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

struct AlarmCall {
  std::string device_uuid;
  std::string feature_uuid;
  std::string feature_name;
  std::string alarm_type;
  float value;
};

struct MqttAlarmCapture {
  std::vector<AlarmCall> calls;

  static MqttAlarmCapture& instance() {
    static MqttAlarmCapture s;
    return s;
  }
  static void reset() { instance().calls.clear(); }
};

void mqtt_notify_alarm(const char* device_uuid, const char* feature_uuid,
                       const char* feature_name, const char* alarm_type, float value) {
  MqttAlarmCapture::instance().calls.push_back({
    device_uuid ? device_uuid : "",
    feature_uuid ? feature_uuid : "",
    feature_name ? feature_name : "",
    alarm_type ? alarm_type : "",
    value
  });
}

// --- display ----------------------------------------------------------------

void init_display(uint8_t) {}
void update_display() {}
void display_force_update() {}
void display_show_message(const char*, const char*) {}
void display_set_connectivity_status(bool, bool) {}
void display_sleep_for(unsigned long) {}

static bool g_water_level = false;
void water_level_init_sensor() {}
bool water_level_read() { return g_water_level; }

// --- feature_values ---------------------------------------------------------

struct FeatureValueSetCall {
  std::string name;
  float value;
};

struct FeatureValueCapture {
  std::vector<FeatureValueSetCall> set_calls;
  int init_calls{0};

  static FeatureValueCapture& instance() {
    static FeatureValueCapture s;
    return s;
  }
  static void reset() { instance() = FeatureValueCapture{}; }
};

void feature_values_init(JsonArray /*features*/) {
  FeatureValueCapture::instance().init_calls++;
}

void feature_values_clear() {
  FeatureValueCapture::reset();
}

size_t feature_values_count() {
  return 0;
}

bool feature_values_set(const char* name, float value) {
  FeatureValueCapture::instance().set_calls.push_back({
    name ? name : "",
    value
  });
  return true;
}

bool feature_values_get(size_t /*index*/, FeatureValue* /*value*/) {
  return false;
}

// --- storage ----------------------------------------------------------------

void   storage_get_features(JsonArray /*arr*/) {}
size_t storage_set_features(JsonArray /*arr*/) { return 0; }

static char   g_stored_uuid[37] = {};
static size_t g_stored_uuid_len  = 0;

size_t storage_get_uuid(char* buf) {
  if (buf) {
    strncpy(buf, g_stored_uuid, 36);
    buf[36] = '\0';
  }
  return g_stored_uuid_len;
}
size_t storage_set_uuid(const char* uuid) {
  strncpy(g_stored_uuid, uuid, 36);
  g_stored_uuid[36] = '\0';
  g_stored_uuid_len = strlen(g_stored_uuid);
  return g_stored_uuid_len;
}

// =============================================================================
// Forward-declare the private functions defined in <main-project-arduino-file>.ino.
// They have no header — declarations here match the definitions in the .ino.
// =============================================================================

bool         get_feature_uuid_by_name(char* featureUuid, size_t max_len, const char* name);
JsonDocument buildFeatures();
void read_and_send_water_level_value();
void publish_initial_values();
void alarms_init();
void alarms_enable();
void alarms_disable();
extern bool previous_water_level;
extern AlarmID_t alarm_water_level;
extern AlarmID_t alarm_online;
void         send_online_status();
void         publish_initial_values();
void         loop();

// Expose <main-project-arduino-file>.ino globals so the fixture can reset them.
extern JsonDocument doc_features;
extern JsonArray    saved_features;
extern char         saved_device_uuid[37];

// =============================================================================
// Fixture — resets all shared state before every test.
// =============================================================================

class MainInoTest : public ::testing::Test {
protected:
  void SetUp() override {
    doc_features.clear();
    saved_features = doc_features.to<JsonArray>();
    memset(saved_device_uuid, 0, sizeof(saved_device_uuid));

    MqttNotifyCapture::reset();
    MqttAlarmCapture::reset();
    previous_water_level = false;
    MqttMockState::reset();
    FeatureValueCapture::reset();
    g_water_level = false;
    g_wifi_status = WL_CONNECTED;
    Alarm.reset();
    g_stored_uuid_len    = 0;
    memset(g_stored_uuid, 0, sizeof(g_stored_uuid));
  }

  void addFeature(const char* name, const char* uuid) {
    JsonObject f = saved_features.add<JsonObject>();
    f["name"] = name;
    f["uuid"] = uuid;
  }
};

TEST_F(MainInoTest, RegistersThresholdBeforeOnlineAsBooleans) {
  JsonDocument doc = buildFeatures();
  ASSERT_EQ(doc.size(), 2u);
  EXPECT_STREQ(doc[0]["name"], "level");
  EXPECT_STREQ(doc[1]["name"], "online");
  for (int i = 0; i < 2; ++i) {
    EXPECT_STREQ(doc[i]["type"], "sensor");
    EXPECT_STREQ(doc[i]["spec"]["format"], "bool");
    EXPECT_EQ(doc[i]["order"].as<int>(), i + 1);
  }
}

TEST_F(MainInoTest, PublishesBothThresholdStatesAndUpdatesCache) {
  addFeature("level", "water-feature");
  for (bool value : {true, false}) {
    g_water_level = value;
    read_and_send_water_level_value();
    const auto& call = MqttNotifyCapture::instance().calls.back();
    EXPECT_EQ(call.type, "level");
    EXPECT_EQ(call.feature_uuid, "water-feature");
    EXPECT_FLOAT_EQ(call.value, value ? 1 : 0);
    EXPECT_FLOAT_EQ(FeatureValueCapture::instance().set_calls.back().value, value ? 1 : 0);
  }
}

TEST_F(MainInoTest, PublishesAlarmOnlyWhenWaterReachesThreshold) {
  addFeature("level", "water-feature");
  strcpy(saved_device_uuid, "device-uuid");
  for (bool value : {false, true, true, false}) {
    g_water_level = value;
    read_and_send_water_level_value();
  }
  ASSERT_EQ(MqttAlarmCapture::instance().calls.size(), 1u);
  const auto& call = MqttAlarmCapture::instance().calls[0];
  EXPECT_EQ(call.device_uuid, "device-uuid");
  EXPECT_EQ(call.feature_uuid, "water-feature");
  EXPECT_EQ(call.feature_name, "level");
  EXPECT_EQ(call.alarm_type, "level");
  EXPECT_FLOAT_EQ(call.value, 1.0f);
  g_water_level = true;
  read_and_send_water_level_value();
  EXPECT_EQ(MqttAlarmCapture::instance().calls.size(), 2u);
}

TEST_F(MainInoTest, MissingFeatureDoesNotPublish) {
  g_water_level = true;
  read_and_send_water_level_value();
  EXPECT_TRUE(MqttAlarmCapture::instance().calls.empty());
  EXPECT_TRUE(MqttNotifyCapture::instance().calls.empty());
}

TEST_F(MainInoTest, ReconnectPublishesThresholdAndOnlineAndEnablesAlarms) {
  strcpy(saved_device_uuid, "device-test");
  addFeature("level", "water-feature");
  addFeature("online", "online-feature");
  alarms_init();
  EXPECT_EQ(Alarm.period(alarm_water_level), 1UL);
  EXPECT_EQ(Alarm.period(alarm_online), 60UL);
  EXPECT_FALSE(Alarm.isEnabled(alarm_water_level));
  EXPECT_FALSE(Alarm.isEnabled(alarm_online));
  MqttMockState::instance().connected_val = false;
  loop();
  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 2u);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "level");
  EXPECT_EQ(MqttNotifyCapture::instance().calls[1].type, "online");
  EXPECT_TRUE(Alarm.isEnabled(alarm_water_level));
  EXPECT_TRUE(Alarm.isEnabled(alarm_online));
  alarms_disable();
  EXPECT_FALSE(Alarm.isEnabled(alarm_water_level));
  EXPECT_FALSE(Alarm.isEnabled(alarm_online));
}

TEST_F(MainInoTest, WifiLossDisablesAlarms) {
  strcpy(saved_device_uuid, "device-test");
  alarms_init();
  alarms_enable();
  MqttMockState::instance().connected_val = true;
  wifi_set_status_for_test(WL_DISCONNECTED);
  loop();
  EXPECT_FALSE(Alarm.isEnabled(alarm_water_level));
  EXPECT_FALSE(Alarm.isEnabled(alarm_online));
}
