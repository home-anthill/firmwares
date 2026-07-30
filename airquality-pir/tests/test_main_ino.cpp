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
#include "secrets.h"

// Headers from the firmware (needed for function signatures only).
#include "wifi_handler.h"
#include "mqtt_handler.h"
#include "registration.h"
#include "storage.h"
#include "airquality_sensor.h"
#include "pir_sensor.h"
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

// --- airquality_sensor (controllable return values) -------------------------

static bool g_airquality_has_newvalue = false;
static int  g_airquality_value        = 0;
void airquality_init_sensor()       {}
bool airquality_has_newvalue()      { return g_airquality_has_newvalue; }
int  airquality_get_value()         { return g_airquality_value; }

// --- pir_sensor (controllable return values) --------------------------------

static int g_pir_prev_value = -1;
static int g_pir_value      = 0;
void pir_init_sensor()       {}
int  pir_get_prev_value()    { return g_pir_prev_value; }
int  pir_get_value()         { return g_pir_value; }

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
  if (buf) { strncpy(buf, g_stored_uuid, 36); buf[36] = '\0'; }
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
void         read_and_send_airquality_value();
void         read_and_send_pir_value();
void         send_online_status();
void         alarms_init();

// Expose <main-project-arduino-file>.ino globals so the fixture can reset them.
extern JsonDocument doc_features;
extern JsonArray    saved_features;
extern char         saved_device_uuid[37];
extern AlarmID_t    alarm_pir;

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
    g_airquality_has_newvalue = false;
    g_airquality_value        = 0;
    g_pir_prev_value          = -1;
    g_pir_value               = 0;
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
  addFeature("airquality", "aq-uuid-0000-0000-000000000001");
  addFeature("motion",     "mo-uuid-0000-0000-000000000002");

  char buf[37] = {};
  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "airquality"));
  EXPECT_STREQ(buf, "aq-uuid-0000-0000-000000000001");

  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "motion"));
  EXPECT_STREQ(buf, "mo-uuid-0000-0000-000000000002");
}

TEST_F(MainInoTest, GetFeatureUuidByNameReturnsFalseWhenNotFound) {
  addFeature("airquality", "aq-uuid-0000-0000-000000000001");

  char buf[37] = {};
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "temperature"));
  EXPECT_STREQ(buf, "");
}

TEST_F(MainInoTest, GetFeatureUuidByNameReturnsFalseOnEmptyFeatures) {
  char buf[37] = {};
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "airquality"));
  EXPECT_STREQ(buf, "");
}

TEST_F(MainInoTest, GetFeatureUuidByNameSkipsEntriesWithNullFields) {
  JsonObject bad = saved_features.add<JsonObject>();
  bad["name"] = "airquality";
  // no "uuid" field → skipped

  addFeature("motion", "mo-uuid-0000-0000-000000000002");

  char buf[37] = {};
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "airquality"));
  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "motion"));
}

TEST_F(MainInoTest, GetFeatureUuidByNameTruncatesUuidToBufferSize) {
  addFeature("airquality", "aq-uuid-0000-0000-000000000001");

  char buf[5] = {};
  get_feature_uuid_by_name(buf, sizeof(buf), "airquality");
  EXPECT_EQ(buf[sizeof(buf) - 1], '\0');
  EXPECT_EQ(strnlen(buf, sizeof(buf)), 4u);
}

// =============================================================================
// buildFeatures
// =============================================================================

TEST_F(MainInoTest, BuildFeaturesReturnsThreeFeatures) {
  JsonDocument result = buildFeatures();
  JsonArray    arr    = result.as<JsonArray>();

  ASSERT_EQ(arr.size(), 3u);
  EXPECT_STREQ(arr[0]["name"].as<const char*>(), "airquality");
  EXPECT_STREQ(arr[1]["name"].as<const char*>(), "motion");
  EXPECT_STREQ(arr[2]["name"].as<const char*>(), "online");
}

TEST_F(MainInoTest, BuildFeaturesHasCorrectFieldsPerEntry) {
  JsonDocument result = buildFeatures();
  JsonArray    arr    = result.as<JsonArray>();

  // airquality
  EXPECT_STREQ(arr[0]["type"].as<const char*>(), "sensor");
  EXPECT_STREQ(arr[0]["unit"].as<const char*>(), "-");
  EXPECT_TRUE(arr[0]["enable"].as<bool>());
  EXPECT_EQ(arr[0]["order"].as<int>(), 1);

  // motion
  EXPECT_STREQ(arr[1]["type"].as<const char*>(), "sensor");
  EXPECT_STREQ(arr[1]["unit"].as<const char*>(), "-");
  EXPECT_TRUE(arr[1]["enable"].as<bool>());
  EXPECT_EQ(arr[1]["order"].as<int>(), 2);

  // online
  EXPECT_STREQ(arr[2]["type"].as<const char*>(), "sensor");
  EXPECT_STREQ(arr[2]["unit"].as<const char*>(), "-");
  EXPECT_TRUE(arr[2]["enable"].as<bool>());
  EXPECT_EQ(arr[2]["order"].as<int>(), 4);
}

TEST_F(MainInoTest, BuildFeaturesIncludesAdmissionSpecs) {
  JsonDocument result = buildFeatures();
  JsonArray    arr    = result.as<JsonArray>();

  JsonObject airqualitySpec = arr[0]["spec"].as<JsonObject>();
  ASSERT_FALSE(airqualitySpec.isNull());
  EXPECT_STREQ(airqualitySpec["format"].as<const char*>(), "int");
  EXPECT_FLOAT_EQ(airqualitySpec["min"].as<float>(), 0.0f);
  EXPECT_FLOAT_EQ(airqualitySpec["max"].as<float>(), 3.0f);
  EXPECT_FLOAT_EQ(airqualitySpec["step"].as<float>(), 1.0f);
  EXPECT_TRUE(airqualitySpec["list"].isNull());

  JsonObject motionSpec = arr[1]["spec"].as<JsonObject>();
  ASSERT_FALSE(motionSpec.isNull());
  EXPECT_STREQ(motionSpec["format"].as<const char*>(), "bool");
  EXPECT_TRUE(motionSpec["min"].isNull());
  EXPECT_TRUE(motionSpec["max"].isNull());
  EXPECT_TRUE(motionSpec["step"].isNull());
  EXPECT_TRUE(motionSpec["list"].isNull());

  JsonObject onlineSpec = arr[2]["spec"].as<JsonObject>();
  ASSERT_FALSE(onlineSpec.isNull());
  EXPECT_STREQ(onlineSpec["format"].as<const char*>(), "bool");
  EXPECT_TRUE(onlineSpec["min"].isNull());
  EXPECT_TRUE(onlineSpec["max"].isNull());
  EXPECT_TRUE(onlineSpec["step"].isNull());
  EXPECT_TRUE(onlineSpec["list"].isNull());
}

// =============================================================================
// read_and_send_airquality_value
// =============================================================================

TEST_F(MainInoTest, ReadAirqualityPublishesWhenHasNewValueAndFeatureFound) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("airquality", "aq-uuid-0000-0000-000000000001");
  g_airquality_has_newvalue = true;
  g_airquality_value        = 2;  // LOW_POLLUTION

  read_and_send_airquality_value();

  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1u);
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 1u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "airquality");
  EXPECT_FLOAT_EQ(FeatureValueCapture::instance().set_calls[0].value, 2.0f);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "airquality");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[0].value, 2.0f);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].device_uuid, "device-uuid-test-0000-000000000000");
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].feature_uuid, "aq-uuid-0000-0000-000000000001");
}

TEST_F(MainInoTest, ReadAirqualitySkipsPublishWhenNoNewValue) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("airquality", "aq-uuid-0000-0000-000000000001");
  g_airquality_has_newvalue = false;

  read_and_send_airquality_value();

  EXPECT_EQ(MqttNotifyCapture::instance().calls.size(), 0u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls.size(), 0u);
}

TEST_F(MainInoTest, ReadAirqualitySkipsPublishWhenFeatureNotFound) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  // No features registered → UUID lookup fails.
  g_airquality_has_newvalue = true;
  g_airquality_value        = 3;

  read_and_send_airquality_value();

  EXPECT_EQ(MqttNotifyCapture::instance().calls.size(), 0u);
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 1u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "airquality");
  EXPECT_FLOAT_EQ(FeatureValueCapture::instance().set_calls[0].value, 3.0f);
}

// =============================================================================
// read_and_send_pir_value
// =============================================================================

TEST_F(MainInoTest, ReadPirPublishesWhenValueChangedAndFeatureFound) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("motion", "mo-uuid-0000-0000-000000000002");
  g_pir_prev_value = 0;
  g_pir_value      = 1;  // changed

  read_and_send_pir_value();

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

TEST_F(MainInoTest, ReadPirSkipsPublishWhenValueUnchanged) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("motion", "mo-uuid-0000-0000-000000000002");
  g_pir_prev_value = 1;
  g_pir_value      = 1;  // same

  read_and_send_pir_value();

  EXPECT_EQ(MqttNotifyCapture::instance().calls.size(), 0u);
  EXPECT_EQ(MqttAlarmCapture::instance().calls.size(), 0u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls.size(), 0u);
}

TEST_F(MainInoTest, ReadPirPublishesFallingEdgeAsTelemetryWithoutAlarm) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("motion", "mo-uuid-0000-0000-000000000002");
  g_pir_prev_value = 1;
  g_pir_value = 0;

  read_and_send_pir_value();

  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1u);
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[0].value, 0.0f);
  EXPECT_EQ(MqttAlarmCapture::instance().calls.size(), 0u);
}

TEST_F(MainInoTest, ReadPirSkipsPublishWhenFeatureNotFound) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  // No features registered → UUID lookup fails.
  g_pir_prev_value = 0;
  g_pir_value      = 1;  // changed, but no feature

  read_and_send_pir_value();

  EXPECT_EQ(MqttNotifyCapture::instance().calls.size(), 0u);
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 1u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "motion");
  EXPECT_FLOAT_EQ(FeatureValueCapture::instance().set_calls[0].value, 1.0f);
  EXPECT_EQ(MqttAlarmCapture::instance().calls.size(), 0u);
}

TEST_F(MainInoTest, PirAlarmRunsEverySecond) {
  alarms_init();

  EXPECT_EQ(Alarm.period(alarm_pir), 1UL);
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
