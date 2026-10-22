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
#include "ph_sensor.h"
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

// --- display ----------------------------------------------------------------

void init_display(uint8_t) {}
void update_display() {}
void display_force_update() {}
void display_show_message(const char*, const char*) {}
void display_set_connectivity_status(bool, bool) {}
void display_sleep_for(unsigned long) {}

// --- pH module --------------------------------------------------------------
static PhReading g_ph_reading = {NAN, NAN, false, false, false};
void ph_init_sensor() {}
void ph_sensor_update() {}
PhReading ph_read() { return g_ph_reading; }

// --- feature_values ---------------------------------------------------------

struct FeatureValueSetCall {
  std::string name;
  float value;
};

struct FeatureValueCapture {
  std::vector<FeatureValueSetCall> set_calls;
  int init_count{0};

  static FeatureValueCapture& instance() {
    static FeatureValueCapture s;
    return s;
  }
  static void reset() { instance() = FeatureValueCapture{}; }
};

void feature_values_init(JsonArray /*features*/) {
  FeatureValueCapture::instance().init_count++;
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
void         read_and_send_ph_value();
void publish_initial_values();
void alarms_init();
void alarms_enable();
void alarms_disable();
extern AlarmID_t alarm_ph;
extern AlarmID_t alarm_online;
void         send_online_status();

// Expose <main-project-arduino-file>.ino globals so the fixture can reset them between tests.
extern JsonDocument doc_features;
extern JsonArray    saved_features;
extern char         saved_device_uuid[37];

// =============================================================================
// Fixture — resets all shared state before every test.
// =============================================================================

class MainInoTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Clear the feature array stored in the .ino globals.
    doc_features.clear();
    saved_features = doc_features.to<JsonArray>();
    memset(saved_device_uuid, 0, sizeof(saved_device_uuid));

    // Reset stubs.
    MqttNotifyCapture::reset();
    FeatureValueCapture::reset();
    g_ph_reading = {NAN, NAN, false, false, false};
    Alarm.reset();
    g_stored_uuid_len = 0;
    memset(g_stored_uuid, 0, sizeof(g_stored_uuid));
  }

  // Helper: add a feature entry to saved_features.
  void addFeature(const char* name, const char* uuid) {
    JsonObject f = saved_features.add<JsonObject>();
    f["name"] = name;
    f["uuid"] = uuid;
  }
};

TEST_F(MainInoTest, RegistersPhAndOnlineWithNumericSpec) {
  JsonDocument doc = buildFeatures();
  JsonArray features = doc.as<JsonArray>();
  ASSERT_EQ(features.size(), 2u);
  EXPECT_STREQ(features[0]["name"], "ph");
  EXPECT_STREQ(features[0]["unit"], "pH");
  EXPECT_STREQ(features[0]["spec"]["format"], "float");
  EXPECT_FLOAT_EQ(features[0]["spec"]["min"], 0);
  EXPECT_FLOAT_EQ(features[0]["spec"]["max"], 14);
  EXPECT_FLOAT_EQ(features[0]["spec"]["step"], 0.01f);
  EXPECT_STREQ(features[1]["name"], "online");
}

TEST_F(MainInoTest, PublishesValidCalibratedPhAndUpdatesDisplayCache) {
  addFeature("ph", "ph-feature");
  g_ph_reading = {0.1f, 5.5f, true, true, false};
  read_and_send_ph_value();
  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1u);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "ph");
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].feature_uuid, "ph-feature");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[0].value, 5.5f);
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 1u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "ph");
}

TEST_F(MainInoTest, SkipsInvalidOrUncalibratedMeasurements) {
  addFeature("ph", "ph-feature");
  for (float value : {NAN, INFINITY}) {
    g_ph_reading.ph = value;
    read_and_send_ph_value();
  }
  EXPECT_TRUE(MqttNotifyCapture::instance().calls.empty());
  EXPECT_TRUE(FeatureValueCapture::instance().set_calls.empty());
}

TEST_F(MainInoTest, DoesNotPublishWithoutRegisteredFeatureIdentity) {
  g_ph_reading = {0.1f, 5.5f, true, true, false};
  read_and_send_ph_value();
  EXPECT_TRUE(MqttNotifyCapture::instance().calls.empty());
}

TEST_F(MainInoTest, InitialPublicationIncludesPhAndOnline) {
  addFeature("ph", "ph-feature");
  addFeature("online", "online-feature");
  g_ph_reading = {0.1f, 5.5f, true, true, false};
  publish_initial_values();
  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 2u);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "ph");
  EXPECT_EQ(MqttNotifyCapture::instance().calls[1].type, "online");
}

TEST_F(MainInoTest, SensorAlarmsStartDisabledAndCanBeReenabled) {
  alarms_init();
  EXPECT_FALSE(Alarm.isEnabled(alarm_ph));
  EXPECT_FALSE(Alarm.isEnabled(alarm_online));
  alarms_enable();
  EXPECT_TRUE(Alarm.isEnabled(alarm_ph));
  EXPECT_TRUE(Alarm.isEnabled(alarm_online));
  alarms_disable();
  EXPECT_FALSE(Alarm.isEnabled(alarm_ph));
  EXPECT_FALSE(Alarm.isEnabled(alarm_online));
}
