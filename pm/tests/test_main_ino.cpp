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
#include "sen65_sensor.h"
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

static Sen65Reading sample{1, 2, 3, 4, 50, 23, 100, 5};
void sen65_init_sensor() {}
Sen65Reading sen65_read() { return sample; }

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


JsonDocument buildFeatures();
void read_and_send_sen65_values();
void send_online_status();
extern JsonArray saved_features;

class Sen65PublishTest : public ::testing::Test {
  void SetUp() override {
    saved_features.clear();
    JsonDocument features = buildFeatures();
    int i = 0;
    for (JsonObject f : features.as<JsonArray>()) {
      f["uuid"] = std::string("feature-") + std::to_string(i++);
      saved_features.add(f);
    }
    MqttNotifyCapture::reset();
    FeatureValueCapture::reset();
    sample = {1, 2, 3, 4, 50, 23, 100, 5};
  }
};

TEST_F(Sen65PublishTest, RegistersEightMeasurementsAndOnline) {
  ASSERT_EQ(saved_features.size(), 9);
  const char* names[] = {"pm1", "pm25", "pm4", "pm10", "temperature", "humidity", "vocIndex", "noxIndex", "online"};
  for (size_t i = 0; i < 9; ++i) {
    EXPECT_STREQ(saved_features[i]["name"].as<const char*>(), names[i]);
    EXPECT_EQ(saved_features[i]["order"].as<int>(), i + 1);
    EXPECT_TRUE(saved_features[i]["enable"].as<bool>());
  }
  EXPECT_STREQ(saved_features[8]["spec"]["format"].as<const char*>(), "bool");
}
TEST_F(Sen65PublishTest, RegistersDatasheetOutputRangesAndResolution) {
  const float minimums[] = {0, 0, 0, 0, -40, 0, 1, 1};
  const float maximums[] = {6000, 6000, 6000, 6000, 80, 100, 500, 500};
  const float steps[] = {0.1f, 0.1f, 0.1f, 0.1f, 0.005f, 0.01f, 0.1f, 0.1f};
  for (size_t i = 0; i < 8; ++i) {
    SCOPED_TRACE(saved_features[i]["name"].as<const char*>());
    JsonObject spec = saved_features[i]["spec"];
    EXPECT_FLOAT_EQ(spec["min"].as<float>(), minimums[i]);
    EXPECT_FLOAT_EQ(spec["max"].as<float>(), maximums[i]);
    EXPECT_FLOAT_EQ(spec["step"].as<float>(), steps[i]);
  }
}

TEST_F(Sen65PublishTest, PublishesPairedReadWithCorrectNamesAndValues) {
  read_and_send_sen65_values();
  const auto& calls = MqttNotifyCapture::instance().calls;
  ASSERT_EQ(calls.size(), 8);
  EXPECT_EQ(calls[4].type, "humidity");
  EXPECT_FLOAT_EQ(calls[4].value, 50);
  EXPECT_EQ(calls[5].type, "temperature");
  EXPECT_FLOAT_EQ(calls[5].value, 23);
  EXPECT_EQ(calls[6].type, "vocIndex");
  EXPECT_EQ(calls[7].type, "noxIndex");
}
TEST_F(Sen65PublishTest, SkipsNonfiniteFieldsAndMissingRegistration) {
  sample.pm1 = NAN;
  sample.voc_index = INFINITY;
  saved_features.remove(1);
  read_and_send_sen65_values();
  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 5);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls.size(), 6);
}
TEST_F(Sen65PublishTest, PublishesOnline) {
  send_online_status();
  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "online");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[0].value, 1);
}
