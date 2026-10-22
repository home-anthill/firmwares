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
#include "temp_sensor.h"
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

static float sample = 23;
void temp_init_sensor() {}
float temp_get_temperature() { return sample; }

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
void read_and_send_temperature();
void send_online_status();
extern JsonArray saved_features;

class TemperaturePublishTest : public ::testing::Test {
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
    sample = 23;
  }
};

TEST_F(TemperaturePublishTest, RegistersTemperatureAndOnline) {
  ASSERT_EQ(saved_features.size(), 2);
  EXPECT_STREQ(saved_features[0]["name"].as<const char*>(), "temperature");
  EXPECT_STREQ(saved_features[1]["name"].as<const char*>(), "online");
  EXPECT_FLOAT_EQ(saved_features[0]["spec"]["min"].as<float>(), -200);
  EXPECT_FLOAT_EQ(saved_features[0]["spec"]["max"].as<float>(), 1372);
  EXPECT_FLOAT_EQ(saved_features[0]["spec"]["step"].as<float>(), 0.0625);
}
TEST_F(TemperaturePublishTest, PublishesTemperature) {
  read_and_send_temperature();
  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "temperature");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[0].value, 23);
}
TEST_F(TemperaturePublishTest, SkipsInvalidAndUnregisteredTemperature) {
  sample = NAN;
  read_and_send_temperature();
  sample = INFINITY;
  read_and_send_temperature();
  sample = 23;
  saved_features.remove(0);
  read_and_send_temperature();
  EXPECT_TRUE(MqttNotifyCapture::instance().calls.empty());
}
TEST_F(TemperaturePublishTest, PublishesOnline) {
  send_online_status();
  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "online");
}
