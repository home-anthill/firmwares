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
#include "motion_sensor.h"
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
int  wifi_get_status()          { return WL_CONNECTED; }

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
void mqtt_connect(const char* /*uuid*/) {}

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

// --- motion_sensor (controllable return values) --------------------------------

static int g_motion_prev_value = -1;
static int g_motion_value      = 0;
void motion_init_sensor()       {}
int  motion_get_prev_value()    { return g_motion_prev_value; }
int  motion_get_value()         { return g_motion_value; }

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
void         read_and_send_motion_value();
void         send_online_status();
void         alarms_init();

// Expose <main-project-arduino-file>.ino globals so the fixture can reset them.
extern JsonDocument doc_features;
extern JsonArray    saved_features;
extern char         saved_device_uuid[37];
extern AlarmID_t    alarm_motion;

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
    FeatureValueCapture::reset();
    Alarm.reset();
    g_motion_prev_value          = -1;
    g_motion_value               = 0;
    g_stored_uuid_len         = 0;
    memset(g_stored_uuid, 0, sizeof(g_stored_uuid));
  }

  void addFeature(const char* name, const char* uuid) {
    JsonObject f = saved_features.add<JsonObject>();
    f["name"] = name;
    f["uuid"] = uuid;
  }
};

// =============================================================================
// get_feature_uuid_by_name
// =============================================================================

TEST_F(MainInoTest, GetFeatureUuidByNameFindsMatchingFeature) {
  addFeature("online", "on-uuid-0000-0000-000000000001");
  addFeature("motion",     "mo-uuid-0000-0000-000000000002");

  char buf[37] = {};
  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "online"));
  EXPECT_STREQ(buf, "on-uuid-0000-0000-000000000001");

  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "motion"));
  EXPECT_STREQ(buf, "mo-uuid-0000-0000-000000000002");
}

TEST_F(MainInoTest, GetFeatureUuidByNameReturnsFalseWhenNotFound) {
  addFeature("online", "on-uuid-0000-0000-000000000001");

  char buf[37] = {};
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "temperature"));
  EXPECT_STREQ(buf, "");
}

TEST_F(MainInoTest, GetFeatureUuidByNameReturnsFalseOnEmptyFeatures) {
  char buf[37] = {};
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "online"));
  EXPECT_STREQ(buf, "");
}

TEST_F(MainInoTest, GetFeatureUuidByNameSkipsEntriesWithNullFields) {
  JsonObject bad = saved_features.add<JsonObject>();
  bad["name"] = "online";
  // no "uuid" field → skipped

  addFeature("motion", "mo-uuid-0000-0000-000000000002");

  char buf[37] = {};
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "online"));
  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "motion"));
}

TEST_F(MainInoTest, GetFeatureUuidByNameTruncatesUuidToBufferSize) {
  addFeature("online", "on-uuid-0000-0000-000000000001");

  char buf[5] = {};
  get_feature_uuid_by_name(buf, sizeof(buf), "online");
  EXPECT_EQ(buf[sizeof(buf) - 1], '\0');
  EXPECT_EQ(strnlen(buf, sizeof(buf)), 4u);
}

// =============================================================================
// buildFeatures
// =============================================================================

TEST_F(MainInoTest, BuildFeaturesReturnsMotionAndOnlineOnly) {
  JsonDocument result = buildFeatures();
  JsonArray arr = result.as<JsonArray>();
  ASSERT_EQ(arr.size(), 2u);
  EXPECT_STREQ(arr[0]["name"].as<const char*>(), "motion");
  EXPECT_STREQ(arr[1]["name"].as<const char*>(), "online");
}

TEST_F(MainInoTest, BuildFeaturesHasCorrectFieldsAndBooleanSpecs) {
  JsonDocument result = buildFeatures();
  JsonArray arr = result.as<JsonArray>();
  for (size_t i = 0; i < arr.size(); ++i) {
    EXPECT_STREQ(arr[i]["type"].as<const char*>(), "sensor");
    EXPECT_STREQ(arr[i]["unit"].as<const char*>(), "-");
    EXPECT_TRUE(arr[i]["enable"].as<bool>());
    EXPECT_EQ(arr[i]["order"].as<int>(), i + 1);
    JsonObject spec = arr[i]["spec"].as<JsonObject>();
    EXPECT_STREQ(spec["format"].as<const char*>(), "bool");
    EXPECT_TRUE(spec["min"].isNull());
    EXPECT_TRUE(spec["max"].isNull());
    EXPECT_TRUE(spec["step"].isNull());
    EXPECT_TRUE(spec["list"].isNull());
  }
}

// =============================================================================
// read_and_send_motion_value
// =============================================================================

TEST_F(MainInoTest, ReadMotionPublishesWhenValueChangedAndFeatureFound) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("motion", "mo-uuid-0000-0000-000000000002");
  g_motion_prev_value = 0;
  g_motion_value      = 1;  // changed

  read_and_send_motion_value();

  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1u);
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 1u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "motion");
  EXPECT_FLOAT_EQ(FeatureValueCapture::instance().set_calls[0].value, 1.0f);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "motion");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[0].value, 1.0f);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].device_uuid, "device-uuid-test-0000-000000000000");
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].feature_uuid, "mo-uuid-0000-0000-000000000002");
  ASSERT_EQ(MqttAlarmCapture::instance().calls.size(), 1u);
  EXPECT_EQ(MqttAlarmCapture::instance().calls[0].feature_name, "motion");
  EXPECT_EQ(MqttAlarmCapture::instance().calls[0].alarm_type, "motion");
  EXPECT_FLOAT_EQ(MqttAlarmCapture::instance().calls[0].value, 1.0f);
}

TEST_F(MainInoTest, ReadMotionSkipsPublishWhenValueUnchanged) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("motion", "mo-uuid-0000-0000-000000000002");
  g_motion_prev_value = 1;
  g_motion_value      = 1;  // same

  read_and_send_motion_value();

  EXPECT_EQ(MqttNotifyCapture::instance().calls.size(), 0u);
  EXPECT_EQ(MqttAlarmCapture::instance().calls.size(), 0u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls.size(), 0u);
}

TEST_F(MainInoTest, ReadMotionPublishesFallingEdgeAsTelemetryWithoutAlarm) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("motion", "mo-uuid-0000-0000-000000000002");
  g_motion_prev_value = 1;
  g_motion_value = 0;

  read_and_send_motion_value();

  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1u);
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[0].value, 0.0f);
  EXPECT_EQ(MqttAlarmCapture::instance().calls.size(), 0u);
}

TEST_F(MainInoTest, ReadMotionSkipsPublishWhenFeatureNotFound) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  // No features registered → UUID lookup fails.
  g_motion_prev_value = 0;
  g_motion_value      = 1;  // changed, but no feature

  read_and_send_motion_value();

  EXPECT_EQ(MqttNotifyCapture::instance().calls.size(), 0u);
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 1u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "motion");
  EXPECT_FLOAT_EQ(FeatureValueCapture::instance().set_calls[0].value, 1.0f);
  EXPECT_EQ(MqttAlarmCapture::instance().calls.size(), 0u);
}

TEST_F(MainInoTest, MotionAlarmRunsEverySecond) {
  alarms_init();

  EXPECT_EQ(Alarm.period(alarm_motion), 1UL);
}

// =============================================================================
// send_online_status
// =============================================================================

TEST_F(MainInoTest, SendOnlineStatusPublishesAndRecordsFeatureValue) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("online", "on-uuid-0000-0000-000000000003");

  send_online_status();

  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1u);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "online");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[0].value, 1.0f);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].device_uuid, "device-uuid-test-0000-000000000000");
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].feature_uuid, "on-uuid-0000-0000-000000000003");

  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 1u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "online");
  EXPECT_FLOAT_EQ(FeatureValueCapture::instance().set_calls[0].value, 1.0f);
}
