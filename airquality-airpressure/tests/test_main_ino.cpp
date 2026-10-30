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
#include "barometer_sensor.h"
#include "co2_sensor.h"
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

// --- display ----------------------------------------------------------------

void init_display(uint8_t) {}
void update_display() {}
void display_force_update() {}
void display_show_message(const char*, const char*) {}
void display_set_connectivity_status(bool, bool) {}
void display_sleep_for(unsigned long) {}

// --- barometer_sensor (controllable return values) --------------------------

static float g_barometer_altitude     = NAN;
static float g_barometer_pressure = NAN;
static float g_co2 = NAN;
static float g_iaq = NAN;
static float g_voc = NAN;
static float g_eco2 = NAN;
void barometer_update() {}
void co2_init_sensor() {}
float co2_read() { return g_co2; }
void barometer_init_sensor() {}
BarometerReading barometer_read() { return {g_barometer_pressure, g_barometer_altitude, g_iaq, g_voc, g_eco2}; }

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
void         read_and_send_barometer_value();
void read_and_send_co2_value();
void publish_initial_values();
void alarms_init();
void alarms_enable();
void alarms_disable();
extern AlarmID_t alarm_barometer;
extern AlarmID_t alarm_co2;
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
    MqttMockState::reset();
    FeatureValueCapture::reset();
    g_barometer_altitude     = NAN;
    g_barometer_pressure = NAN;
    g_iaq = NAN; g_voc = NAN; g_eco2 = NAN;
    g_co2 = NAN;
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

// =============================================================================
// get_feature_uuid_by_name
// =============================================================================

TEST_F(MainInoTest, GetFeatureUuidByNameFindsMatchingFeature) {
  addFeature("airpressure", "ap-uuid-0000-0000-000000000001");
  addFeature("altitude", "alt-uuid-0000-0000-000000000002");

  char buf[37] = {};
  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "airpressure"));
  EXPECT_STREQ(buf, "ap-uuid-0000-0000-000000000001");

  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "altitude"));
  EXPECT_STREQ(buf, "alt-uuid-0000-0000-000000000002");
}

TEST_F(MainInoTest, GetFeatureUuidByNameReturnsFalseWhenNotFound) {
  addFeature("airpressure", "ap-uuid-0000-0000-000000000001");

  char buf[37] = {};
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "humidity"));
  EXPECT_STREQ(buf, "");
}

TEST_F(MainInoTest, GetFeatureUuidByNameReturnsFalseOnEmptyFeatures) {
  char buf[37] = {};
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "airpressure"));
  EXPECT_STREQ(buf, "");
}

TEST_F(MainInoTest, GetFeatureUuidByNameSkipsEntriesWithNullFields) {
  JsonObject bad = saved_features.add<JsonObject>();
  bad["name"] = "airpressure";
  // no "uuid" field → skipped

  addFeature("altitude", "alt-uuid-0000-0000-000000000002");

  char buf[37] = {};
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "airpressure"));
  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "altitude"));
}

TEST_F(MainInoTest, GetFeatureUuidByNameTruncatesUuidToBufferSize) {
  addFeature("airpressure", "ap-uuid-0000-0000-000000000001");

  char buf[5] = {};
  get_feature_uuid_by_name(buf, sizeof(buf), "airpressure");
  EXPECT_EQ(buf[sizeof(buf) - 1], '\0');
  EXPECT_EQ(strnlen(buf, sizeof(buf)), 4u);
}

// =============================================================================
// buildFeatures
// =============================================================================

TEST_F(MainInoTest, BuildFeaturesReturnsSevenFeatures) {
  JsonDocument result = buildFeatures();
  JsonArray    arr    = result.as<JsonArray>();

  ASSERT_EQ(arr.size(), 7u);
  EXPECT_STREQ(arr[0]["name"].as<const char*>(), "co2");
  EXPECT_STREQ(arr[1]["name"].as<const char*>(), "airpressure");
  EXPECT_STREQ(arr[2]["name"].as<const char*>(), "altitude");
  EXPECT_STREQ(arr[3]["name"].as<const char*>(), "iaq");
  EXPECT_STREQ(arr[4]["name"].as<const char*>(), "voc");
  EXPECT_STREQ(arr[5]["name"].as<const char*>(), "eco2");
  EXPECT_STREQ(arr[6]["name"].as<const char*>(), "online");
}

TEST_F(MainInoTest, BuildFeaturesHasCorrectFieldsPerEntry) {
  JsonDocument result = buildFeatures();
  JsonArray    arr    = result.as<JsonArray>();

  // airpressure
  EXPECT_STREQ(arr[1]["type"].as<const char*>(), "sensor");
  EXPECT_STREQ(arr[1]["unit"].as<const char*>(), "hPa");
  EXPECT_TRUE(arr[1]["enable"].as<bool>());
  EXPECT_EQ(arr[1]["order"].as<int>(), 2);

  // altitude
  EXPECT_STREQ(arr[2]["type"].as<const char*>(), "sensor");
  EXPECT_STREQ(arr[2]["unit"].as<const char*>(), "m");
  EXPECT_TRUE(arr[2]["enable"].as<bool>());
  EXPECT_EQ(arr[2]["order"].as<int>(), 3);

  // online
  EXPECT_STREQ(arr[6]["type"].as<const char*>(), "sensor");
  EXPECT_STREQ(arr[6]["unit"].as<const char*>(), "-");
  EXPECT_TRUE(arr[6]["enable"].as<bool>());
  EXPECT_EQ(arr[6]["order"].as<int>(), 7);
}

TEST_F(MainInoTest, BuildFeaturesIncludesAdmissionSpecs) {
  JsonDocument result = buildFeatures();
  JsonArray    arr    = result.as<JsonArray>();

  JsonObject airpressureSpec = arr[1]["spec"].as<JsonObject>();
  ASSERT_FALSE(airpressureSpec.isNull());
  EXPECT_STREQ(airpressureSpec["format"].as<const char*>(), "float");
  EXPECT_FLOAT_EQ(airpressureSpec["min"].as<float>(), 300.0f);
  EXPECT_FLOAT_EQ(airpressureSpec["max"].as<float>(), 1100.0f);
  EXPECT_FLOAT_EQ(airpressureSpec["step"].as<float>(), 0.01f);
  EXPECT_TRUE(airpressureSpec["list"].isNull());

  JsonObject altitudeSpec = arr[2]["spec"].as<JsonObject>();
  ASSERT_FALSE(altitudeSpec.isNull());
  EXPECT_STREQ(altitudeSpec["format"].as<const char*>(), "float");
  EXPECT_FLOAT_EQ(altitudeSpec["min"].as<float>(), -1000.0f);
  EXPECT_FLOAT_EQ(altitudeSpec["max"].as<float>(), 10000.0f);
  EXPECT_FLOAT_EQ(altitudeSpec["step"].as<float>(), 0.5f);
  EXPECT_TRUE(altitudeSpec["list"].isNull());

  JsonObject onlineSpec = arr[6]["spec"].as<JsonObject>();
  ASSERT_FALSE(onlineSpec.isNull());
  EXPECT_STREQ(onlineSpec["format"].as<const char*>(), "bool");
  EXPECT_TRUE(onlineSpec["min"].isNull());
  EXPECT_TRUE(onlineSpec["max"].isNull());
  EXPECT_TRUE(onlineSpec["step"].isNull());
  EXPECT_TRUE(onlineSpec["list"].isNull());
}

// =============================================================================
// read_and_send_barometer_value
// =============================================================================

TEST_F(MainInoTest, ReadBarometerPublishesBothReadingsWhenValid) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("altitude", "alt-uuid-0000-0000-000000000002");
  addFeature("airpressure", "ap-uuid-0000-0000-000000000001");
  g_barometer_altitude     = 21.5f;
  g_barometer_pressure = 101.3f;

  read_and_send_barometer_value();

  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 2u);
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 2u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "altitude");
  EXPECT_FLOAT_EQ(FeatureValueCapture::instance().set_calls[0].value, 21.5f);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[1].name, "airpressure");
  EXPECT_FLOAT_EQ(FeatureValueCapture::instance().set_calls[1].value, 101.3f);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "altitude");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[0].value, 21.5f);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[1].type, "airpressure");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[1].value, 101.3f);
}

TEST_F(MainInoTest, ReadBarometerSkipsAltitudeWhenNan) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("altitude", "alt-uuid-0000-0000-000000000002");
  addFeature("airpressure", "ap-uuid-0000-0000-000000000001");
  g_barometer_altitude     = NAN;
  g_barometer_pressure = 101.3f;

  read_and_send_barometer_value();

  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1u);
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 1u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "airpressure");
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "airpressure");
}

TEST_F(MainInoTest, ReadBarometerSkipsAirpressureWhenNan) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("altitude", "alt-uuid-0000-0000-000000000002");
  addFeature("airpressure", "ap-uuid-0000-0000-000000000001");
  g_barometer_altitude     = 21.5f;
  g_barometer_pressure = NAN;

  read_and_send_barometer_value();

  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1u);
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 1u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "altitude");
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "altitude");
}

TEST_F(MainInoTest, ReadBarometerSkipsPublishWhenFeatureNotFound) {
  // No features → UUID lookup fails → nothing published.
  g_barometer_altitude     = 21.5f;
  g_barometer_pressure = 101.3f;

  read_and_send_barometer_value();

  EXPECT_EQ(MqttNotifyCapture::instance().calls.size(), 0u);
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 2u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "altitude");
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[1].name, "airpressure");
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

// =============================================================================
// publish_initial_values / loop MQTT reconnect path
// =============================================================================

TEST_F(MainInoTest, PublishInitialValuesSendsSensorValuesAndOnlineStatus) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("altitude", "alt-uuid-0000-0000-000000000002");
  addFeature("airpressure", "ap-uuid-0000-0000-000000000001");
  addFeature("online", "on-uuid-0000-0000-000000000003");
  g_barometer_altitude     = 21.5f;
  g_barometer_pressure = 101.3f;

  publish_initial_values();

  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 3u);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "altitude");
  EXPECT_EQ(MqttNotifyCapture::instance().calls[1].type, "airpressure");
  EXPECT_EQ(MqttNotifyCapture::instance().calls[2].type, "online");
}

TEST_F(MainInoTest, LoopPublishesInitialValuesImmediatelyAfterMqttConnect) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("altitude", "alt-uuid-0000-0000-000000000002");
  addFeature("airpressure", "ap-uuid-0000-0000-000000000001");
  addFeature("online", "on-uuid-0000-0000-000000000003");
  g_barometer_altitude     = 21.5f;
  g_barometer_pressure = 101.3f;
  MqttMockState::instance().connected_val = false;

  loop();

  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 3u);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "altitude");
  EXPECT_EQ(MqttNotifyCapture::instance().calls[1].type, "airpressure");
  EXPECT_EQ(MqttNotifyCapture::instance().calls[2].type, "online");
}

TEST_F(MainInoTest, Co2FeatureHasPpmIntegerRangeAndStableName) {
  JsonDocument doc = buildFeatures();
  auto spec = doc[0]["spec"].as<JsonObject>();
  EXPECT_STREQ(doc[0]["unit"], "ppm");
  EXPECT_EQ(doc[0]["order"].as<int>(), 1);
  EXPECT_STREQ(spec["format"], "int");
  EXPECT_EQ(spec["min"].as<int>(), 0);
  EXPECT_EQ(spec["max"].as<int>(), 40000);
  EXPECT_EQ(spec["step"].as<int>(), 1);
}

TEST_F(MainInoTest, PublishesCo2ThroughTheSameSignedNotifyPathAndCache) {
  addFeature("co2", "co2-feature");
  g_co2 = 850;
  read_and_send_co2_value();
  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1u);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "co2");
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].feature_uuid, "co2-feature");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[0].value, 850);
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 1u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "co2");
}

TEST_F(MainInoTest, MissingCo2OrRegistrationDoesNotPublish) {
  addFeature("co2", "co2-feature");
  read_and_send_co2_value();
  EXPECT_TRUE(MqttNotifyCapture::instance().calls.empty());
  EXPECT_TRUE(FeatureValueCapture::instance().set_calls.empty());
  saved_features.clear();
  g_co2 = 850;
  read_and_send_co2_value();
  EXPECT_TRUE(MqttNotifyCapture::instance().calls.empty());
}

TEST_F(MainInoTest, Co2FailureDoesNotSuppressPressureOrAltitude) {
  addFeature("airpressure", "pressure-feature");
  addFeature("altitude", "altitude-feature");
  g_barometer_pressure = 1013.25f;
  g_barometer_altitude = 0;
  read_and_send_co2_value();
  read_and_send_barometer_value();
  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 2u);
}

TEST_F(MainInoTest, AllThreeAlarmsStartDisabledAndCo2PollsEveryFiveSeconds) {
  alarms_init();
  EXPECT_EQ(Alarm.period(alarm_co2), 5UL);
  EXPECT_FALSE(Alarm.isEnabled(alarm_co2));
  EXPECT_FALSE(Alarm.isEnabled(alarm_barometer));
  EXPECT_FALSE(Alarm.isEnabled(alarm_online));
  alarms_enable();
  EXPECT_TRUE(Alarm.isEnabled(alarm_co2));
  alarms_disable();
  EXPECT_FALSE(Alarm.isEnabled(alarm_co2));
}

TEST_F(MainInoTest, InitialPublicationIncludesBothSensorsAndOnline) {
  addFeature("co2", "co2-feature");
  addFeature("airpressure", "pressure-feature");
  addFeature("altitude", "altitude-feature");
  addFeature("online", "online-feature");
  g_co2 = 850; g_barometer_pressure = 1013.25f; g_barometer_altitude = 0;
  publish_initial_values();
  EXPECT_EQ(MqttNotifyCapture::instance().calls.size(), 4u);
}

TEST_F(MainInoTest, AirQualityFeaturesHaveNumericSpecsAndSeparateCarbonDioxideIdentity) {
  JsonDocument doc = buildFeatures();
  EXPECT_STREQ(doc[0]["name"].as<const char*>(), "co2");
  for (size_t i = 3; i < 6; ++i) {
    EXPECT_STREQ(doc[i]["type"].as<const char*>(), "sensor");
    EXPECT_STREQ(doc[i]["spec"]["format"].as<const char*>(), "float");
    EXPECT_TRUE(doc[i]["enable"].as<bool>());
    EXPECT_EQ(doc[i]["order"].as<int>(), i + 1);
  }
  EXPECT_STREQ(doc[3]["unit"].as<const char*>(), "IAQ");
  EXPECT_FLOAT_EQ(doc[3]["spec"]["max"].as<float>(), 500);
  EXPECT_STREQ(doc[4]["unit"].as<const char*>(), "ppm");
  EXPECT_FLOAT_EQ(doc[4]["spec"]["step"].as<float>(), 0.001f);
  EXPECT_STREQ(doc[5]["unit"].as<const char*>(), "ppm");
}

TEST_F(MainInoTest, PublishesOnlyAvailableRegisteredAirQualityReadings) {
  addFeature("iaq", "iaq-feature");
  addFeature("voc", "voc-feature");
  addFeature("eco2", "eco2-feature");
  g_iaq = 75.5f; g_voc = 0.75f; g_eco2 = 650;
  read_and_send_barometer_value();
  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 3u);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "iaq");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[0].value, 75.5f);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[1].type, "voc");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[1].value, 0.75f);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[2].type, "eco2");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[2].value, 650);
  MqttNotifyCapture::reset();
  g_iaq = NAN; g_voc = NAN; g_eco2 = NAN;
  read_and_send_barometer_value();
  EXPECT_TRUE(MqttNotifyCapture::instance().calls.empty());
}
